#pragma once
#include "../Plugin.h"
#include <windows.h>
#include <memory>


#include "../../util/MainTools.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "FileAction.hpp"
#include "Utils.hpp"
#include "util/TraverseFilesHelper.hpp"


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
		g_taskQueueCV.wait(lock, []() {
			return !g_taskQueue.empty() || g_workerShouldStop;
		});

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


class FolderPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<ActionBase>> allActions;

public:
	FolderPlugin() = default;
	~FolderPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"文件夹";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.folderplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"文件夹";
	}


	bool Initialize(IPluginHost* host) override {
		g_host = host;
		g_pluginIndexedRunningAppsThread = std::thread(WorkerThreadFunction);
		return g_host != nullptr;
	}


	void Shutdown() override {
		if (g_host) {
			stopThreadPluginRunningApps();
		}
		g_host = nullptr;
	}


	std::vector<std::shared_ptr<ActionBase>> GetAllActions() override {
		if (!g_host) return {};
		return allActions;
	}

	void RefreshAllActions() override {
		allActions.clear();
		// 遍历运行中的窗口并添加到列表
		bool isPathAdded = false;

		std::vector<TraverseOptions> runnerConfigs = ParseRunnerConfig();
		for (TraverseOptions traverseOptions1 : runnerConfigs) {
			if (traverseOptions1.type.empty()) {
				if (g_host->GetSettingsMap().at("pref_use_everything_sdk_index").boolValue) {
					g_host->TraverseFilesForEverythingSDK(traverseOptions1.folder, traverseOptions1, [&](const std::wstring& name,
													const std::wstring& fullPath,
													const std::wstring& parent,
													const std::wstring& ext) {
														const auto action = std::make_shared<FileAction>(
															name, fullPath, false,  parent
														);
														action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
														allActions.push_back(action);
													});
				} else {
					::TraverseFiles(traverseOptions1.folder, traverseOptions1, [&](const std::wstring& name,
																					const std::wstring& fullPath,
																					const std::wstring& parent,
																					const std::wstring& ext) {
						const auto action = std::make_shared<FileAction>(
							name, fullPath, false,  parent
						);
						action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
						allActions.push_back(action);
					});
				}
			} else if (traverseOptions1.type == L"path" && !isPathAdded &&
				g_host->GetSettingsMap().at("pref_indexed_envpath_apps").boolValue) {
				TraversePATHExecutables2([&](const std::wstring& name,
																					const std::wstring& fullPath,
																					const std::wstring& parent,
																					const std::wstring& ext) {
						const auto action = std::make_shared<FileAction>(
							name, fullPath, false, parent
						);
					action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());

						allActions.push_back(action);
					}, traverseOptions1);
				
				isPathAdded = true;
			}
		}


		TraverseOptions emptyTraverseOptions;
		TraverseOptions defaultOptions = CreateDefaultPathTraverseOptions();
		if (!isPathAdded && g_host->GetSettingsMap().at("pref_indexed_envpath_apps").boolValue) {
			// TODO: 解决这个崩溃问题
			TraversePATHExecutables2([&](const std::wstring& name,
																					const std::wstring& fullPath,
																					const std::wstring& parent,
																					const std::wstring& ext) {
						const auto action = std::make_shared<FileAction>(
							name, fullPath, false, parent
						);
				action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());

						allActions.push_back(action);
					}, defaultOptions);
		}

		
	

	}


	void OnActionExecute(std::shared_ptr<ActionBase>& action) override {
		if (!g_host) return;
		auto exampleAction = std::dynamic_pointer_cast<FileAction>(action);
		if (!exampleAction) return;

		exampleAction->Invoke();
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new FolderPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}


// case TRAY_MENU_ID_EDIT_CONFIG: // 编辑 JSON 配置文件
// 	{
// 		wchar_t path[MAX_PATH];
// 		wcsncpy_s(path, RUNNER_CONFIG_PATH.c_str(), MAX_PATH - 1);
// 		ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
// 	}
// 	break;
// 	ShellExecute(nullptr, L"open", RUNNER_CONFIG_PATH.c_str(), nullptr, nullptr, SW_SHOW);
