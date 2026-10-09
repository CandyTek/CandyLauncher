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


// 在独立 STA 线程中按解析名提取 UWP 图标；Shell 图标提取较慢，串行执行是 UWP 索引的主要耗时。
static void LoadUwpAppIcons(const std::vector<std::wstring>& parsingNames, std::vector<HBITMAP>& bitmaps) {
	bitmaps.assign(parsingNames.size(), nullptr);
	if (parsingNames.empty()) return;
	// 实测 8 线程最快；更多线程会在 Shell 内部锁上竞争而变慢。
	const size_t threadCount = std::min<size_t>(8, parsingNames.size());
	std::atomic<size_t> next{0};
	auto worker = [&]() {
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		for (size_t i = next++; i < parsingNames.size(); i = next++) {
			IShellItemImageFactory* imageFactory = nullptr;
			const std::wstring itemPath = L"shell:AppsFolder\\" + parsingNames[i];
			if (FAILED(SHCreateItemFromParsingName(itemPath.c_str(), nullptr, IID_PPV_ARGS(&imageFactory)))) continue;
			// SIIGBF_RESIZETOFIT 即使没有确切的尺寸，也可以确保我们获得图像；SIIGBF_ICONONLY 防止获得缩略图预览。
			imageFactory->GetImage({48, 48}, SIIGBF_RESIZETOFIT | SIIGBF_ICONONLY, &bitmaps[i]);
			imageFactory->Release();
		}
		if (SUCCEEDED(comResult)) CoUninitialize();
	};
	std::vector<std::thread> threads;
	threads.reserve(threadCount - 1);
	for (size_t i = 1; i < threadCount; ++i) threads.emplace_back(worker);
	worker();
	for (auto& thread : threads) thread.join();
}

/// <summary>
/// Enumerates UWP applications from the AppsFolder and adds them to the actions list.
/// This is the C++ equivalent of the C# SpecificallyForGetCurrentUwpName2() and the subsequent loop.
/// </summary>
/// <param name="actions">The list of actions to add UWP apps to.</param>
template <typename Callback>
static void LoadUwpApps(Callback&& callback, const TraverseOptions& options) {
	const HRESULT comResult = CoInitialize(NULL);
	if (FAILED(comResult)) {
		return;
	}
	const ULONGLONG uwpStart = GetTickCount64();
	size_t enumCount = 0;
	std::vector<std::wstring> names;
	std::vector<std::wstring> parsingNames;

	IShellItem* pAppsFolderItem = nullptr;
	if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(&pAppsFolderItem)))) {
		IShellFolder* pAppsFolder = nullptr;
		if (SUCCEEDED(pAppsFolderItem->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&pAppsFolder)))) {
			IEnumIDList* pEnumIDList = nullptr;
			if (pAppsFolder->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList) == S_OK) {
				PITEMID_CHILD pidlItem = nullptr;
				while (pEnumIDList->Next(1, &pidlItem, nullptr) == S_OK) {
					++enumCount;
					// 直接从文件夹读取名称，避免为每个非 UWP 条目创建 ShellItem。
					STRRET parsingRet{}, displayRet{};
					LPWSTR pwszParsingName = nullptr;
					LPWSTR pwszDisplayName = nullptr;
					if (SUCCEEDED(pAppsFolder->GetDisplayNameOf(pidlItem, SHGDN_FORPARSING, &parsingRet)) &&
						SUCCEEDED(StrRetToStrW(&parsingRet, pidlItem, &pwszParsingName)) &&
						// UWP应用程序通常具有“！”以他们的解析名称。
						wcschr(pwszParsingName, L'!') != nullptr &&
						SUCCEEDED(pAppsFolder->GetDisplayNameOf(pidlItem, SHGDN_NORMAL, &displayRet)) &&
						SUCCEEDED(StrRetToStrW(&displayRet, pidlItem, &pwszDisplayName))) {
						// 进行UWP列表项的筛选，排除，重命名
						std::wstring uwpAppName = pwszDisplayName;
						if (!shouldExclude(options, uwpAppName)) {
							if (const auto it = options.renameMap.find(uwpAppName); it != options.renameMap.end()) {
								uwpAppName = it->second;
							}
							names.push_back(std::move(uwpAppName));
							parsingNames.emplace_back(pwszParsingName);
						}
					}
					CoTaskMemFree(pwszDisplayName);
					CoTaskMemFree(pwszParsingName);
					CoTaskMemFree(pidlItem);
				}
				pEnumIDList->Release();
			}
			pAppsFolder->Release();
		}
		pAppsFolderItem->Release();
	}
	const ULONGLONG enumDone = GetTickCount64();

	std::vector<HBITMAP> bitmaps;
	LoadUwpAppIcons(parsingNames, bitmaps);
	for (size_t i = 0; i < names.size(); ++i) {
		// 构造命令字符串以启动UWP应用程序
		callback(
			names[i], // 逻辑名（被 rename 过）
			L"shell:AppsFolder\\" + parsingNames[i],
			parsingNames[i],
			bitmaps[i]
		);
	}
	Logi(L"UWP", L"total=", GetTickCount64() - uwpStart, L"ms enum=", enumCount, L"/", enumDone - uwpStart,
		L"ms icons=", names.size(), L"/", GetTickCount64() - enumDone, L"ms");
	CoUninitialize();
}
