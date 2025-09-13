#pragma once

#include "Constants.hpp"
#include "DataKeeper.hpp"
#include "MainTools.hpp"
#include "ListViewManager.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>


// 任务队列系统
inline std::queue<std::function<void()>> g_taskQueue;
inline std::mutex g_taskQueueMutex;
inline std::condition_variable g_taskQueueCV;
// 用于插件索引系统正在运行的窗口
inline std::thread g_pluginIndexedRunningAppsThread;
inline std::atomic<bool> g_workerShouldStop{false};

// 工作线程函数
inline void WorkerThreadFunction() {
	while (!g_workerShouldStop) {
		std::unique_lock<std::mutex> lock(g_taskQueueMutex);
		g_taskQueueCV.wait(lock, []() { return !g_taskQueue.empty() || g_workerShouldStop; });

		if (g_workerShouldStop) break;

		if (!g_taskQueue.empty()) {
			auto task = g_taskQueue.front();
			g_taskQueue.pop();
			lock.unlock();

			try {
				task();
			} catch (...) {
				// 捕获任务执行中的异常，防止线程崩溃
			}
		}
	}
}

// 向工作线程提交任务
inline void SubmitTask(std::function<void()> task) {
	{
		std::lock_guard<std::mutex> lock(g_taskQueueMutex);
		g_taskQueue.push(std::move(task));
	}
	g_taskQueueCV.notify_one();
}



inline void stopThreadPluginRunningApps() {
	if (g_pluginIndexedRunningAppsThread.joinable()) {
		g_workerShouldStop = true;
		g_taskQueueCV.notify_all();
		g_pluginIndexedRunningAppsThread.join();
	}
}
inline void UserShowMainWindowSimple() {
	ShowMainWindowSimple();
	if (g_settings_map["pref_indexed_running_app"].boolValue) {
		// 向子线程提交下列任务，注意可能会有: 查询、清除，添加冲突问题
		SubmitTask([]() {
			// 先清除旧的运行应用列表
			runningAppActions.clear();

			// 遍历运行中的窗口并添加到列表
			::TraverseRunningWindows([&](const std::wstring &name,
										 const std::wstring &fullPath,
										 const std::wstring &hwnd,
										 const std::wstring &command) {
				const auto action = std::make_shared<RunCommandAction>(
						name, fullPath, hwnd, RUNNING_APP
				);
				action->iImageIndex = GetSysImageIndex(action->GetTargetPath());
				runningAppActions.push_back(action);
			});
		});
		
	}

}

