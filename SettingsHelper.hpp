#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls

#include "BaseTools.hpp"
#include "Resource.h"
#include "Constants.hpp"
#include "ShortCutHelper.hpp"
#include "DataKeeper.hpp"
#include "IndexedManager.hpp"

static std::map<std::wstring, std::wstring> tabNameMap = {
		{L"normal",  L"常规"},
		{L"feature", L"功能"},
		{L"other",   L"其他"},
		{L"hotkey",  L"快捷键"},
		{L"index",   L"索引"},
		{L"about",   L"关于"}
};


//inline std::vector<SettingItem> ParseConfig(const std::string& configStr) {
//    nlohmann::json j = nlohmann::json::parse(configStr);
//    std::vector<SettingItem> settings;
//    for (auto& item : j["prefList"]) {
//        settings.push_back({
//            item["key"].get<std::string>(),
//            item["type"].get<std::string>(),
//            item["subPage"].get<std::string>(),
//            item["defValue"]
//            });
//    }
//    return settings;
//}


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

	std::wstring configPath = LR"(C:\Users\Administrator\source\repos\WindowsProject1\settings.json)";
	FILE *fp = nullptr;
	if (_wfopen_s(&fp, configPath.c_str(), L"rb") != 0 || !fp) {
		std::wcerr << L"设置基础文件不存在：" << configPath << std::endl;
		return GetSettingsJsonText2();
	}

	std::string data;
	constexpr size_t kBuf = 4096;
	std::vector<char> buf(kBuf);
	size_t n;
	while ((n = fread(buf.data(), 1, buf.size(), fp)) > 0) {
		data.append(buf.data(), n);
	}
	fclose(fp);
	// 如果文件是 UTF-8 BOM，去掉 BOM
	if (data.size() >= 3 &&
		static_cast<unsigned char>(data[0]) == 0xEF &&
		static_cast<unsigned char>(data[1]) == 0xBB &&
		static_cast<unsigned char>(data[2]) == 0xBF) {
		data.erase(0, 3);
	}
	return data;
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
	std::ifstream f(filename);
	if (!f.is_open()) {
		// File doesn't exist, probably the first run. Return empty.
		return {};
	}

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


static void saveConfig2(const std::vector<std::wstring> &subPages, const std::vector<SettingItem> &settings2,
						const std::vector<std::vector<HWND>> &hCtrlsByTab) {
	nlohmann::json newConfig;
	newConfig["version"] = 1;
	newConfig["prefList"] = nlohmann::json::array();

	// 遍历每个tab
	for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
		int ctrlIdx = 0;
		for (size_t i = 0; i < settings2.size(); ++i) {
			const auto &item = settings2[i];
			if (utf8_to_wide(item.subPage) != subPages[tabIdx])
				continue;

			nlohmann::json obj;
			obj["key"] = item.key;
			obj["type"] = item.type;
			obj["subPage"] = item.subPage;

			HWND hCtrl = hCtrlsByTab[tabIdx][static_cast<size_t>(ctrlIdx + 1)];
			if (item.type == "bool") {
				BOOL checked = (SendMessage(hCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
				obj["defValue"] = checked;
			} else if (item.type == "string") {
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::wstring ws(buf);
				obj["defValue"] = wide_to_utf8(ws);
			} else if (item.type == "stringArr") {
				wchar_t buf[1024];
				GetWindowTextW(hCtrl, buf, 1024);
				std::wstring ws(buf);
				std::vector<std::string> arr;
				size_t start = 0;
				while (start < ws.size()) {
					size_t end = ws.find_first_of(L"\r\n", start);
					if (end == std::wstring::npos) end = ws.size();
					std::wstring line = ws.substr(start, end - start);
					if (!line.empty())
						arr.push_back(wide_to_utf8(line));
					start = ws.find_first_not_of(L"\r\n", end);
					if (start == std::wstring::npos) break;
				}
				obj["defValue"] = arr;
			} else if (item.type == "list") {
				int selIdx = (int) SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
				std::string val = item.entryValues[selIdx];
				obj["defValue"] = val;
			} else if (item.type == "long") {
				wchar_t buf[32];
				GetWindowTextW(hCtrl, buf, 32);
				obj["defValue"] = _wtol(buf); // 宽字符转 long
			} else if (item.type == "hotkeystring") {
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string hotkeyStr = wide_to_utf8(std::wstring(buf));
				obj["defValue"] = _wtol(buf); // 宽字符转 long
			} else if (item.type == "button") {
				// Button类型保持原始的defValue（action名称）
				obj["defValue"] = item.defValue;
			}

			newConfig["prefList"].push_back(obj);
			ctrlIdx += 2; // hLabel, hCtrl
		}
	}
	// 你可以保存 newConfig.dump(4) 到文件
	std::string temp = newConfig.dump(4);
	ShowErrorMsgBox(temp);
	// MessageBoxW(hwnd, L"保存成功（实际存储代码请补全）", L"提示", MB_OK);
}

static void LoadSettingsMap() {
	settingsMap.clear();
	for (const auto &item: settings2) {
		settingsMap[item.key] = item;
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
				BOOL value = (SendMessage(hCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
				newConfig[key] = value;
				item.defValue = nlohmann::json(value); // ADDED: Update the setting's default value
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
	PostMessage(s_mainHwnd, WM_CONFIG_SAVED, 1, 0);
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
					bool defaultValue = defaultItem->defValue.get<bool>();
					SendMessage(hCtrl, BM_SETCHECK, defaultValue ? BST_CHECKED : BST_UNCHECKED, 0);
					item.defValue = defaultItem->defValue;
				} else if (item.type == "string" || item.type == "hotkeystring") {
					std::string defaultValue = defaultItem->defValue.get<std::string>();
					SetWindowTextW(hCtrl, utf8_to_wide(defaultValue).c_str());
					item.defValue = defaultItem->defValue;
				} else if (item.type == "long") {
					long defaultValue = defaultItem->defValue.get<long>();
					SetWindowTextW(hCtrl, std::to_wstring(defaultValue).c_str());
					item.defValue = defaultItem->defValue;
				} else if (item.type == "list") {
					std::string defaultValue = defaultItem->defValue.get<std::string>();
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

static std::wstring GetCurrentDirectoryW() {
	wchar_t buffer[MAX_PATH];
	DWORD result = ::GetCurrentDirectoryW(MAX_PATH, buffer);
	if (result > 0 && result <= MAX_PATH) {
		return std::wstring(buffer);
	}
	return L".";
}


static void handleButtonAction(HWND hwnd, const std::string &action) {
	if (action == "open_config_folder") {
		// Open the configuration folder
		std::wstring currentPath = GetCurrentDirectoryW();
		ShellExecuteW(hwnd, L"open", L"explorer.exe", currentPath.c_str(), nullptr, SW_SHOW);
	} else if (action.empty()) {
		// This is the indexed manager button (pref_btn_indexed_manager)
		ShowIndexedManagerWindow(hwnd);
	} else if (action == "backup_settings") {
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
	} else if (action == "restore_settings") {
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
