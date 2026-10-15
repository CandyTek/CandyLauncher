#pragma once

// 控制面板 / Windows 设置项枚举的最小实现，不依赖主程序的任何全局状态，
// 同时被独立进程 CandyUwpHelper.exe（uwp_helper/UwpHelperMain.cpp）和 Folder 插件的进程内回退路径
// （plugins/Folder/SystemSettingsIndex.hpp）编译。
//
// 枚举控制面板命名空间会把 appwiz.cpl / printui / fontext / 设备与网络提供者等 40 多个 DLL 拉进进程并常驻
// 约 12 MB；解析 "设置" 应用名与 ms-resource 字符串又会加载 MRT（mrmcorer、coreuicomponents、
// windows.staterepositorycore 等）。所以默认交给短命的助手进程去做，结果经管道传回主程序。

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <propkey.h>

#include <cwctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace SystemSettings {

struct Item {
	std::wstring name;   // 显示名
	std::wstring target; // ShellExecute 可打开的目标：控制面板解析名 "::{...}\..." 或 "ms-settings:xxx"
	int iconIndex = -1;  // 系统图像列表索引，只在枚举进程内有效
};

// "设置" 应用主程序路径，主程序据此延迟解析所有 ms-settings 项共用的图标
inline std::wstring SettingsAppExePath() {
	wchar_t windowsPath[MAX_PATH]{};
	if (!GetWindowsDirectoryW(windowsPath, MAX_PATH)) return L"";
	return std::wstring(windowsPath) + L"\\ImmersiveControlPanel\\SystemSettings.exe";
}

inline int ControlPanelIconIndex() {
	wchar_t windowsPath[MAX_PATH]{};
	if (!GetWindowsDirectoryW(windowsPath, MAX_PATH)) return -1;
	const std::wstring exe = std::wstring(windowsPath) + L"\\System32\\control.exe";
	SHFILEINFOW info{};
	return SHGetFileInfoW(exe.c_str(), 0, &info, sizeof(info), SHGFI_SYSICONINDEX) ? info.iIcon : -1;
}

// 读取 "设置" 应用 ms-resource 字符串。加载 MRT 运行库
inline std::wstring LoadSettingsMuiString(const std::wstring& resourceKey) {
	const std::wstring indirect = L"@{windows?ms-resource://Windows.UI.SettingsAppThreshold/SearchResources/" +
		resourceKey + L"/Description}";
	wchar_t buffer[512]{};
	return SUCCEEDED(SHLoadIndirectString(indirect.c_str(), buffer, 512, nullptr)) ? buffer : L"";
}

// 本地化的 "设置" 应用名。包限定的 ms-resource 解析会加载 windows.staterepositorycore.dll
inline std::wstring LoadSettingsAppName() {
	wchar_t indirect[512]{};
	DWORD size = sizeof(indirect);
	if (RegGetValueW(HKEY_CLASSES_ROOT, L"ms-settings\\Application", L"ApplicationName",
		RRF_RT_REG_SZ, nullptr, indirect, &size) != ERROR_SUCCESS) return L"";
	wchar_t name[512]{};
	return SUCCEEDED(SHLoadIndirectString(indirect, name, 512, nullptr)) ? name : L"";
}

// 检查是否属于需要排除的文件夹（字体、程序与功能、开始菜单）。调用线程须已初始化 COM
inline bool IsFolderBlacklisted(const std::wstring& parsingName, PCIDLIST_ABSOLUTE folderId) {
	std::wstring lower = parsingName;
	for (auto& ch : lower) ch = static_cast<wchar_t>(std::towlower(ch));
	if (lower.find(L"7b81be6a-ce2b-4676-a29e-eb907a5126c5") != std::wstring::npos ||
		lower.find(L"programdata\\microsoft\\windows\\start menu") != std::wstring::npos ||
		lower.find(L"a8a91a66-3a7d-4424-8d24-04e180695c7a") != std::wstring::npos) {
		return true;
	}
	// 控制面板中的字体项可能以本地化名称出现在解析路径中，使用不受语言影响的规范名称
	if (folderId) {
		IShellItem2* shellItem = nullptr;
		if (SUCCEEDED(SHCreateItemFromIDList(folderId, IID_PPV_ARGS(&shellItem)))) {
			PWSTR canonicalName = nullptr;
			bool isFonts = false;
			if (SUCCEEDED(shellItem->GetString(PKEY_ApplicationName, &canonicalName))) {
				isFonts = canonicalName && _wcsicmp(canonicalName, L"Microsoft.Fonts") == 0;
				CoTaskMemFree(canonicalName);
			}
			shellItem->Release();
			if (isFonts) return true;
		}
	}
	return false;
}

// 按 PIDL 提取 size x size 的 32bpp 图标 DIB；失败返回 nullptr，调用方拥有位图
inline HBITMAP ExtractItemIcon(PCIDLIST_ABSOLUTE itemId, const int size) {
	IShellItemImageFactory* factory = nullptr;
	if (FAILED(SHCreateItemFromIDList(itemId, IID_PPV_ARGS(&factory)))) return nullptr;
	HBITMAP bitmap = nullptr;
	// SIIGBF_RESIZETOFIT 保证即使没有精确尺寸也能拿到图像；SIIGBF_ICONONLY 避免拿到缩略图
	if (FAILED(factory->GetImage({size, size}, SIIGBF_RESIZETOFIT | SIIGBF_ICONONLY, &bitmap))) bitmap = nullptr;
	factory->Release();
	return bitmap;
}

namespace detail {

// 控制面板项目常带 SFGAO_FOLDER；先索引项目本身，再枚举其子项目
inline void EnumerateControlPanelFolder(IShellFolder* folder, PCIDLIST_ABSOLUTE parentId, const int fallbackIconIndex,
										std::vector<Item>& items, std::unordered_map<int, HBITMAP>* icons,
										const int iconSize) {
	if (!folder || !parentId) return;
	IEnumIDList* enumerator = nullptr;
	const DWORD flags = SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN;
	if (FAILED(folder->EnumObjects(nullptr, flags, &enumerator)) || !enumerator) return;

	PITEMID_CHILD child = nullptr;
	while (enumerator->Next(1, &child, nullptr) == S_OK) {
		PIDLIST_ABSOLUTE absoluteId = ILCombine(parentId, child);
		if (absoluteId) {
			PWSTR target = nullptr;
			if (SUCCEEDED(SHGetNameFromIDList(absoluteId, SIGDN_DESKTOPABSOLUTEPARSING, &target)) && target) {
				SFGAOF attributes = SFGAO_FOLDER;
				LPCITEMIDLIST childList = child;
				const bool isFolder = SUCCEEDED(folder->GetAttributesOf(1, &childList, &attributes)) &&
					(attributes & SFGAO_FOLDER);

				STRRET displayName{};
				wchar_t name[MAX_PATH]{};
				if (SUCCEEDED(folder->GetDisplayNameOf(child, SHGDN_NORMAL, &displayName)) &&
					SUCCEEDED(StrRetToBufW(&displayName, child, name, MAX_PATH)) && name[0]) {
					SHFILEINFOW iconInfo{};
					const int iconIndex = SHGetFileInfoW(reinterpret_cast<LPCWSTR>(absoluteId), 0,
						&iconInfo, sizeof(iconInfo), SHGFI_PIDL | SHGFI_SYSICONINDEX)
						? iconInfo.iIcon : fallbackIconIndex;
					// 同一图像列表索引只提取一次像素
					if (icons && icons->find(iconIndex) == icons->end()) {
						icons->emplace(iconIndex, ExtractItemIcon(absoluteId, iconSize));
					}
					items.push_back({name, target, iconIndex});
				}

				// 父项目保留在索引中；黑名单只阻止枚举其子项
				if (isFolder && !IsFolderBlacklisted(target, absoluteId)) {
					IShellFolder* subFolder = nullptr;
					if (SUCCEEDED(folder->BindToObject(child, nullptr, IID_PPV_ARGS(&subFolder)))) {
						EnumerateControlPanelFolder(subFolder, absoluteId, fallbackIconIndex, items, icons, iconSize);
						subFolder->Release();
					}
				}
			}
			CoTaskMemFree(target);
			ILFree(absoluteId);
		}
		CoTaskMemFree(child);
	}
	enumerator->Release();
}

} // namespace detail

// 枚举控制面板全部项目。调用线程须已初始化 COM。
// icons 非空时同时按 iconIndex 提取一份 iconSize 像素的 32bpp 图标（可能为 nullptr），调用方拥有这些位图
inline void EnumerateControlPanel(std::vector<Item>& items, std::unordered_map<int, HBITMAP>* icons = nullptr,
								const int iconSize = 48) {
	const int fallbackIconIndex = ControlPanelIconIndex();
	IShellFolder* desktop = nullptr;
	if (FAILED(SHGetDesktopFolder(&desktop))) return;
	PIDLIST_ABSOLUTE rootId = nullptr;
	if (SUCCEEDED(SHGetKnownFolderIDList(FOLDERID_ControlPanelFolder, 0, nullptr, &rootId))) {
		IShellFolder* rootFolder = nullptr;
		if (SUCCEEDED(desktop->BindToObject(rootId, nullptr, IID_PPV_ARGS(&rootFolder)))) {
			detail::EnumerateControlPanelFolder(rootFolder, rootId, fallbackIconIndex, items, icons, iconSize);
			rootFolder->Release();
		}
		CoTaskMemFree(rootId);
	}
	desktop->Release();
}

// 枚举 "设置" 应用的常用页面（固定列表，名称来自系统 MUI 资源）。第一项是 "设置" 应用本身。
// iconIndex 由调用方决定（主程序侧按 SettingsAppExePath() 延迟解析），这里一律为 -1
inline void EnumerateWindowsSettings(std::vector<Item>& items) {
	struct Page { const wchar_t* uri; const wchar_t* resource; };
	static constexpr Page pages[] = {
		{L"display", L"SettingsPagePCSystemDisplay"},
		{L"sound", L"SettingsPageAudio"},
		{L"notifications", L"SettingsPageAppsNotifications"},
		{L"powersleep", L"SettingsPageScreenPowerAndSleep"},
		{L"storagesense", L"SettingsPageStorageSenseStorageOverview"},
		{L"clipboard", L"SettingsPageClipboard"},
		{L"multitasking", L"SettingsPageMultiTasking"},
		{L"about", L"SettingsPagePCSystemInfo"},
		{L"bluetooth", L"SettingsPagePCSystemBluetooth"},
		{L"printers", L"SettingsPageDevicesPrinters"},
		{L"mousetouchpad", L"SettingsPagePCSystemDeviceSettings"},
		{L"typing", L"SettingsPageTimeRegionSpelling"},
		{L"appsfeatures", L"SettingsPageAppsSizes"},
		{L"defaultapps", L"SettingsPageAppsDefaults"},
		{L"startupapps", L"SettingsPageStartup"},
		{L"network-status", L"SettingsPageNetworkStatus"},
		{L"network-wifi", L"SettingsPageNetworkWiFi"},
		{L"network-ethernet", L"SettingsPageNetworkEthernet"},
		{L"network-vpn", L"SettingsPageNetworkVPN"},
		{L"network-proxy", L"SettingsPageNetworkProxy"},
		{L"personalization-background", L"SettingsPageBackground"},
		{L"colors", L"SettingsPageColors-2"},
		{L"lockscreen", L"SettingsPageLockScreen"},
		{L"themes", L"SettingsPageThemes"},
		{L"personalization-start", L"SettingsPageStart"},
		{L"taskbar", L"SettingsPageTaskbar"},
		{L"yourinfo", L"SettingsPageAccountsPicture-2"},
		{L"signinoptions", L"SettingsPageSignInOptions"},
		{L"otherusers", L"SettingsPageOtherUsers"},
		{L"regionlanguage", L"SettingsPageTimeRegionLanguage"},
		{L"dateandtime", L"SettingsPageTimeRegionDateTime"},
		{L"regionformatting", L"SettingsPageTimeRegionRegion"},
		{L"gaming-gamebar", L"SettingsPageGameBar"},
		{L"gaming-gamemode", L"SettingsPageGameMode"},
		{L"easeofaccess-display", L"SettingsPageEaseOfAccessDisplay"},
		{L"privacy", L"SettingsPagePrivacyGeneral"},
		{L"privacy-location", L"SettingsPagePrivacyLocation"},
		{L"privacy-webcam", L"SettingsPagePrivacyWebcam"},
		{L"privacy-microphone", L"SettingsPagePrivacyMicrophone"},
		{L"windowsupdate", L"SettingsPageRestoreMusUpdate"},
		{L"backup", L"SettingsPageRestoreOneBackup"},
		{L"recovery", L"SettingsPageRestoreRestore"},
		{L"activation", L"SettingsPageActivate"},
		{L"windowsdefender", L"SettingsPageWindowsDefender"}
	};
	std::wstring appName = LoadSettingsAppName();
	if (appName.empty()) appName = L"ms-settings:";
	items.push_back({std::move(appName), L"ms-settings:", -1});
	for (const auto& page : pages) {
		std::wstring name = LoadSettingsMuiString(page.resource);
		if (name.empty()) name = page.uri;
		items.push_back({std::move(name), std::wstring(L"ms-settings:") + page.uri, -1});
	}
}

} // namespace SystemSettings
