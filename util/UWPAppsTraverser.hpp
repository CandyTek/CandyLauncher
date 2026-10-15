#pragma once

#include <windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <shellapi.h>
#include <filesystem>
#include "../model/TraverseOptions.hpp"
#include "MainTools.hpp"
#include <psapi.h>
#include <atomic>
#include <thread>
#include <vector>
#include "UwpAppsEnumerator.hpp"
#include "UwpHelperClient.hpp"
#pragma comment(lib, "Psapi.lib")


// 索引Windows应用商店应用（UWP应用），废弃的，不生效

template <typename Callback>
static void TraverseUWPApps(
	const TraverseOptions& options,
	Callback&& callback) {
	// UWP应用的包信息存储在注册表中
	HKEY hPackages;
	const std::wstring packagesKey =
		L"SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, packagesKey.c_str(), 0, KEY_READ, &hPackages) != ERROR_SUCCESS) {
		return;
	}

	auto addUWPApp = [&](const std::wstring& displayName, const std::wstring& packageFamilyName,
						const std::wstring& appId = L"") {
		if (displayName.empty() || packageFamilyName.empty()) return;

		std::wstring appName = displayName;

		// 检查排除规则
		if (shouldExclude(options, appName)) return;

		// 应用重命名映射
		if (const auto it = options.renameMap.find(appName); it != options.renameMap.end()) {
			appName = it->second;
		}

		// UWP应用的启动命令格式
		std::wstring uwpCommand = L"shell:AppsFolder\\" + packageFamilyName;
		if (!appId.empty()) {
			uwpCommand += L"!" + appId;
		}
		callback(
			appName, // 逻辑名（被 rename 过）
			uwpCommand
		);
	};

	DWORD index = 0;
	wchar_t packageName[256];
	DWORD packageNameSize = sizeof(packageName) / sizeof(wchar_t);

	// 枚举所有UWP包
	while (RegEnumKeyExW(hPackages, index, packageName, &packageNameSize, nullptr, nullptr, nullptr, nullptr) ==
		ERROR_SUCCESS) {
		HKEY hPackage;
		if (RegOpenKeyExW(hPackages, packageName, 0, KEY_READ, &hPackage) == ERROR_SUCCESS) {
			wchar_t displayName[512] = {0};
			wchar_t packageFamilyName[256] = {0};
			DWORD displayNameSize = sizeof(displayName);
			DWORD packageFamilyNameSize = sizeof(packageFamilyName);

			// 获取显示名称和包族名称
			if (RegQueryValueExW(hPackage, L"DisplayName", nullptr, nullptr, (LPBYTE)displayName, &displayNameSize) ==
				ERROR_SUCCESS &&
				RegQueryValueExW(hPackage, L"PackageFamilyName", nullptr, nullptr, (LPBYTE)packageFamilyName,
								&packageFamilyNameSize) == ERROR_SUCCESS) {
				// 过滤掉系统应用和框架应用
				std::wstring packageStr(packageName);
				if (packageStr.find(L"Microsoft.Windows") == std::wstring::npos &&
					packageStr.find(L"Microsoft.VCLibs") == std::wstring::npos &&
					packageStr.find(L"Microsoft.NET") == std::wstring::npos &&
					packageStr.find(L"Microsoft.UI") == std::wstring::npos) {
					addUWPApp(displayName, packageFamilyName);
				}
			}

			RegCloseKey(hPackage);
		}

		packageNameSize = sizeof(packageName) / sizeof(wchar_t);
		index++;
	}

	RegCloseKey(hPackages);
}


// ---------- 以下为实际生效的 UWP 索引 ----------
//
// 枚举 shell:AppsFolder 会把 windows.storage / Windows.StateRepository / CoreUIComponents 等 WinRT 运行库
// 加载进进程并常驻约 10 MB，所以默认交给短命的 CandyUwpHelper.exe（uwp_helper/UwpHelperMain.cpp）去做，
// 结果连同 48px 图标像素经匿名管道传回（util/UwpHelperClient.hpp / UwpHelperProtocol.hpp）。助手不存在、
// 超时或输出不合法时回退到进程内枚举（UwpApps::Enumerate / LoadIcons，util/UwpAppsEnumerator.hpp），行为与旧版一致。

// 回调签名：(逻辑名(已 rename), "shell:AppsFolder\\<解析名>", 解析名, HBITMAP 图标或 nullptr)；回调方拥有位图。
// 排除与重命名规则在这里按显示名应用，与旧版一致
template <typename Callback>
static void LoadUwpApps(Callback&& callback, const TraverseOptions& options) {
	const ULONGLONG uwpStart = GetTickCount64();

	std::vector<UwpHelper::Item> apps;
	const std::wstring helperExe = FindUwpHelperExe();
	if (!helperExe.empty() && RunUwpHelper(helperExe, UwpHelper::kModeUwp, apps, 30000)) {
		size_t kept = 0, withIcon = 0;
		for (UwpHelper::Item& app : apps) {
			std::wstring uwpAppName = app.name;
			if (shouldExclude(options, uwpAppName)) continue;
			if (const auto it = options.renameMap.find(uwpAppName); it != options.renameMap.end()) {
				uwpAppName = it->second;
			}
			++kept;
			if (!app.icon.empty()) ++withIcon;
			callback(uwpAppName, L"shell:AppsFolder\\" + app.target, app.target,
					UwpApps::CreateDibFromPixels(app.icon));
		}
		Logi(L"UWP", L"helper total=", GetTickCount64() - uwpStart, L"ms apps=", apps.size(), L" kept=", kept,
			L" icons=", withIcon);
		return;
	}
	if (helperExe.empty()) {
		Logw(L"UWP", L"CandyUwpHelper.exe not found next to the app, enumerating in-process");
	} else {
		Logw(L"UWP", L"helper failed, enumerating in-process");
	}

	// ---- 进程内回退 ----
	const HRESULT comResult = CoInitialize(nullptr);
	if (FAILED(comResult)) return;
	size_t enumCount = 0;
	std::vector<std::wstring> names;
	std::vector<std::wstring> parsingNames;
	for (UwpApps::Entry& entry : UwpApps::Enumerate(&enumCount)) {
		if (shouldExclude(options, entry.displayName)) continue;
		if (const auto it = options.renameMap.find(entry.displayName); it != options.renameMap.end()) {
			entry.displayName = it->second;
		}
		names.push_back(std::move(entry.displayName));
		parsingNames.push_back(std::move(entry.parsingName));
	}
	const ULONGLONG enumDone = GetTickCount64();

	std::vector<HBITMAP> bitmaps;
	UwpApps::LoadIcons(parsingNames, bitmaps);
	for (size_t i = 0; i < names.size(); ++i) {
		callback(names[i], L"shell:AppsFolder\\" + parsingNames[i], parsingNames[i], bitmaps[i]);
	}
	Logi(L"UWP", L"in-process total=", GetTickCount64() - uwpStart, L"ms enum=", enumCount, L"/", enumDone - uwpStart,
		L"ms icons=", names.size(), L"/", GetTickCount64() - enumDone, L"ms");
	CoUninitialize();
}
