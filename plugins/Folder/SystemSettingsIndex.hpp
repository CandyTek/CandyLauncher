#pragma once

#include <windows.h>
#include <objbase.h>
#include <unordered_map>
#include <vector>
#include <string>

#include "model/TraverseOptions.hpp"
#include "util/BaseTools.hpp"
#include "util/LogUtil.hpp"
#include "util/SystemSettingsEnumerator.hpp"
#include "util/UwpHelperClient.hpp"

// 控制面板 / Windows 设置索引项。图标三选一：
//   iconBitmap + iconKey：助手进程传回的 48px 位图，iconKey 相同的项共用一份（只有第一项带位图），调用方拥有位图
//   iconPath：由主程序按路径延迟解析（所有 ms-settings 项共用 SystemSettings.exe 的图标）
//   iconIndex：进程内枚举得到的系统图像列表索引（回退路径，以及索引管理器需要 iImage 时）
struct SystemSettingsItem {
	std::wstring name;
	std::wstring target;
	int iconIndex = -1;
	std::wstring iconPath;
	std::wstring iconKey;
	HBITMAP iconBitmap = nullptr;
};

struct SystemSettingsComScope {
	HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	~SystemSettingsComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
	bool available() const { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
};

// 索引管理器的标题用。包限定的 ms-resource 解析会在本进程加载 StateRepository，所以只在用户打开管理器时调用
inline std::wstring LoadSettingsAppName() {
	SystemSettingsComScope com;
	if (!com.available()) return L"";
	return SystemSettings::LoadSettingsAppName();
}

inline int GetSystemSettingsAppIconIndex() {
	SHFILEINFOW info{};
	return SHGetFileInfoW(SystemSettings::SettingsAppExePath().c_str(), 0, &info, sizeof(info), SHGFI_SYSICONINDEX)
		? info.iIcon : -1;
}

// 进程内枚举（回退路径）。会把控制面板 applet / MRT / StateRepository 等 DLL 加载进本进程
inline std::vector<SystemSettingsItem> EnumerateSystemSettingsInProcess(const std::wstring& type) {
	std::vector<SystemSettingsItem> items;
	SystemSettingsComScope com;
	if (!com.available()) return items;
	std::vector<SystemSettings::Item> raw;
	int settingsIconIndex = -1;
	if (type == L"control_panel") {
		SystemSettings::EnumerateControlPanel(raw);
	} else {
		SystemSettings::EnumerateWindowsSettings(raw);
		settingsIconIndex = GetSystemSettingsAppIconIndex();
	}
	items.reserve(raw.size());
	for (auto& entry : raw) {
		SystemSettingsItem item;
		item.name = std::move(entry.name);
		item.target = std::move(entry.target);
		item.iconIndex = type == L"control_panel" ? entry.iconIndex : settingsIconIndex;
		items.push_back(std::move(item));
	}
	return items;
}

// 默认通过 CandyUwpHelper.exe 枚举，枚举用到的几十个 Shell DLL 只在助手进程里加载；
// 助手不存在或失败时回退到进程内枚举
inline std::vector<SystemSettingsItem> EnumerateSystemSettingsViaHelper(const std::wstring& type) {
	const bool controlPanel = type == L"control_panel";
	const wchar_t* mode = controlPanel ? UwpHelper::kModeControlPanel : UwpHelper::kModeWindowsSettings;
	std::vector<UwpHelper::Item> raw;
	const std::wstring helperExe = FindUwpHelperExe();
	if (helperExe.empty() || !RunUwpHelper(helperExe, mode, raw, 30000)) {
		Logw(L"FolderPlugin", helperExe.empty() ? L"CandyUwpHelper.exe not found, enumerating " : L"helper failed, enumerating ",
			type, L" in-process");
		return EnumerateSystemSettingsInProcess(type);
	}
	const std::wstring settingsExe = controlPanel ? L"" : SystemSettings::SettingsAppExePath();
	// 助手只在 key 第一次出现时带像素。这里给每个同 key 的条目各建一份小位图（几十个 48px 图标，
	// 几百 KB），这样排除、去重或标题冲突去掉任意一项都不会让其余项丢图标；主程序按 icon_key 仍只复制一份
	std::unordered_map<std::wstring, const UwpApps::DibPixels*> pixelsByKey;
	for (const auto& entry : raw) {
		if (!entry.iconKey.empty() && !entry.icon.empty()) pixelsByKey.try_emplace(entry.iconKey, &entry.icon);
	}
	std::vector<SystemSettingsItem> items;
	items.reserve(raw.size());
	for (auto& entry : raw) {
		SystemSettingsItem item;
		item.name = std::move(entry.name);
		item.target = std::move(entry.target);
		item.iconPath = settingsExe;
		item.iconKey = entry.iconKey;
		if (const auto pixels = pixelsByKey.find(entry.iconKey); pixels != pixelsByKey.end()) {
			item.iconBitmap = UwpApps::CreateDibFromPixels(*pixels->second);
		} else if (!entry.icon.empty()) {
			item.iconBitmap = UwpApps::CreateDibFromPixels(entry.icon);
		}
		items.push_back(std::move(item));
	}
	return items;
}

// inProcess=true 时强制进程内枚举并返回系统图像列表索引（索引管理器的列表用 iImage 画图标）
inline std::vector<SystemSettingsItem> GetSystemSettingsItems(const TraverseOptions& options, const bool inProcess = false) {
	// Windows 7 没有 "设置" 应用，也不识别 ms-settings: 协议，直接跳过该索引源
	if (options.type != L"control_panel" && !IsWindows8OrGreater()) return {};
	auto items = inProcess ? EnumerateSystemSettingsInProcess(options.type) : EnumerateSystemSettingsViaHelper(options.type);
	std::vector<SystemSettingsItem> result;
	result.reserve(items.size());
	for (auto& item : items) {
		bool excluded = shouldExclude(options, item.name);
		if (!excluded) {
			if (const auto renamed = options.renameMap.find(item.name); renamed != options.renameMap.end())
				item.name = renamed->second;
			excluded = shouldExclude(options, item.name);
		}
		if (excluded) {
			if (item.iconBitmap) DeleteObject(item.iconBitmap);
			continue;
		}
		result.push_back(std::move(item));
	}
	return result;
}
