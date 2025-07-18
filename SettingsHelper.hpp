#pragma once

#include <fstream> // Required for file operations

#include "BaseTools.hpp"
#include "Resource.h"
#include "Constants.hpp"
#include "ShortCutHelper.hpp"
#include "DataKeeper.hpp"

static std::map<std::wstring, std::wstring> tabNameMap = {
	{L"normal", L"常规"},
	{L"feature", L"功能"},
	{L"other", L"其他"},
	{L"hotkey", L"快捷键"},
	{L"index", L"索引"},
	{L"about", L"关于"}
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


inline std::vector<SettingItem> ParseConfig(const std::string& configStr)
{
	nlohmann::json j = nlohmann::json::parse(configStr);
	std::vector<SettingItem> settings;

	for (auto& item : j["prefList"])
	{
		SettingItem setting;
		setting.key = item["key"].get<std::string>();
		setting.type = item["type"].get<std::string>();
		setting.title = item["title"].get<std::string>();
		setting.subPage = item["subPage"].get<std::string>();

		// 如果存在 entries，就解析；否则设为空
		if (item.contains("entries"))
		{
			setting.entries = item["entries"].get<std::vector<std::string>>();
		}
		else
		{
			setting.entries = {};
		}

		if (item.contains("entryValues"))
		{
			setting.entryValues = item["entryValues"].get<std::vector<std::string>>();
		}
		else
		{
			setting.entryValues = {};
		}

		setting.defValue = item["defValue"]; // 保持原始 JSON 类型，灵活性更高

		settings.push_back(setting);
	}

	return settings;
}


static std::string GetSettingsJsonText2()
{
	HMODULE hModule = GetModuleHandleW(nullptr);

	HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_SETTINGS_JSON), RT_RCDATA);
	if (!hRes)
		throw std::runtime_error("Resource not found");

	HGLOBAL hResLoad = LoadResource(hModule, hRes);
	if (!hResLoad)
		throw std::runtime_error("LoadResource failed");

	DWORD size = SizeofResource(hModule, hRes);
	const void* pData = LockResource(hResLoad);
	if (!pData)
		throw std::runtime_error("LockResource failed");

	std::string settingsText(static_cast<const char*>(pData), size);
	return settingsText;
}


#ifndef NDEBUG
static std::string GetSettingsJsonText()
{
	std::wstring configPath = LR"(C:\Users\Administrator\source\repos\WindowsProject1\settings.json)";
	std::ifstream in((configPath.data())); // 用 std::ifstream 而不是 std::wifstream
	if (!in)
	{
		std::wcerr << L"设置基础文件不存在：" << configPath << std::endl;
		// throw std::runtime_error("设置基础文件不存在");
		// exit(1);
		return GetSettingsJsonText2();
	}

	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string settingsText = buffer.str();
	//ShowErrorMsgBox(settingsText);
	return settingsText;
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
static nlohmann::json loadUserConfig(const std::string& filename = "user_settings.json")
{
	std::ifstream f(filename);
	if (!f.is_open())
	{
		// File doesn't exist, probably the first run. Return empty.
		return {};
	}

	nlohmann::json userConfig;
	try
	{
		// Parse the file content into the JSON object.
		userConfig = nlohmann::json::parse(f);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		// The file is corrupted or not valid JSON.
		std::string error_msg = "Config file load error in '" + filename +
			"'. Using default settings.\n\nError: " + e.what();
		MessageBoxA(nullptr, error_msg.c_str(), "Config Load Error", MB_OK | MB_ICONWARNING);
		return {}; // Return empty to fall back to defaults.
	}
	return userConfig;
}


static void saveConfig2(const std::vector<std::wstring>& subPages, const std::vector<SettingItem>& settings2,
						const std::vector<std::vector<HWND>>& hCtrlsByTab)
{
	nlohmann::json newConfig;
	newConfig["version"] = 1;
	newConfig["prefList"] = nlohmann::json::array();

	// 遍历每个tab
	for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx)
	{
		int ctrlIdx = 0;
		for (size_t i = 0; i < settings2.size(); ++i)
		{
			const auto& item = settings2[i];
			if (utf8_to_wide(item.subPage) != subPages[tabIdx])
				continue;

			nlohmann::json obj;
			obj["key"] = item.key;
			obj["type"] = item.type;
			obj["subPage"] = item.subPage;

			HWND hCtrl = hCtrlsByTab[tabIdx][static_cast<size_t>(ctrlIdx + 1)];
			if (item.type == "bool")
			{
				BOOL checked = (SendMessage(hCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
				obj["defValue"] = checked;
			}
			else if (item.type == "string")
			{
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::wstring ws(buf);
				obj["defValue"] = wide_to_utf8(ws);
			}
			else if (item.type == "stringArr")
			{
				wchar_t buf[1024];
				GetWindowTextW(hCtrl, buf, 1024);
				std::wstring ws(buf);
				std::vector<std::string> arr;
				size_t start = 0;
				while (start < ws.size())
				{
					size_t end = ws.find_first_of(L"\r\n", start);
					if (end == std::wstring::npos) end = ws.size();
					std::wstring line = ws.substr(start, end - start);
					if (!line.empty())
						arr.push_back(wide_to_utf8(line));
					start = ws.find_first_not_of(L"\r\n", end);
					if (start == std::wstring::npos) break;
				}
				obj["defValue"] = arr;
			}
			else if (item.type == "list")
			{
				int selIdx = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
				std::string val = item.entryValues[selIdx];
				obj["defValue"] = val;
			}
			else if (item.type == "long")
			{
				wchar_t buf[32];
				GetWindowTextW(hCtrl, buf, 32);
				obj["defValue"] = _wtol(buf); // 宽字符转 long
			}
			else if (item.type == "hotkeystring")
			{
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string hotkeyStr = wide_to_utf8(std::wstring(buf));
				obj["defValue"] = _wtol(buf); // 宽字符转 long
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

static void LoadSettingsMap()
{
	settingsMap.clear();
	for (const auto& item : settings2)
	{
		settingsMap[item.key] = item;
	}
}


static void saveConfig(HWND hwnd, const std::vector<std::wstring>& subPages, std::vector<SettingItem>& settings2,
						const std::vector<std::vector<HWND>>& hCtrlsByTab,
						const std::string& filename = "user_settings.json")
{
	// The final JSON object will be a simple key-value map.
	nlohmann::json newConfig;

	// Iterate over each tab to process its controls
	for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx)
	{
		// ctrlIdx tracks the position in the hCtrlsByTab[tabIdx] vector.
		// It increments by 2 for each setting (label + control).
		int ctrlIdx = 0;

		// Iterate through all setting definitions to find those for the current tab
		for (auto& item : settings2)
		{
			// Skip if the setting does not belong to the current tab
			if (utf8_to_wide(item.subPage) != subPages[tabIdx])
				continue;

			// "text" types are for display only and have no value to save.
			// They also don't have a corresponding value control, so we skip them.
			if (item.type == "text")
			{
				continue;
			}

			// Retrieve the handle for the actual input control.
			// hCtrlsByTab stores HWNDs in pairs: [hLabel1, hCtrl1, hLabel2, hCtrl2, ...]
			HWND hCtrl = hCtrlsByTab[tabIdx][static_cast<size_t>(ctrlIdx + 1)];
			const std::string& key = item.key;

			if (item.type == "bool")
			{
				BOOL value = (SendMessage(hCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED);
				newConfig[key] = value;
				item.defValue = nlohmann::json(value); // ADDED: Update the setting's default value
			}
			else if (item.type == "string")
			{
				wchar_t buf[256];
				GetWindowTextW(hCtrl, buf, 256);
				std::string value = wide_to_utf8(std::wstring(buf));
				newConfig[key] = value;
				item.defValue = nlohmann::json(value);
			}
			else if (item.type == "stringArr")
			{
				int textLen = GetWindowTextLengthW(hCtrl);
				if (textLen > 0)
				{
					std::vector<wchar_t> buf(textLen + 1);
					GetWindowTextW(hCtrl, &buf[0], textLen + 1);
					std::wstring ws(&buf[0]);

					std::vector<std::string> arr;
					size_t start = 0;
					while (start < ws.size())
					{
						size_t end = ws.find_first_of(L"\r\n", start);
						if (end == std::wstring::npos) end = ws.size();

						std::wstring line = ws.substr(start, end - start);
						if (!line.empty())
						{
							arr.push_back(wide_to_utf8(line));
						}

						start = ws.find_first_not_of(L"\r\n", end);
						if (start == std::wstring::npos) break;
					}
					newConfig[key] = arr;
					item.defValue = nlohmann::json(arr);
				}
				else
				{
					// If the text box is empty, save an empty array
					nlohmann::json empty_array = nlohmann::json::array();
					newConfig[key] = empty_array;
					item.defValue = empty_array;
				}
			}
			else if (item.type == "list")
			{
				int selIdx = (int)SendMessage(hCtrl, CB_GETCURSEL, 0, 0);
				if (selIdx != CB_ERR && static_cast<size_t>(selIdx) < item.entryValues.size())
				{
					// Save the underlying value, not the displayed text
					const std::string& value = item.entryValues[selIdx];
					newConfig[key] = value;
					item.defValue = nlohmann::json(value);
				}
			}
			else if (item.type == "long")
			{
				wchar_t buf[32];
				GetWindowTextW(hCtrl, buf, 32);
				long value = _wtol(buf);
				newConfig[key] = value;
				item.defValue = nlohmann::json(value);
			}
			else if (item.type == "hotkeystring")
			{
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
	ShowWindow(g_settingsHwnd,SW_MINIMIZE);
	PostMessage(s_mainHwnd, WM_CONFIG_SAVED, 1, 0);
}
