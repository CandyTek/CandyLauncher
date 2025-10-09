#include "../Plugin.hpp"
#include <windows.h>
#include <memory>


#include "../../util/MainTools.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "FileAction.hpp"
#include "IndexManager.hpp"
#include "RunnerConfigUtils.hpp"
#include "util/BitmapUtil.hpp"
#include "util/FileSystemTraverser.hpp"
#include "util/FileUtil.hpp"


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

inline const wchar_t* kLinkName = L"CandyLauncher 索引文件夹.lnk";
inline const wchar_t* kLinkDesc = L"CandyLauncher 索引文件夹";

inline bool InstallSendToEntry(const std::wstring& exePath) {
	HRESULT hr = CoInitialize(nullptr);
	bool needUninit = SUCCEEDED(hr);

	PWSTR sendto = nullptr;
	std::wstring linkPath;
	bool ok = false;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
		linkPath.assign(sendto);
		if (!linkPath.empty() && linkPath.back() != L'\\') linkPath += L'\\';
		linkPath += kLinkName;

		IShellLinkW* psl = nullptr;
		hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
							IID_PPV_ARGS(&psl));
		if (SUCCEEDED(hr)) {
			psl->SetPath(exePath.c_str()); // 目标：CreateShortcut.exe
			psl->SetDescription(kLinkDesc);
			// 这里不需要设置 "%1"；SendTo 会自动把所选文件路径附加为参数
			// 如需自定义前缀参数，可： psl->SetArguments(L"--mode index");

			IPersistFile* ppf = nullptr;
			hr = psl->QueryInterface(IID_PPV_ARGS(&ppf));
			if (SUCCEEDED(hr)) {
				hr = ppf->Save(linkPath.c_str(), TRUE);
				ppf->Release();
				ok = SUCCEEDED(hr);
			}
			psl->Release();
		}
		CoTaskMemFree(sendto);
	}

	if (needUninit) CoUninitialize();
	return ok;
}

inline bool DeleteSendToEntry(const std::wstring& shortcutName) {
	HRESULT hr = CoInitialize(nullptr);
	bool needUninit = SUCCEEDED(hr);

	bool ok = false;
	PWSTR sendto = nullptr;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
		std::wstring linkPath(sendto);
		if (!linkPath.empty() && linkPath.back() != L'\\') linkPath += L'\\';
		linkPath += shortcutName; // 例如 L"CandyLauncher 索引文件夹.lnk"

		// 尝试删除文件
		if (DeleteFileW(linkPath.c_str())) {
			ok = true;
		} else {
			// 如果不存在也视为成功
			ok = (GetLastError() == ERROR_FILE_NOT_FOUND);
		}

		CoTaskMemFree(sendto);
	}

	if (needUninit) CoUninitialize();

	return ok;
}


class FolderPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;

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
	
	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}


	void Shutdown() override {
		if (g_host) {
			stopThreadPluginRunningApps();
		}
		g_host = nullptr;
	}


	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!g_host) return {};
		return allPluginActions;
	}

	void RefreshAllActions() override {
		allPluginActions.clear();
		// 遍历运行中的窗口并添加到列表
		bool isPathAdded = false;
		bool isUwpAdded = false;

		std::vector<TraverseOptions> runnerConfigs = ParseRunnerConfig();
		for (TraverseOptions traverseOptions1 : runnerConfigs) {
			if (traverseOptions1.type.empty()) {
				if (g_host->GetSettingsMap().at("pref_use_everything_sdk_index").boolValue) {
					g_host->TraverseFilesForEverythingSDK(traverseOptions1.folder, traverseOptions1, [&](const std::wstring& name,
														const std::wstring& fullPath,
														const std::wstring& parent,
														const std::wstring& ext) {
															const auto action = std::make_shared<FileAction>(
																name, fullPath, false, parent
															);
															action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
															allPluginActions.push_back(action);
														});
				} else {
					::TraverseFiles(traverseOptions1.folder, traverseOptions1,EXE_FOLDER_PATH2, [&](const std::wstring& name,
																					const std::wstring& fullPath,
																					const std::wstring& parent,
																					const std::wstring& ext) {
						const auto action = std::make_shared<FileAction>(
							name, fullPath, false, parent
						);
						action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
						allPluginActions.push_back(action);
					});
				}
			} else if (traverseOptions1.type == L"path" && !isPathAdded &&
				g_host->GetSettingsMap().at("com.candytek.folderplugin.envpath_apps").boolValue) {
				TraversePATHExecutables2([&](const std::wstring& name,
											const std::wstring& fullPath,
											const std::wstring& parent,
											const std::wstring& ext) {
					const auto action = std::make_shared<FileAction>(
						name, fullPath, false, parent
					);
					action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());

					allPluginActions.push_back(action);
				}, traverseOptions1,EXE_FOLDER_PATH2);

				isPathAdded = true;
			} else if (traverseOptions1.type == L"uwp" && !isUwpAdded &&
				g_host->GetSettingsMap().at("com.candytek.folderplugin.uwp_apps").boolValue) {
				traverseUwpApps(allPluginActions, traverseOptions1);
				isUwpAdded = true;
			}
		}


		TraverseOptions emptyTraverseOptions;
		TraverseOptions defaultOptions = CreateDefaultPathTraverseOptions();
		if (!isUwpAdded && g_host->GetSettingsMap().at("com.candytek.folderplugin.uwp_apps").boolValue) {
			traverseUwpApps(allPluginActions, emptyTraverseOptions);
		} else if (!isPathAdded && g_host->GetSettingsMap().at("com.candytek.folderplugin.envpath_apps").boolValue) {
			// TODO: 解决这个崩溃问题
			TraversePATHExecutables2([&](const std::wstring& name,
										const std::wstring& fullPath,
										const std::wstring& parent,
										const std::wstring& ext) {
				const auto action = std::make_shared<FileAction>(
					name, fullPath, false, parent
				);
				action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
				allPluginActions.push_back(action);
			}, defaultOptions,EXE_FOLDER_PATH2);
		}
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.folderplugin.uwp_apps",
			"title": "索引UWP应用",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.regedit_apps",
			"title": "索引注册表中注册的应用",
			"type": "bool",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.folderplugin.envpath_apps",
			"title": "索引 %PATH% 中的可执行文件",
			"type": "bool",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.folderplugin.show_sendto_shortcut",
			"title": "在右键 \"发送到\" 菜单中显示 \"CandyLauncher 索引文件夹\"",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.indexed_manager",
			"title": "索引查看器",
			"type": "button",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )";
	}


	void OnUserSettingsLoadDone() override {
		bool pref_indexed_apps_show_sendto_shortcut = g_host->GetSettingsMap().at("com.candytek.folderplugin.show_sendto_shortcut").
															boolValue;

		PWSTR sendto = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
			// 需要编译CreateShortcut 文件夹里的win32 程序，然后把exe放在CandyLauncher.exe 同一个目录下
			std::wstring createShortcutExePath;
			createShortcutExePath.append(GetExecutableFolder()).append(L"\\CreateShortcut.exe");
			std::wstring sendToShortcutName;
			sendToShortcutName.append(sendto).append(L"\\").append(kLinkName);

			bool sendtoShortcutExists = PathFileExistsW(sendToShortcutName.c_str());

			if (pref_indexed_apps_show_sendto_shortcut) {
				if (!sendtoShortcutExists) {
					// 创建快捷方式
					InstallSendToEntry(createShortcutExePath);
				}
			} else {
				if (sendtoShortcutExists) {
					bool result = DeleteSendToEntry(kLinkName);
					// ShowErrorMsgBox(L"删除快捷方式" + result);
				}
			}
		}
	}


	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!g_host) return false;
		auto exampleAction = std::dynamic_pointer_cast<FileAction>(action);
		if (!exampleAction) return false;

		exampleAction->Invoke();
		return true;
	}

	void OnSettingItemExecute(const SettingItem* setting,HWND parentHwnd) override {
		if (setting->key == "com.candytek.folderplugin.indexed_manager") {
			ShowIndexedManagerWindow(parentHwnd);
		}
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
