#pragma once

#include <ShlObj.h>
#include <propkey.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <vector>
#include <string>

#include "model/TraverseOptions.hpp"

struct SystemSettingsItem {
	std::wstring name;
	std::wstring target;
	int iconIndex = -1;
};

struct SystemSettingsComScope {
	HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	~SystemSettingsComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
	bool available() const { return SUCCEEDED(result) || result == RPC_E_CHANGED_MODE; }
};

inline int GetSystemSettingsAppIconIndex() {
	wchar_t windowsPath[MAX_PATH]{};
	if (!GetWindowsDirectoryW(windowsPath, MAX_PATH)) return -1;
	const std::wstring exe = std::wstring(windowsPath) + L"\\ImmersiveControlPanel\\SystemSettings.exe";
	SHFILEINFOW info{};
	return SHGetFileInfoW(exe.c_str(), 0, &info, sizeof(info), SHGFI_SYSICONINDEX) ? info.iIcon : -1;
}

inline int GetControlPanelIconIndex() {
	wchar_t windowsPath[MAX_PATH]{};
	if (!GetWindowsDirectoryW(windowsPath, MAX_PATH)) return -1;
	const std::wstring exe = std::wstring(windowsPath) + L"\\System32\\control.exe";
	SHFILEINFOW info{};
	return SHGetFileInfoW(exe.c_str(), 0, &info, sizeof(info), SHGFI_SYSICONINDEX) ? info.iIcon : -1;
}

inline std::wstring LoadSettingsMuiString(const std::wstring& resourceKey) {
	const std::wstring indirect = L"@{windows?ms-resource://Windows.UI.SettingsAppThreshold/SearchResources/" +
		resourceKey + L"/Description}";
	wchar_t buffer[512]{};
	return SUCCEEDED(SHLoadIndirectString(indirect.c_str(), buffer, 512, nullptr)) ? buffer : L"";
}

inline std::wstring LoadSettingsAppName() {
	SystemSettingsComScope com;
	if (!com.available()) return L"";
	wchar_t indirect[512]{};
	DWORD size = sizeof(indirect);
	if (RegGetValueW(HKEY_CLASSES_ROOT, L"ms-settings\\Application", L"ApplicationName",
		RRF_RT_REG_SZ, nullptr, indirect, &size) != ERROR_SUCCESS) return L"";
	wchar_t name[512]{};
	return SUCCEEDED(SHLoadIndirectString(indirect, name, 512, nullptr)) ? name : L"";
}

// 检查是否属于需要排除的文件夹（字体、程序与功能）
inline bool IsFolderBlacklisted(const std::wstring& parsingName, PCIDLIST_ABSOLUTE folderId) {
	std::wstring lower = MyToLower(parsingName);
	// 排除设备、程序、字体、开始菜单
	if (
		// lower.find(L"ddf6456e-1c73-4311-a50d-92109f40a61b") != std::wstring::npos ||
		// lower.find(L"93412589-74d4-4e4e-ad0e-e0cb621440fd") != std::wstring::npos ||
		lower.find(L"7b81be6a-ce2b-4676-a29e-eb907a5126c5") != std::wstring::npos ||
		lower.find(L"programdata\\microsoft\\windows\\start menu") != std::wstring::npos ||
		lower.find(L"a8a91a66-3a7d-4424-8d24-04e180695c7a") != std::wstring::npos
		) {
		return true;
	}
	// 控制面板中的字体项可能以本地化名称出现在解析路径中，使用不受语言影响的规范名称。
	if (folderId) {
		Microsoft::WRL::ComPtr<IShellItem2> shellItem;
		if (SUCCEEDED(SHCreateItemFromIDList(folderId, IID_PPV_ARGS(&shellItem)))) {
			PWSTR canonicalName = nullptr;
			if (SUCCEEDED(shellItem->GetString(PKEY_ApplicationName, &canonicalName))) {
				const bool isFonts = canonicalName && _wcsicmp(canonicalName, L"Microsoft.Fonts") == 0;
				CoTaskMemFree(canonicalName);
				if (isFonts) return true;
			}
		}
	}
	return false;
}

// 控制面板项目常带 SFGAO_FOLDER；先索引项目本身，再枚举其子项目。
static void EnumerateControlPanelFolderRecursive(
	IShellFolder* folder,
	PCIDLIST_ABSOLUTE parentId,
	int fallbackIconIndex,
	std::vector<SystemSettingsItem>& items)
	{
	if (!folder || !parentId) return;

	Microsoft::WRL::ComPtr<IEnumIDList> enumerator;
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
					items.push_back({name, target, iconIndex});
				}

				// 父项目保留在索引中；黑名单只阻止枚举其子项。
				if (isFolder && !IsFolderBlacklisted(target, absoluteId)) {
					Microsoft::WRL::ComPtr<IShellFolder> subFolder;
					if (SUCCEEDED(folder->BindToObject(child, nullptr, IID_PPV_ARGS(&subFolder)))) {
						EnumerateControlPanelFolderRecursive(subFolder.Get(), absoluteId, fallbackIconIndex, items);
					}
				}
			}
			CoTaskMemFree(target);
			ILFree(absoluteId);
		}
		CoTaskMemFree(child);
	}
}

// 主入口函数
inline std::vector<SystemSettingsItem> EnumerateControlPanelItems() {
    std::vector<SystemSettingsItem> items;
    SystemSettingsComScope com;
    if (!com.available()) return items;

    const int fallbackIconIndex = GetControlPanelIconIndex();

    Microsoft::WRL::ComPtr<IShellFolder> desktop;
    if (FAILED(SHGetDesktopFolder(&desktop))) return items;

    PIDLIST_ABSOLUTE rootId = nullptr;
    if (FAILED(SHGetKnownFolderIDList(FOLDERID_ControlPanelFolder, 0, nullptr, &rootId))) {
        return items;
    }

    Microsoft::WRL::ComPtr<IShellFolder> rootFolder;
    if (SUCCEEDED(desktop->BindToObject(rootId, nullptr, IID_PPV_ARGS(&rootFolder)))) {
        EnumerateControlPanelFolderRecursive(rootFolder.Get(), rootId, fallbackIconIndex, items);
    }

    CoTaskMemFree(rootId);
    return items;
}

inline std::vector<SystemSettingsItem> EnumerateWindowsSettingsItems() {
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
	SystemSettingsComScope com;
	std::vector<SystemSettingsItem> items;
	if (!com.available()) return items;
	const int iconIndex = GetSystemSettingsAppIconIndex();
	std::wstring appName = LoadSettingsAppName();
	if (appName.empty()) appName = L"ms-settings:";
	items.push_back({std::move(appName), L"ms-settings:", iconIndex});
	for (const auto& page : pages) {
		std::wstring name = LoadSettingsMuiString(page.resource);
		if (name.empty()) name = page.uri;
		items.push_back({std::move(name), std::wstring(L"ms-settings:") + page.uri, iconIndex});
	}
	return items;
}

inline std::vector<SystemSettingsItem> GetSystemSettingsItems(const TraverseOptions& options) {
	auto items = options.type == L"control_panel" ? EnumerateControlPanelItems() : EnumerateWindowsSettingsItems();
	std::vector<SystemSettingsItem> result;
	for (auto& item : items) {
		if (shouldExclude(options, item.name)) continue;
		if (const auto renamed = options.renameMap.find(item.name); renamed != options.renameMap.end())
			item.name = renamed->second;
		if (shouldExclude(options, item.name)) continue;
		result.push_back(std::move(item));
	}
	return result;
}
