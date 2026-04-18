#include "../Plugin.hpp"
#include <windows.h>
#include <memory>


#include "../../util/MainTools.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <algorithm>
#include <cwctype>
#include <unordered_set>

#include "FileAction.hpp"
#include "IndexManagerWindow.hpp"
#include "ContextMenuHelper.hpp"
#include "FolderPluginConfigUtils.hpp"
#include "../../util/BitmapUtil.hpp"
#include "../../util/FileSystemTraverser.hpp"
#include "../../util/FileUtil.hpp"


// 任务队列系统
constexpr const char* OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY = "openFolderIndexedManager";
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


struct ParsedHotkey {
	UINT vk = 0;
	UINT mod = 0;
	bool valid = false;

	bool matches(UINT inVk, UINT inMod) const {
		return valid && inVk == vk && inMod == mod;
	}
};

static ParsedHotkey ParseHotkeyString(const std::string& utf8Str) {
	if (utf8Str.empty()) return {};
	std::wstring str = utf8_to_wide(utf8Str);

	size_t posEnd = str.rfind(L')');
	size_t posStart = str.rfind(L'(', posEnd);
	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1) return {};

	ParsedHotkey h;
	try { h.vk = static_cast<UINT>(std::stoi(str.substr(posStart + 1, posEnd - posStart - 1))); }
	catch (...) { return {}; }

	if (posStart > 0) {
		size_t posEnd2 = posStart - 1;
		size_t posStart2 = str.rfind(L'(', posEnd2);
		if (posStart2 != std::wstring::npos && posEnd2 > posStart2) {
			try { h.mod = static_cast<UINT>(std::stoi(str.substr(posStart2 + 1, posEnd2 - posStart2 - 1))); }
			catch (...) {}
		}
	}

	h.valid = true;
	return h;
}

static std::wstring NormalizeActionTitleForDedup(const std::wstring& title) {
	std::wstring normalized = title;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), towlower);
	return normalized;
}

class FolderPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	ParsedHotkey hkOpenFileLocation;
	ParsedHotkey hkOpenTargetLocation;
	ParsedHotkey hkOpenWithClipboard;
	ParsedHotkey hkRunAsAdmin;

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
		if (!g_host) return false;
		g_host->RegisterAppLaunchActionCallback(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY, []() {
			ShowIndexedManagerWindow(nullptr);
		});
		g_refreshFolderPlugin = [this]() {
			RefreshAllActions();
		};
		g_workerShouldStop = false;
		g_pluginIndexedRunningAppsThread = std::thread(WorkerThreadFunction);
		return true;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}


	void Shutdown() override {
		if (g_host) {
			g_host->UnregisterAppLaunchActionCallback(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY);
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
		const bool allowDuplicateItems = g_host->GetSettingsMap().at("com.candytek.folderplugin.allow_duplicate_items").boolValue;
		std::unordered_set<std::wstring> indexedNames;
		auto pushAction = [&](const std::shared_ptr<FileAction>& action) {
			if (!action) return;
			if (!allowDuplicateItems) {
				const std::wstring normalizedTitle = NormalizeActionTitleForDedup(action->getTitle());
				if (indexedNames.find(normalizedTitle) != indexedNames.end()) return;
				indexedNames.insert(normalizedTitle);
			}
			allPluginActions.push_back(action);
		};
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
															pushAction(action);
														});
				} else {
					::TraverseFiles(traverseOptions1.folder, traverseOptions1, EXE_FOLDER_PATH2, [&](const std::wstring& name,
									const std::wstring& fullPath,
									const std::wstring& parent,
									const std::wstring& ext) {
										const auto action = std::make_shared<FileAction>(
											name, fullPath, false, parent
										);
										action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
										pushAction(action);
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

					pushAction(action);
				}, traverseOptions1, EXE_FOLDER_PATH2);

				isPathAdded = true;
			} else if (traverseOptions1.type == L"uwp" && !isUwpAdded &&
				g_host->GetSettingsMap().at("com.candytek.folderplugin.uwp_apps").boolValue) {
				std::vector<std::shared_ptr<BaseAction>> uwpActions;
				traverseUwpApps(uwpActions, traverseOptions1);
				for (const auto& baseAction : uwpActions) {
					auto fileAction = std::dynamic_pointer_cast<FileAction>(baseAction);
					if (fileAction) pushAction(fileAction);
				}
				isUwpAdded = true;
			}
		}


		TraverseOptions emptyTraverseOptions;
		TraverseOptions defaultOptions = CreateDefaultPathTraverseOptions();
		if (!isUwpAdded && g_host->GetSettingsMap().at("com.candytek.folderplugin.uwp_apps").boolValue) {
			std::vector<std::shared_ptr<BaseAction>> uwpActions;
			traverseUwpApps(uwpActions, emptyTraverseOptions);
			for (const auto& baseAction : uwpActions) {
				auto fileAction = std::dynamic_pointer_cast<FileAction>(baseAction);
				if (fileAction) pushAction(fileAction);
			}
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
				pushAction(action);
			}, defaultOptions, EXE_FOLDER_PATH2);
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
			"key": "com.candytek.folderplugin.allow_duplicate_items",
			"title": "允许项目重复",
			"title_en": "Allow duplicate items",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.run_item_as_admin",
			"title": "以管理员身份运行项目",
			"title_en": "Run item as administrator",
			"type": "bool",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.folderplugin.indexed_manager",
			"title": "索引查看器",
			"type": "button",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_open_file_location",
			"title": "打开项目文件所在位置",
			"title_en": "Open item file location",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_open_target_location",
			"title": "打开项目目标所在位置",
			"title_en": "Open item target location",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.open_with_clipboard_params",
			"title": "打开附带剪贴板参数",
			"title_en": "Open with clipboard parameters",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_run_item_as_admin",
			"title": "以管理员身份运行项目",
			"title_en": "Run item as administrator",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		}

	]
}

   )";
	}


	void OnUserSettingsLoadDone() override {
		if (!g_host) return;
		g_host->RegisterAppLaunchActionCallback(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY, []() {
			ShowIndexedManagerWindow(nullptr);
		});

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

		const auto& settings = g_host->GetSettingsMap();
		hkOpenFileLocation   = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_open_file_location").stringValue);
		hkOpenTargetLocation = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_open_target_location").stringValue);
		hkOpenWithClipboard  = ParseHotkeyString(settings.at("com.candytek.folderplugin.open_with_clipboard_params").stringValue);
		hkRunAsAdmin         = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_run_item_as_admin").stringValue);
	}


	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!g_host) return false;
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
		if (!fileAction) return false;

		fileAction->Invoke();
		return true;
	}

	int OnSendHotKey(const std::shared_ptr<BaseAction> action, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		auto it = std::dynamic_pointer_cast<FileAction>(action);
		if (!it) return 0;

		if (hkOpenFileLocation.matches(vk, currentModifiers)) {
			it->InvokeOpenFolder();
			return 1;
		}
		if (hkOpenTargetLocation.matches(vk, currentModifiers)) {
			it->InvokeOpenGoalFolder();
			return 1;
		}
		if (hkOpenWithClipboard.matches(vk, currentModifiers)) {
			it->InvokeWithTargetClipBoard();
			return 1;
		}
		if (hkRunAsAdmin.matches(vk, currentModifiers)) {
			it->InvokeWithTarget(nullptr, true);
			return 1;
		}

		return 0;
	}


	void OnSettingItemExecute(const SettingItem* setting, HWND parentHwnd) override {
		if (setting->key == "com.candytek.folderplugin.indexed_manager") {
			ShowIndexedManagerWindow(parentHwnd);
		}
	}

	bool OnItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) override {
		ConsolePrintln(L"FolderPlugin", L"OnItemRightClick entered");
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
		if (!fileAction) {
			ConsolePrintln(L"FolderPlugin", L"OnItemRightClick skipped: action is not FileAction");
			return false;
		}
		ConsolePrintln(L"FolderPlugin", L"ShowShellContextMenu path=" + fileAction->GetTargetPath());
		ShowShellContextMenu(parentHwnd, fileAction->GetTargetPath(), screenPt);
		ConsolePrintln(L"FolderPlugin", L"ShowShellContextMenu returned");
		return true;
	}

	bool OnItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) override {
		ConsolePrintln(L"FolderPlugin", L"OnItemShiftRightClick entered");
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
		if (!fileAction) {
			ConsolePrintln(L"FolderPlugin", L"OnItemShiftRightClick skipped: action is not FileAction");
			return false;
		}
		ConsolePrintln(L"FolderPlugin", L"ShowMyContextMenu path=" + fileAction->GetTargetPath());
		const UINT cmd = ShowMyContextMenu(parentHwnd, fileAction->GetTargetPath(), screenPt);
		ConsolePrintln(L"FolderPlugin", L"ShowMyContextMenu cmd=" + std::to_wstring(cmd));
		switch (cmd) {
		case IDM_RUN_AS_ADMIN: fileAction->InvokeWithTarget(nullptr, true);
			break;
		case IDM_OPEN_IN_CONSOLE: OpenConsoleHere(SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		case IDM_KILL_PROCESS: KillProcessByImagePath(SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		case IDM_COPY_PATH: CopyTextToClipboard(parentHwnd, fileAction->GetTargetPath());
			break;
		case IDM_COPY_TARGET_PATH: CopyTextToClipboard(parentHwnd, SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		default: break;
		}
		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new FolderPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 2;
}


// case TRAY_MENU_ID_EDIT_CONFIG: // 编辑 JSON 配置文件
// 	{
// 		wchar_t path[MAX_PATH];
// 		wcsncpy_s(path, RUNNER_CONFIG_PATH.c_str(), MAX_PATH - 1);
// 		ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
// 	}
// 	break;
// 	ShellExecute(nullptr, L"open", RUNNER_CONFIG_PATH.c_str(), nullptr, nullptr, SW_SHOW);
