#pragma once

#include <fstream> // Required for file operations

#include "../util/BaseTools.hpp"
#include "../common/Resource.h"
#include "../common/Constants.hpp"
#include "../util/ShortcutUtil.hpp"
#include "../common/GlobalState.hpp"
#include "../view/SwitchView.hpp"
#include "../util/PinyinHelper.hpp"
#include "../util/FileSystemTraverser.hpp"
#include "plugins/PluginManager.hpp"
#include "util/FileUtil.hpp"
#include "util/MyJsonUtil.hpp"
#include "view/CustomButtonHelper.hpp"


// tab按钮句柄，按json 顺序
inline std::vector<HWND> hTabButtons;
// tab句柄，按json 顺序
inline std::vector<HWND> tabContainers;
inline int currentSubPageIndex = 0;
inline std::wstring originalSkinPath;

// tab下所有控件句柄，用来实现页面滚动功能，整体移动该页控件
inline std::vector<std::vector<HWND>> hCtrlsByTab;

// 用来查找程序目录下的所有皮肤文件
static std::vector<std::wstring> FindSkinFiles() {
	std::vector<std::wstring> skinFiles;
	TraverseOptions options;
	options.folder = EXE_FOLDER_PATH;
	options.recursive = true;
	options.extensions = {L".json"};

	TraverseFiles(EXE_FOLDER_PATH, options, EXE_FOLDER_PATH, [&](const std::wstring& name, const std::wstring& fullPath,
																const std::wstring& parent, const std::wstring& ext) {
		if (MyStartsWith(name, L"skin_")) {
			skinFiles.push_back(fullPath);
		}
	});

	return skinFiles;
}

// 获取 settings.json 文件内容
static std::string GetSettingsJsonText() {
	// 正式版使用程序内嵌的文本，debug版使用固定路径，因为频繁调试下cmake并不会频繁更新内嵌的json文本
#ifndef NDEBUG
	std::string jsonText = ReadUtf8File(LR"(C:\Users\Administrator\source\repos\WindowsProject1\settings.json)");
	if (jsonText.empty()) return GetAppResourceText(IDR_SETTINGS_JSON);
	return jsonText;
#else
	return GetAppResourceText(IDR_SETTINGS_JSON);
#endif
}


// 从user_settings.json文件加载用户的配置
static nlohmann::json loadUserConfig(const std::wstring& filename) {
	std::string f = ReadUtf8File(filename);
	nlohmann::json userConfig;
	try {
		// Parse the file content into the JSON object.
		userConfig = nlohmann::json::parse(f);
	} catch (const nlohmann::json::parse_error& e) {
		// The file is corrupted or not valid JSON.
		std::wstring error_msg = L"Config file load error in '" + filename +
			L"'. Using default settings.\n\nError: " + utf8_to_wide(e.what());
		MessageBoxW(nullptr, error_msg.c_str(), L"Config Load Error", MB_OK | MB_ICONWARNING);
		return {}; // Return empty to fall back to defaults.
	}
	return userConfig;
}


static void LoadSettingsMap() {
	g_settings_map.clear();

	// 递归加载设置项到map中
	std::function<void(const std::vector<SettingItem>&)> loadSettingsToMap =
		[&](const std::vector<SettingItem>& settings) {
		for (const auto& item : settings) {
			g_settings_map[item.key] = item;
			if ((item.type == "expand" || item.type == "expandswitch") && !item.children.empty()) {
				loadSettingsToMap(item.children);
			}
		}
	};

	loadSettingsToMap(g_settings_ui_last_save);
	// auto it = g_settings_map.find("com.candytek.normallaunchplugin.moreitem");
	// auto it = g_settings_map.find("com.candytek.folderplugin.envpath_apps");
	// auto it = g_settings_map.find("pref_use_everything_sdk_index");
	// bool isFind =it != g_settings_map.end();
}

// 保存配置到JSON文件
static void saveConfigToFile(const std::wstring& filename, const nlohmann::json& newConfig) {
	const std::string newSettingsText = newConfig.dump(4);
	std::ofstream o(filename);
	o << newSettingsText << std::endl;
	o.close();
}

// 递归深度优先遍历
static SettingItem* getSettingItemForCtrlSubId(const size_t targetIndex, std::vector<SettingItem>& settings) {
	size_t index = 0;

	std::function<SettingItem*(std::vector<SettingItem>&)> dfs =
		[&](std::vector<SettingItem>& items) -> SettingItem* {
		for (auto& item : items) {
			if (index == targetIndex) {
				return &item;
			}
			index++;
			if (item.type == "expand" || item.type == "expandswitch") {
				if (auto* found = dfs(item.children)) {
					return found;
				}
			}
		}
		return nullptr;
	};

	return dfs(settings);
}

// 根据tab名称查找索引
static int FindTabIndexByName(const std::string& name) {
	for (size_t i = 0; i < subPageTabs.size(); ++i) {
		if (subPageTabs[i] == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

static HWND FindTabHwndByName(const std::string& name) {
	int index = FindTabIndexByName(name);
	if (FindTabIndexByName(name) == -1) {
		return nullptr;
	}
	return tabContainers[index];
}

// 根据索引获取tab的HWND，用于SettingItem.subPageIndex
static HWND FindTabHwndByIndex(uint8_t index) {
	if (index < tabContainers.size()) {
		return tabContainers[index];
	}
	return nullptr;
}

// 更改tab页，使用模拟点击的方法
static void SwitchToTab(const std::string& tabName) {
	int newIdx = FindTabIndexByName(tabName);
	if (newIdx >= 0 && newIdx < static_cast<int>(hTabButtons.size())) {
		HWND hBtn = hTabButtons[newIdx];
		HWND hParent = GetParent(hBtn);

		// 模拟用户点击按钮，发送 WM_COMMAND
		SendMessage(hParent, WM_COMMAND,
					MAKEWPARAM(GetDlgCtrlID(hBtn), BN_CLICKED),
					(LPARAM)hBtn);
	}
}

// 递归保存设置控件的值
static void saveSettingControlValues(nlohmann::json& newConfig2, const bool isJustCurrectTab = false) {
	size_t index = 2999;

	std::function<void(std::vector<SettingItem>&, nlohmann::json&)> saveSettingControlValuesSub =
		[&](std::vector<SettingItem>& settings, nlohmann::json& newConfig) {
		for (SettingItem& item : settings) {
			index++; // 对应CreateSettingControlsForTab中的currentCtrlSubId++，每个item都会消耗一个ID

			// expand类型不需要保存值，但需要递归保存其子项
			if (item.type == "expand") {
				saveSettingControlValuesSub(item.children, newConfig);
				continue;
			}
			// expandswitch类型需要保存开关值，同时递归保存子项
			if (item.type == "expandswitch") {
				// 通过当前索引来获取hwnd
				HWND hCtrl = nullptr;
				if (HWND parentHwnd = FindTabHwndByIndex(item.subPageIndex); parentHwnd != nullptr) {
					hCtrl = GetDlgItem(parentHwnd, (int)index);
				}

				if (hCtrl) {
					bool value = GetExpandSwitchState(hCtrl);
					newConfig[item.key] = value;
					item.setValue(nlohmann::json(value));
				}

				// 递归保存子项
				saveSettingControlValuesSub(item.children, newConfig);
				continue;
			}
			if (isJustCurrectTab) {
				if (item.subPageIndex != currentSubPageIndex) continue;
			}

			// 跳过不需要保存的类型
			if (item.type == "text" || item.type == "button") {
				continue;
			}

			// 通过当前索引来获取hwnd
			HWND hCtrl = nullptr;
			if (HWND parentHwnd = FindTabHwndByIndex(item.subPageIndex); parentHwnd != nullptr) {
				hCtrl = GetDlgItem(parentHwnd, (int)index);
			}

			if (!hCtrl) {
				ConsolePrintln("找不到这个key: " + item.title);
				continue;
			}

			const std::string& key = item.key;

			if (item.type == "bool") {
				bool value = GetSwitchState(hCtrl);
				newConfig[key] = value;
				item.setValue(nlohmann::json(value));
			} else if (item.type == "string") {
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string value = wide_to_utf8(std::wstring(buf));
				newConfig[key] = value;
				item.setValue(nlohmann::json(value));
			} else if (item.type == "stringArr") {
				int textLen = GetWindowTextLengthW(hCtrl);
				if (textLen > 0) {
					std::vector<wchar_t> buf(textLen + 1);
					GetWindowTextW(hCtrl, &buf[0], textLen + 1);
					std::wstring ws(&buf[0]);

					std::vector<std::string> arr;
					size_t start = 0;
					while (start < ws.size()) {
						size_t end = ws.find_first_of(L"\r\n", start);
						if (end == std::wstring::npos) end = ws.size();

						std::wstring line = ws.substr(start, end - start);
						if (!line.empty()) {
							arr.push_back(wide_to_utf8(line));
						}

						start = ws.find_first_not_of(L"\r\n", end);
						if (start == std::wstring::npos) break;
					}
					newConfig[key] = arr;
					item.setValue(nlohmann::json(arr));
				} else {
					// If the text box is empty, save an empty array
					nlohmann::json empty_array = nlohmann::json::array();
					newConfig[key] = empty_array;
					item.setValue(empty_array);
				}
			} else if (item.type == "list") {
				int selIdx = static_cast<int>(SendMessage(hCtrl, CB_GETCURSEL, 0, 0));
				if (selIdx != CB_ERR && static_cast<size_t>(selIdx) < item.entryValues.size()) {
					// Save the underlying value, not the displayed text
					const std::string& value = item.entryValues[selIdx];
					newConfig[key] = value;
					item.setValue(nlohmann::json(value));
				}
			} else if (item.type == "long") {
				wchar_t buf[32];
				GetWindowTextW(hCtrl, buf, 32);
				long value = _wtol(buf);
				newConfig[key] = value;
				item.setValue(nlohmann::json(value));
			} else if (item.type == "hotkeystring") {
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string value = wide_to_utf8(std::wstring(buf));
				newConfig[key] = value;
				item.setValue(nlohmann::json(value));
			}
		}
	};
	saveSettingControlValuesSub(g_settings_ui, newConfig2);
}


static void initGlobalVariable() {
	pref_show_window_and_release_modifier_key = (g_settings_map["pref_show_window_and_release_modifier_key"].boolValue);
	pref_ctrl_number_launch_item = (g_settings_map["pref_ctrl_number_launch_item"].boolValue);
	pref_alt_number_launch_item = (g_settings_map["pref_alt_number_launch_item"].boolValue);
	pref_switch_list_right_click_with_shift_right_click = (g_settings_map[
		"pref_switch_list_right_click_with_shift_right_click"].boolValue);
	pref_show_window_and_release_modifier_key = (g_settings_map["pref_show_window_and_release_modifier_key"].boolValue);
	pref_run_item_as_admin = (g_settings_map["pref_run_item_as_admin"].boolValue);
	pref_hide_in_fullscreen = (g_settings_map["pref_hide_in_fullscreen"].boolValue);
	pref_hide_in_topmost_fullscreen = (g_settings_map["pref_hide_in_topmost_fullscreen"].boolValue);
	pref_lock_window_popup_position = (g_settings_map["pref_lock_window_popup_position"].boolValue);
	pref_preserve_last_search_term = (g_settings_map["pref_preserve_last_search_term"].boolValue);
	pref_single_click_to_open = (g_settings_map["pref_single_click_to_open"].boolValue);
	pref_fuzzy_match = (g_settings_map["pref_fuzzy_match"].boolValue);
	pref_last_search_term_selected = (g_settings_map["pref_last_search_term_selected"].boolValue);
	pref_close_after_open_item = (g_settings_map["pref_close_after_open_item"].boolValue);
	pref_force_ime_mode = g_settings_map["pref_force_ime_mode"].stringValue;
	pref_max_search_results = g_settings_map["pref_max_search_results"].intValue;
	pref_hotkey_toggle_main_panel = g_settings_map["pref_hotkey_toggle_main_panel"].stringValue;
	pref_fuzzy_match_score_threshold = g_settings_map["pref_fuzzy_match_score_threshold"].intValue;
	EDIT_HINT_TEXT = utf8_to_wide(g_settings_map["pref_search_box_placeholder"].stringValue);

	int64_t tempX = g_settings_map["pref_window_popup_position_offset_x"].intValue;
	int64_t tempY = g_settings_map["pref_window_popup_position_offset_y"].intValue;
	if (tempX >= 0 && tempX <= 100) {
		window_position_offset_x = static_cast<float>(tempX) / 100.0f;
	}
	if (tempY >= 0 && tempY <= 100) {
		window_position_offset_y = static_cast<float>(tempY) / 100.0f;
	}
	PinyinHelper::SetActivePinyinScheme(g_settings_map["pref_pinyin_mode"].stringValue);
}

// 程序开始加载所有用户配置项
static void LoadSettingList() {
	g_settings_ui_last_save.clear();
	subPageTabs.clear();
	const std::string settingsText = GetSettingsJsonText();
	std::vector<SettingItem> pluginSettings;

	if (g_pluginManager) {
		for (const auto& pluginCatalog : g_pluginManager->GetPluginCatalog()) {
		try {
			SettingItem setting;
			setting.key = wide_to_utf8(pluginCatalog.pkgName);
			setting.type = "expandswitch";
			setting.title = wide_to_utf8(pluginCatalog.name);
			setting.subPageIndex = GetTabIndexForSetting("plugin");
			setting.setValue(nlohmann::json(pluginCatalog.enabled));

			if (const PluginInfo* loadedPlugin = g_pluginManager->FindLoadedPluginByPackageName(pluginCatalog.pkgName)) {
				setting.pluginId = loadedPlugin->pluginId;
				const std::wstring defaultSettingJson = pluginCatalog.defaultSettingJson.empty()
					? loadedPlugin->plugin->DefaultSettingJson()
					: pluginCatalog.defaultSettingJson;
				setting.children = ParseConfig(wide_to_utf8(defaultSettingJson), loadedPlugin->pluginId);
			} else if (!pluginCatalog.defaultSettingJson.empty() && pluginCatalog.defaultSettingJson != L"{}") {
				setting.children = ParseConfig(wide_to_utf8(pluginCatalog.defaultSettingJson), 65535);
			}

			pluginSettings.push_back(setting);
		} catch (...) {
			ConsolePrintln(L"[SettingsManager]注意，该插件的设置json加载失败:" + pluginCatalog.name);
		}
		}
	}

	std::vector<SettingItem> appSettings = ParseConfig(settingsText, 65535);
	g_settings_ui_last_save.insert(g_settings_ui_last_save.end(), pluginSettings.begin(), pluginSettings.end());
	g_settings_ui_last_save.insert(g_settings_ui_last_save.end(), appSettings.begin(), appSettings.end());

	bool isFind = false;
	for (size_t i = 0; i < g_settings_ui_last_save.size(); ++i) {
		if (g_settings_ui_last_save[i].key == "com.candytek.featurelaunchplugin.moreitem") {
			isFind = true;
			break;
		}
	}
	// auto it = g_settings_ui_last_save.find("com.candytek.normallaunchplugin.moreitem");
	// auto it = g_settings_map.find("com.candytek.folderplugin.envpath_apps");
	// auto it = g_settings_map.find("pref_use_everything_sdk_index");
	// bool isFind =it != g_settings_map.end();


	nlohmann::json userConfig = loadUserConfig(USER_SETTINGS_PATH);
	if (!g_settings_ui_last_save.empty()) {
		// 将用户设置合并到主要设置结构中。此此更新在UI构建之前更新每个项目的" defvalue"。
		if (!userConfig.is_null() && !userConfig.empty()) {
			// 递归处理设置项和子项
			std::function<void(std::vector<SettingItem>&)> mergeUserSettings =
				[&](std::vector<SettingItem>& settings) {
				for (auto& setting : settings) {
					if (userConfig.contains(setting.key)) {
						// 该键存在一个保存的值。用用户的值将设置的默认值置换。Nlohmann:: JSON库自动处理类型转换。
						try {
							setting.setValue(userConfig[setting.key]);
						} catch (...) {
						}
					}
					// 递归处理expand类型的子项
					if ((setting.type == "expand" || setting.type == "expandswitch") && !setting.children.empty()) {
						mergeUserSettings(setting.children);
					}
				}
			};

			mergeUserSettings(g_settings_ui_last_save);
		}
	}

	LoadSettingsMap();
	initGlobalVariable();
}

// 等未来彻底稳定了再实现该方法
static void resetToDefaults(HWND hwnd, const std::vector<std::string>& subPages,
							std::vector<SettingItem>& settings2,
							const std::vector<std::vector<HWND>>& hCtrlsByTab) {
}

// 等未来彻底稳定了再实现该方法
static void applySettings(HWND hwnd, const std::vector<std::string>& subPages,
						std::vector<SettingItem>& settings2,
						const std::vector<std::vector<HWND>>& hCtrlsByTab) {
}


static void handleButtonAction(HWND hwnd, const std::string& key) {
	if (key == "pref_open_config_folder") {
		// Open the configuration folder
		std::wstring currentPath = GetCurrentWorkingDirectoryW();
		ShellExecuteW(hwnd, L"open", L"explorer.exe", currentPath.c_str(), nullptr, SW_SHOW);
	} else if (key == "pref_backup_settings") {
		// Create a backup of current settings
		wchar_t fileName[MAX_PATH];
		SYSTEMTIME st;
		GetLocalTime(&st);
		swprintf_s(fileName, L"settings_backup_%04d%02d%02d_%02d%02d%02d.json",
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

		if (CopyFileW(USER_SETTINGS_PATH.c_str(), fileName, FALSE)) {
			std::wstring message = L"设置已备份到: " + std::wstring(fileName);
			MessageBoxW(hwnd, message.c_str(), L"备份成功", MB_OK | MB_ICONINFORMATION);
		} else {
			MessageBoxW(hwnd, L"备份失败", L"错误", MB_OK | MB_ICONERROR);
		}
	} else if (key == "pref_restore_settings") {
		// Show file dialog to select backup file
		OPENFILENAMEW ofn;
		wchar_t szFile[260] = {0};

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hwnd;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = nullptr;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = nullptr;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameW(&ofn)) {
			if (CopyFileW(szFile, USER_SETTINGS_PATH.c_str(), FALSE)) {
				MessageBoxW(hwnd, L"设置已恢复，请重启程序生效", L"恢复成功", MB_OK | MB_ICONINFORMATION);
			} else {
				MessageBoxW(hwnd, L"恢复失败", L"错误", MB_OK | MB_ICONERROR);
			}
		}
	}
}

static void AddAppStartupTime() {
	auto it = std::find_if(g_settings_ui_last_save.rbegin(), g_settings_ui_last_save.rend(),
							[](const SettingItem& item) {
								return item.key == "pref_about_other1";
							});
	if (it == g_settings_ui_last_save.rend()) {
		return;
	}
	it->title = "本次应用启动完成耗时: " + std::to_string(APP_STARTUP_TIME) + "ms";
}
