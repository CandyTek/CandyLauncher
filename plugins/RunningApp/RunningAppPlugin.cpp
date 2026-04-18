#include "../Plugin.hpp"
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

#include "util/BitmapUtil.hpp"
#include "util/FileSystemTraverser.hpp"
#include "util/FileUtil.hpp"
#include "util/MainTools.hpp"
#include "util/RunningWindowsTraverser.hpp"


// 任务队列系统
inline std::queue<std::function<void()>> g_taskQueue;
inline std::mutex g_taskQueueMutex;
inline std::condition_variable g_taskQueueCV;
// 用于插件索引系统正在运行的窗口
inline std::thread g_pluginIndexedRunningAppsThread;
inline std::atomic<bool> g_workerShouldStop{false};

// 工作线程函数
inline void WorkerThreadFunction()
{
	while (!g_workerShouldStop)
	{
		std::unique_lock<std::mutex> lock(g_taskQueueMutex);
		g_taskQueueCV.wait(lock, []() { return !g_taskQueue.empty() || g_workerShouldStop; });

		if (g_workerShouldStop) break;

		if (!g_taskQueue.empty())
		{
			auto task = g_taskQueue.front();
			g_taskQueue.pop();
			lock.unlock();

			try
			{
				task();
			}
			catch (...)
			{
				// 捕获任务执行中的异常，防止线程崩溃
			}
		}
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


inline void stopThreadPluginRunningApps()
{
	if (g_pluginIndexedRunningAppsThread.joinable())
	{
		g_workerShouldStop = true;
		g_taskQueueCV.notify_all();
		g_pluginIndexedRunningAppsThread.join();
	}
}


class RunningAppPlugin : public IPlugin
{
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;

public:
	RunningAppPlugin() = default;
	~RunningAppPlugin() override = default;

	std::wstring GetPluginName() const override
	{
		return L"运行中的窗口";
	}
	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.runningapp";
	}

	std::wstring GetPluginVersion() const override
	{
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override
	{
		return L"正在运行的应用";
	}


	bool Initialize(IPluginHost* host) override
	{
		m_host = host;
		g_pluginIndexedRunningAppsThread = std::thread(WorkerThreadFunction);
		return m_host != nullptr;
	}
	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}


	void Shutdown() override
	{
		if (m_host)
		{
			stopThreadPluginRunningApps();
		}
		m_host = nullptr;
	}
	


	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override
	{
		if (!m_host) return {};
		return allPluginActions;
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


	void refreshRunningApps()
	{
		allPluginActions.clear();
		// 遍历运行中的窗口并添加到列表
		::TraverseRunningWindows([&](const std::wstring &name,
									 const std::wstring &fullPath,
									 const std::wstring &hwnd,
									 const std::wstring &command) {
			auto action = std::make_shared<RunningAppAction>();
			action->title = L"正在运行: " + name;
			action->subTitle = fullPath;
			action->filePath = fullPath;
			action->runningAppHwnd = hwnd;
			action->iconFilePathIndex = GetSysImageIndex(fullPath);
			action->matchText = m_host->GetTheProcessedMatchingText(name) + GetFileNameFromPath(fullPath);
			allPluginActions.push_back(action);
		});

	}


	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override
	{
		if (!m_host) return false;
		auto runningAppAction = std::dynamic_pointer_cast<RunningAppAction>(action);
		if (!runningAppAction) return false;

		runningAppAction->Invoke();
		return true;
	}

	
	bool OnItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) override {
		ConsolePrintln(L"FolderPlugin", L"OnItemRightClick entered");
		auto runningAppAction = std::dynamic_pointer_cast<RunningAppAction>(action);
		if (!runningAppAction) {
			ConsolePrintln(L"FolderPlugin", L"OnItemRightClick skipped: action is not RunningAppAction");
			return false;
		}
		ConsolePrintln(L"FolderPlugin", L"ShowShellContextMenu path=" + runningAppAction->getFilePath());
		ShowShellContextMenu(parentHwnd, runningAppAction->getFilePath(), screenPt);
		ConsolePrintln(L"FolderPlugin", L"ShowShellContextMenu returned");
		return true;
	}

	
	bool OnItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) override {
		ConsolePrintln(L"FolderPlugin", L"OnItemShiftRightClick entered");
		auto runningAppAction = std::dynamic_pointer_cast<RunningAppAction>(action);
		if (!runningAppAction) {
			ConsolePrintln(L"FolderPlugin", L"OnItemShiftRightClick skipped: action is not RunningAppAction");
			return false;
		}
		ConsolePrintln(L"FolderPlugin", L"ShowMyContextMenu path=" + runningAppAction->getFilePath());
		const UINT cmd = ShowMyContextMenu(parentHwnd, runningAppAction->getFilePath(), screenPt);
		ConsolePrintln(L"FolderPlugin", L"ShowMyContextMenu cmd=" + std::to_wstring(cmd));
		switch (cmd) {
		case IDM_OPEN_APP:
			// runningAppAction->Invoke(runningAppAction->getFilePath());
			break;
		case IDM_OPEN_IN_CONSOLE:
			OpenConsoleHere(SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
			break;
		case IDM_KILL_PROCESS:
			KillProcessByImagePath(SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
			break;
		case IDM_COPY_PATH:
			CopyTextToClipboard(parentHwnd, runningAppAction->getFilePath());
			break;
		// case IDM_COPY_TARGET_PATH:
		// 	CopyTextToClipboard(parentHwnd, SaveGetShortcutTargetAndReturn(runningAppAction->getFilePath()));
		// 	break;
		default:
			break;
		}
		return true;
	}


};

PLUGIN_EXPORT IPlugin* CreatePlugin()
{
	return new RunningAppPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin)
{
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion()
{
	return 1;
}
