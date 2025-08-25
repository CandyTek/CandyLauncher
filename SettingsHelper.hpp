#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls

#include "BaseTools.hpp"
#include "Resource.h"
#include "Constants.hpp"
#include "ShortCutHelper.hpp"
#include "DataKeeper.hpp"
#include "IndexedManager.hpp"
#include "SwitchView.h"
#include "PinyinHelper.h"

// Global button list





static bool to_bool(const nlohmann::json& v) {
	if (v.is_boolean()) return v.get<bool>();
	if (v.is_number_integer()) return v.get<int>() != 0;          // 兼容 0/1
	if (v.is_string()) {
		std::string s = v.get<std::string>();
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		return (s == "true" || s == "1" || s == "yes" || s == "on");
	}
	throw std::runtime_error("not a boolean-like value");
}

inline void setSettingItemValue(SettingItem& item){
	if (item.type == "string" || item.type == "hotkeystring" || item.type == "list") {
		item.stringValue = item.defValue.get<std::string>();
	}else if(item.type == "long"){
		item.intValue = item.defValue.get<int64_t>();
//		ShowErrorMsgBox(L""+std::to_wstring(std::get<int64_t>(item.value)));
	}else if(item.type == "bool"){
		item.boolValue = to_bool(item.defValue);
		
//		ShowErrorMsgBox(L""+std::to_wstring(item.boolValue));
	}else if(item.type == "double"){
		item.doubleValue = item.defValue.get<double>();
	}
}



inline std::vector<SettingItem> ParseConfig(const std::string &configStr) {
	nlohmann::json j = nlohmann::json::parse(configStr);
	std::vector<SettingItem> settings;

	for (auto &item: j["prefList"]) {
		SettingItem setting;
		setting.key = item["key"].get<std::string>();
		setting.type = item["type"].get<std::string>();
		setting.title = item["title"].get<std::string>();
		setting.subPage = item["subPage"].get<std::string>();

		// 如果存在 entries，就解析；否则设为空
		if (item.contains("entries")) {
			setting.entries = item["entries"].get<std::vector<std::string>>();
		} else {
			setting.entries = {};
		}

		if (item.contains("entryValues")) {
			setting.entryValues = item["entryValues"].get<std::vector<std::string>>();
		} else {
			setting.entryValues = {};
		}

		setting.defValue = item["defValue"]; // 保持原始 JSON 类型，灵活性更高
		setSettingItemValue(setting);
		settings.push_back(setting);
	}

	return settings;
}


static std::string GetSettingsJsonText2() {
	HMODULE hModule = GetModuleHandleW(nullptr);

	HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_SETTINGS_JSON), RT_RCDATA);
	if (!hRes)
		throw std::runtime_error("Resource not found");

	HGLOBAL hResLoad = LoadResource(hModule, hRes);
	if (!hResLoad)
		throw std::runtime_error("LoadResource failed");

	DWORD size = SizeofResource(hModule, hRes);
	const void *pData = LockResource(hResLoad);
	if (!pData)
		throw std::runtime_error("LockResource failed");

	std::string settingsText(static_cast<const char *>(pData), size);
	return settingsText;
}


#ifndef NDEBUG

static std::string GetSettingsJsonText() {
	std::string jsonText = ReadUtf8File(LR"(C:\Users\Administrator\source\repos\WindowsProject1\settings.json)");
	if (jsonText.empty())return GetSettingsJsonText2();
	return jsonText;
}

#else
static std::string GetSettingsJsonText()
{
	return GetSettingsJsonText2();
}

#endif


/**
 * @brief Loads the user's configuration from a JSON file.
 * * @param filename The name of the file to load.
 * @return A nlohmann::json object containing the user's settings.
 * Returns an empty json object if the file doesn't exist or is invalid.
 */
static nlohmann::json loadUserConfig(const std::string &filename = "user_settings.json") {
	std::string f=ReadUtf8File(filename);
	nlohmann::json userConfig;
	try {
		// Parse the file content into the JSON object.
		userConfig = nlohmann::json::parse(f);
	}
	catch (const nlohmann::json::parse_error &e) {
		// The file is corrupted or not valid JSON.
		std::string error_msg = "Config file load error in '" + filename +
								"'. Using default settings.\n\nError: " + e.what();
		MessageBoxA(nullptr, error_msg.c_str(), "Config Load Error", MB_OK | MB_ICONWARNING);
		return {}; // Return empty to fall back to defaults.
	}
	return userConfig;
}


static void LoadSettingsMap() {
	g_settings_map.clear();
	for (const auto &item: g_settings2) {
		g_settings_map[item.key] = item;
	}
}


static void saveConfig(HWND hwnd, const std::vector<std::wstring> &subPages, std::vector<SettingItem> &settings2,
					   const std::vector<std::vector<HWND>> &hCtrlsByTab,
					   const std::string &filename = "user_settings.json") {
	// The final JSON object will be a simple key-value map.
	nlohmann::json newConfig;

	// Iterate over each tab to process its controls
	for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
		// ctrlIdx tracks the position in the hCtrlsByTab[tabIdx] vector.
		// It increments by 2 for each setting (label + control).
		int ctrlIdx = 0;

		// Iterate through all setting definitions to find those for the current tab
		for (auto &item: settings2) {
			// Skip if the setting does not belong to the current tab
			if (utf8_to_wide(item.subPage) != subPages[tabIdx])
				continue;

			// "text" and "button" types are for display only and have no value to save.
			// They also don't have a corresponding value control, so we skip them.
			if (item.type == "text" || item.type == "button") {
				continue;
			}

			// Retrieve the handle for the actual input control.
			// hCtrlsByTab stores HWNDs in pairs: [hLabel1, hCtrl1, hLabel2, hCtrl2, ...]
			HWND hCtrl = hCtrlsByTab[tabIdx][static_cast<size_t>(ctrlIdx + 1)];
			const std::string &key = item.key;

			if (item.type == "bool") {
				bool value = GetSwitchState(hCtrl);
				newConfig[key] = value;
				item.defValue = nlohmann::json(value);
			} else if (item.type == "string") {
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string value = wide_to_utf8(std::wstring(buf));
				newConfig[key] = value;
				item.defValue = nlohmann::json(value);
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
					item.defValue = nlohmann::json(arr);
				} else {
					// If the text box is empty, save an empty array
					nlohmann::json empty_array = nlohmann::json::array();
					newConfig[key] = empty_array;
					item.defValue = empty_array;
				}
			} else if (item.type == "list") {
				int selIdx = (int) SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
				if (selIdx != CB_ERR && static_cast<size_t>(selIdx) < item.entryValues.size()) {
					// Save the underlying value, not the displayed text
					const std::string &value = item.entryValues[selIdx];
					newConfig[key] = value;
					item.defValue = nlohmann::json(value);
				}
			} else if (item.type == "long") {
				wchar_t buf[32];
				GetWindowTextW(hCtrl, buf, 32);
				long value = _wtol(buf);
				newConfig[key] = value;
				item.defValue = nlohmann::json(value);
			} else if (item.type == "hotkeystring") {
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string value = wide_to_utf8(std::wstring(buf));
				newConfig[key] = value;
				item.defValue = nlohmann::json(value);
			}
			setSettingItemValue(item);

			// Move to the next pair of controls for the next setting item
			ctrlIdx += 2;
		}
	}

	// Convert the JSON object to a formatted string
	std::string newSettingsText = newConfig.dump(4);

	// Display the result in a message box as the original code did.
	// In a real application, you would save this string to your settings file,
	// for example, by calling a function like `WriteSettingsJsonText(newSettingsText);`
	// ShowErrorMsgBox(newSettingsText);

	// Write the string to the user settings file.
	std::ofstream o(filename);
	o << newSettingsText << std::endl;
	o.close();

	// Notify the user.
	// MessageBoxW(hwnd, L"Settings saved successfully!", L"Success", MB_OK);
	LoadSettingsMap();
	ShowWindow(g_settingsHwnd, SW_MINIMIZE);
	PostMessage(g_mainHwnd, WM_CONFIG_SAVED, 1, 0);
}


static void initGlobalVariable() {
	pref_show_window_and_release_modifier_key = (g_settings_map["pref_show_window_and_release_modifier_key"].boolValue);
	pref_ctrl_number_launch_item = (g_settings_map["pref_ctrl_number_launch_item"].boolValue);
	pref_alt_number_launch_item = (g_settings_map["pref_alt_number_launch_item"].boolValue);
	pref_switch_list_right_click_with_shift_right_click = (g_settings_map["pref_switch_list_right_click_with_shift_right_click"].boolValue);
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
}

static void LoadSettingList() {
	const std::string settingsText = GetSettingsJsonText();
	try {
		g_settings2 = ParseConfig(settingsText);
	}
	catch (...) {
		ShowErrorMsgBox(L"解析基础设置项失败！");
		ExitProcess(1);
	}
	nlohmann::json userConfig = loadUserConfig();
	// 将用户设置合并到主要设置结构中。此此更新在UI构建之前更新每个项目的“ defvalue”。
	if (!userConfig.is_null() && !userConfig.empty()) {
		for (auto &setting: g_settings2) {
			if (userConfig.contains(setting.key)) {
				// 该键存在一个保存的值。用用户的值将设置的默认值置换。Nlohmann:: JSON库自动处理类型转换。
				setting.defValue = userConfig[setting.key];
				if(setting.defValue.is_boolean()){
//					ShowErrorMsgBox(L""+std::to_wstring(setting.defValue.get<bool>()));

				}else if(setting.defValue.is_number_integer()) {
//					ShowErrorMsgBox(L""+std::to_wstring(setting.defValue.get<int>()));
				}
				try
				{
					setSettingItemValue(setting);
				} catch (...) {

				}
			}
		}
	}
	LoadSettingsMap();
	initGlobalVariable();
	const std::string pref_pinyin_mode = g_settings_map["pref_pinyin_mode"].stringValue;
	PinyinHelper::changePinyinType(pref_pinyin_mode);
}

static void resetToDefaults(HWND hwnd, const std::vector<std::wstring> &subPages,
							std::vector<SettingItem> &settings2,
							const std::vector<std::vector<HWND>> &hCtrlsByTab) {
	// Reset all settings to their default values in the JSON
	std::string defaultSettingsText = GetSettingsJsonText();
	std::vector<SettingItem> defaultSettings = ParseConfig(defaultSettingsText);

	// Update the UI controls with default values
	for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
		int ctrlIdx = 0;

		for (auto &item: settings2) {
			if (utf8_to_wide(item.subPage) != subPages[tabIdx])
				continue;

			if (item.type == "text" || item.type == "button")
				continue;

			// Find the corresponding default setting
			auto defaultItem = std::find_if(defaultSettings.begin(), defaultSettings.end(),
											[&item](const SettingItem &def) {
												return def.key == item.key;
											});

			if (defaultItem != defaultSettings.end()) {
				HWND hCtrl = hCtrlsByTab[tabIdx][static_cast<size_t>(ctrlIdx + 1)];

				if (item.type == "bool") {
					bool defaultValue = defaultItem->boolValue;
					SetSwitchState(hCtrl, defaultValue);
					item.defValue = defaultItem->defValue;
				} else if (item.type == "string" || item.type == "hotkeystring") {
					std::string defaultValue = defaultItem->stringValue;
					SetWindowTextW(hCtrl, utf8_to_wide(defaultValue).c_str());
					item.defValue = defaultItem->defValue;
				} else if (item.type == "long") {
					int64_t defaultValue = defaultItem->intValue;
					SetWindowTextW(hCtrl, std::to_wstring(defaultValue).c_str());
					item.defValue = defaultItem->defValue;
				} else if (item.type == "list") {
					std::string defaultValue = defaultItem->stringValue;
					for (size_t i = 0; i < item.entryValues.size(); ++i) {
						if (item.entryValues[i] == defaultValue) {
							SendMessage(hCtrl, CB_SETCURSEL, i, 0);
							break;
						}
					}
					item.defValue = defaultItem->defValue;
				} else if (item.type == "stringArr") {
					if (defaultItem->defValue.is_array()) {
						std::wstring defaultText;
						for (const auto &str: defaultItem->defValue) {
							defaultText += utf8_to_wide(str.get<std::string>()) + L"\r\n";
						}
						SetWindowTextW(hCtrl, defaultText.c_str());
						item.defValue = defaultItem->defValue;
					}
				}
			}

			ctrlIdx += 2;
		}
	}

	MessageBoxW(hwnd, L"设置已重置为默认值", L"重置完成", MB_OK | MB_ICONINFORMATION);
}

static void applySettings(HWND hwnd, const std::vector<std::wstring> &subPages,
						  std::vector<SettingItem> &settings2,
						  const std::vector<std::vector<HWND>> &hCtrlsByTab) {
	// Apply settings without closing the window
	saveConfig(hwnd, subPages, settings2, hCtrlsByTab, "user_settings.json");

	// Show a brief confirmation message
	MessageBoxW(hwnd, L"设置已应用", L"应用完成", MB_OK | MB_ICONINFORMATION);
}



static void handleButtonAction(HWND hwnd, const std::string &key) {
	if (key == "pref_open_config_folder") {
		// Open the configuration folder
		std::wstring currentPath = GetCurrentWorkingDirectoryW();
		ShellExecuteW(hwnd, L"open", L"explorer.exe", currentPath.c_str(), nullptr, SW_SHOW);
	} else if (key == "pref_btn_indexed_manager") {
		// This is the indexed manager button (pref_btn_indexed_manager)
		ShowIndexedManagerWindow(hwnd);
	} else if (key == "pref_backup_settings") {
		// Create a backup of current settings
		wchar_t fileName[MAX_PATH];
		SYSTEMTIME st;
		GetLocalTime(&st);
		swprintf_s(fileName, L"settings_backup_%04d%02d%02d_%02d%02d%02d.json",
				   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

		if (CopyFileW(L"user_settings.json", fileName, FALSE)) {
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
			if (CopyFileW(szFile, L"user_settings.json", FALSE)) {
				MessageBoxW(hwnd, L"设置已恢复，请重启程序生效", L"恢复成功", MB_OK | MB_ICONINFORMATION);
			} else {
				MessageBoxW(hwnd, L"恢复失败", L"错误", MB_OK | MB_ICONERROR);
			}
		}
	}
}
