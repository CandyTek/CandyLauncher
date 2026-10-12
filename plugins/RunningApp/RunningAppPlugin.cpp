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

// 任务队列系统
inline std::queue<std::function<void()>> g_taskQueue;
inline std::mutex g_taskQueueMutex;
inline std::condition_variable g_taskQueueCV;
// 用于插件索引系统正在运行的窗口
inline std::thread g_pluginIndexedRunningAppsThread;
inline std::atomic<bool> g_workerShouldStop{false};

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

// 工作线程函数
inline void WorkerThreadFunction()
{
	// 1. 初始化 COM
	HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	while (!g_workerShouldStop)
	{
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(g_taskQueueMutex);
			g_taskQueueCV.wait(lock, []() {
				return !g_taskQueue.empty() || g_workerShouldStop.load();
			});

			if (g_workerShouldStop) break;

			if (!g_taskQueue.empty())
			{
				task = std::move(g_taskQueue.front());
				g_taskQueue.pop();
			}
		}

		if (task)
		{
			// 调用无 C++ 对象展开的包装函数
			ExecuteTaskWithSEH(task);
		}
	}

	if (SUCCEEDED(hr))
	{
		::CoUninitialize();
	}
}

// 停止工作线程
inline void stopThreadPluginRunningApps()
{
	// 1. 标记退出
	g_workerShouldStop = true;

	// 2. 清空队列中尚未执行的任务，防止执行悬空指针
	{
		std::lock_guard<std::mutex> lock(g_taskQueueMutex);
		std::queue<std::function<void()>> emptyQueue;
		std::swap(g_taskQueue, emptyQueue);
	}

	// 3. 唤醒并等待线程结束
	g_taskQueueCV.notify_all();
	if (g_pluginIndexedRunningAppsThread.joinable())
	{
		g_pluginIndexedRunningAppsThread.join();
	}
}

// 向工作线程提交任务
inline void SubmitTask(std::function<void()> task)
{
	{
		std::lock_guard<std::mutex> lock(g_taskQueueMutex);
		g_taskQueue.push(std::move(task));
	}
	g_taskQueueCV.notify_one();
}

class RunningAppPlugin : public candy::Plugin
{
private:
	candy::ActionSet<RunningAppAction> windows{0};
	std::atomic<bool> indexEdgeTabs{false};

	// 本程序自身的 exe 路径，用于排除自身窗口
	std::wstring selfExePath;

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

public:
	bool Initialize() override
	{
		m_host = &GetHost();
		selfExePath = GetExecutablePath();
		g_pluginIndexedRunningAppsThread = std::thread(WorkerThreadFunction);
		return true;
	}

	void Shutdown() override
	{
		stopThreadPluginRunningApps();
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
			SubmitTask([this]()
			{
				refreshRunningApps();
			});
		}
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

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override
	{
		const auto runningAppAction = windows.Find(userData);
		if (!runningAppAction) return false;

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
			break;
		case IDM_OPEN_IN_CONSOLE:
			OpenConsoleHere(SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
			break;
		case IDM_KILL_PROCESS:
			KillProcessByImagePath(SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
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
