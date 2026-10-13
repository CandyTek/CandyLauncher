#include <windows.h>
#include <memory>

#include "RunningAppAction.hpp"
#include "RunningAppPluginData.hpp"
#include "ContextMenuHelper.hpp"


#include "../../util/MainTools.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_set>

#include "util/BitmapUtil.hpp"
#include "util/FileSystemTraverser.hpp"
#include "util/FileUtil.hpp"
#include "util/ImmersiveAppViewTraverser.hpp"
#include "util/MainTools.hpp"
#include "util/RunningWindowsTraverser.hpp"
#include <objbase.h>

#include "util/ClipboardUtil.hpp"

// 独立的 SEH 执行包装函数，该函数内部不包含任何需要析构的 C++ 局部对象
inline void ExecuteTaskWithSEH(const std::function<void()>& task)
{
	__try
	{
		task();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Loge(L"RunningApp", L"工作线程发生底层 SEH 异常");
	}
}

// 索引工作线程：串行执行提交的任务，线程上初始化 STA COM。
// 线程归插件实例所有，而不是 DLL 全局对象：进程经 ExitProcess 退出时不会调用 Shutdown，
// 全局 std::thread 在 DLL 卸载时仍 joinable 会触发 std::terminate → abort。
class RunningAppWorker
{
public:
	~RunningAppWorker()
	{
		Stop();
	}

	// onThreadExit 在线程退出前、CoUninitialize 之前执行，用于释放属于该线程 STA 的 COM 指针
	bool Start(std::function<void()> onThreadExit)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_thread.joinable()) return true;
		m_shouldStop = false;
		m_onThreadExit = std::move(onThreadExit);
		try
		{
			m_thread = std::thread([this] { Run(); });
		}
		catch (const std::exception& e)
		{
			Loge(L"RunningApp", L"Failed to start worker thread: ", e.what());
			return false;
		}
		return true;
	}

	// 丢弃尚未执行的任务并等待线程结束，可重复调用
	void Stop()
	{
		std::thread thread;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_shouldStop = true;
			std::queue<std::function<void()>> empty;
			std::swap(m_queue, empty);
			thread = std::move(m_thread);
		}
		m_cv.notify_all();
		if (!thread.joinable()) return;
		// 在工作线程自身上调用时不能 join 自己
		if (thread.get_id() == std::this_thread::get_id()) thread.detach();
		else thread.join();
	}

	// 返回 false 表示线程未运行，任务没有入队
	bool Submit(std::function<void()> task)
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_thread.joinable() || m_shouldStop) return false;
			m_queue.push(std::move(task));
		}
		m_cv.notify_one();
		return true;
	}

private:
	void Run()
	{
		const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		for (;;)
		{
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(lock, [this] { return m_shouldStop || !m_queue.empty(); });
				if (m_shouldStop) break;
				task = std::move(m_queue.front());
				m_queue.pop();
			}
			if (task) ExecuteTaskWithSEH(task);
		}

		if (m_onThreadExit) ExecuteTaskWithSEH(m_onThreadExit);
		if (SUCCEEDED(hr)) ::CoUninitialize();
	}

	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::queue<std::function<void()>> m_queue;
	std::thread m_thread;
	bool m_shouldStop = false;
	std::function<void()> m_onThreadExit;
};

// 结束拥有该窗口的进程；句柄无效或无权限时返回 false
static bool KillProcessOfWindow(HWND hwnd)
{
	DWORD pid = 0;
	if (!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd, &pid) || pid == 0) return false;
	if (pid == GetCurrentProcessId()) return false;
	const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (!process) return false;
	const bool ok = TerminateProcess(process, 1) != FALSE;
	CloseHandle(process);
	return ok;
}

class RunningAppPlugin : public candy::Plugin
{
private:
	candy::ActionSet<RunningAppAction> windows{0};
	std::atomic<bool> indexEdgeTabs{false};
	// 已有一次刷新在排队时不再重复提交，窗口连续显示只触发一次索引
	std::atomic<bool> refreshQueued{false};

	// 本程序自身的 exe 路径，用于排除自身窗口
	std::wstring selfExePath;
	RunningAppWorker worker;

	void appendNormalWindows(std::vector<std::shared_ptr<RunningAppAction>>& items,
							std::unordered_set<std::wstring>& seenKeys)
	{
		::TraverseRunningWindows([&](const std::wstring &name,
									 const std::wstring &fullPath,
									 const std::wstring &hwnd,
									 const std::wstring &command) {
			// 排除本程序自身的窗口
			if (!selfExePath.empty() && _wcsicmp(fullPath.c_str(), selfExePath.c_str()) == 0) {
				return;
			}

			const std::wstring dedupeKey = L"hwnd:" + hwnd;
			if (!seenKeys.insert(dedupeKey).second) {
				return;
			}

			auto action = std::make_shared<RunningAppAction>();
			action->title = L"正在运行: " + name;
			action->subtitle = fullPath;
			action->filePath = fullPath;
			action->runningAppHwnd = hwnd;
			action->stableKey = dedupeKey;
			action->iconPath = fullPath;
			action->matchTextVerbatim = true;
			action->matchText = GetHost().ProcessMatchText(name) + GetFileNameFromPath(fullPath);
			items.push_back(std::move(action));
		});
	}

	void appendEdgeTabs(std::vector<std::shared_ptr<RunningAppAction>>& items,
						std::unordered_set<std::wstring>& seenKeys)
	{
		TraverseImmersiveApplicationViews([&](const ImmersiveAppViewInfo& viewInfo) {
			if (!IsEdgeProcessPath(viewInfo.processPath)) {
				return;
			}
			if (viewInfo.title.empty()) {
				return;
			}

			const std::wstring dedupeKey = L"edge:"
				+ viewInfo.title + L"|"
				+ std::to_wstring(reinterpret_cast<uintptr_t>(viewInfo.thumbnailHwnd));
			if (!seenKeys.insert(dedupeKey).second) {
				return;
			}

			auto action = std::make_shared<RunningAppAction>();
			action->title = L"Edge 标签页: " + viewInfo.title;
			action->subtitle = viewInfo.processPath;
			action->filePath = viewInfo.processPath;
			action->runningAppHwnd = std::to_wstring(reinterpret_cast<uintptr_t>(viewInfo.thumbnailHwnd));
			action->stableKey = dedupeKey;
			action->iconPath = viewInfo.processPath;
			action->matchTextVerbatim = true;
			action->matchText = GetHost().ProcessMatchText(viewInfo.title)
				+ GetFileNameFromPath(viewInfo.processPath)
				+ GetHost().ProcessMatchText(viewInfo.appUserModelId);
			action->activateType = RunningAppAction::ActivateType::ApplicationView;
			action->applicationView = viewInfo.view;
			action->isModernApplicationView = viewInfo.isModernView;

			Logi(L"RunningApp", L"索引: ", action->title);
			items.push_back(std::move(action));
		});
	}

	// 在工作线程上运行：发布集合后通知主程序刷新当前列表
	void refreshRunningApps()
	{
		std::vector<std::shared_ptr<RunningAppAction>> items;
		std::unordered_set<std::wstring> seenKeys;
		appendNormalWindows(items, seenKeys);
		if (indexEdgeTabs) {
			appendEdgeTabs(items, seenKeys);
		}
		windows.Publish(GetHost(), std::move(items));
		GetHost().NotifyActionsChanged();
	}

	void requestRefresh()
	{
		if (refreshQueued.exchange(true)) return;
		const bool queued = worker.Submit([this]() {
			refreshQueued = false;
			refreshRunningApps();
		});
		if (!queued) {
			// 工作线程不可用：退化为在当前线程同步刷新
			refreshQueued = false;
			refreshRunningApps();
		}
	}

public:
	bool Initialize() override
	{
		m_host = &GetHost();
		selfExePath = GetExecutablePath();
		// 线程退出前在工作线程上释放 ApplicationView 的 COM 指针，它们属于该线程的 STA
		if (!worker.Start([this]() { windows.ClearLocal(); })) {
			Logw(L"RunningApp", L"Worker thread unavailable, refreshing synchronously");
		}
		return true;
	}

	void Shutdown() override
	{
		worker.Stop();
		windows.ClearLocal();
		m_host = nullptr;
	}

	void OnUserSettingsLoaded() override
	{
		indexEdgeTabs = GetHost().GetBool(L"com.candytek.runningapp.index_edge_tabs", false);
	}

	void OnMainWindowShow(bool isShow) override
	{
		if (isShow)
		{
			requestRefresh();
		}
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override
	{
		const auto runningAppAction = windows.Find(userData);
		if (!runningAppAction) return false;

		if (runningAppAction->activateType == RunningAppAction::ActivateType::ApplicationView) {
			// IApplicationView 代理属于工作线程的 STA，从主线程直接调用会返回 RPC_E_WRONG_THREAD
			if (worker.Submit([runningAppAction]() { runningAppAction->Invoke(); })) return true;
		}
		runningAppAction->Invoke();
		return true;
	}

	bool OnItemRightClick(const uint64_t userData, HWND parentHwnd, POINT screenPt) override {
		const auto runningAppAction = windows.Find(userData);
		if (!runningAppAction) {
			Logw(L"RunningApp", L"OnItemRightClick skipped: action not found");
			return false;
		}
		Logi(L"RunningApp", L"ShowShellContextMenu path=", runningAppAction->getFilePath());
		ShowShellContextMenu(parentHwnd, runningAppAction->getFilePath(), screenPt);
		return true;
	}

	bool OnItemShiftRightClick(const uint64_t userData, HWND parentHwnd, POINT screenPt) override {
		const auto runningAppAction = windows.Find(userData);
		if (!runningAppAction) {
			Logw(L"RunningApp", L"OnItemShiftRightClick skipped: action not found");
			return false;
		}
		const UINT cmd = ShowMyContextMenu(parentHwnd, runningAppAction->getFilePath(), screenPt);
		Logi(L"RunningApp", L"ShowMyContextMenu cmd=", cmd);
		switch (cmd) {
		case IDM_OPEN_APP:
			{
				// 启动该程序的新实例（回车是激活已有窗口）
				const std::wstring target = SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath());
				if (target.empty()) break;
				ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				break;
			}
		case IDM_OPEN_IN_CONSOLE:
			OpenConsoleHere(SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
			break;
		case IDM_KILL_PROCESS:
			// 优先只结束拥有该窗口的进程，避免杀掉同一程序的其他实例；句柄失效时退回按镜像路径结束
			if (!KillProcessOfWindow(runningAppAction->GetHwnd())) {
				KillProcessByImagePath(SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
			}
			break;
		case IDM_COPY_PATH:
			GetHost().SetClipboardText(runningAppAction->getFilePath());
			break;
		default:
			break;
		}
		return true;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto runningAppAction = windows.Find(userData);
		if (!runningAppAction || runningAppAction->filePath.empty()) return false;
		text = runningAppAction->filePath;
		return true;
	}
};

CANDY_PLUGIN_EXPORT(RunningAppPlugin, L"Running Windows", L"Windows of running applications", L"1.1.0", L"com.candytek.runningapp",
					nullptr)
