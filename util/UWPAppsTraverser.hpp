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


/// <summary>
/// Enumerates UWP applications from the AppsFolder and adds them to the actions list.
/// This is the C++ equivalent of the C# SpecificallyForGetCurrentUwpName2() and the subsequent loop.
/// </summary>
/// <param name="actions">The list of actions to add UWP apps to.</param>
template <typename Callback>
static void LoadUwpApps(Callback&& callback, const TraverseOptions& options) {
	if (FAILED(CoInitialize(NULL))) {
		return;
	}

	IKnownFolderManager* pKnownFolderManager = nullptr;
	if (FAILED(
		CoCreateInstance(CLSID_KnownFolderManager, NULL, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pKnownFolderManager)))) {
		CoUninitialize();
		return;
	}

	IShellItem* pAppsFolderItem = nullptr;
	if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(&pAppsFolderItem)))) {
		IShellFolder* pDesktopFolder = nullptr;
		if (SUCCEEDED(SHGetDesktopFolder(&pDesktopFolder))) {
			LPITEMIDLIST pidl = nullptr;
			if (SUCCEEDED(SHGetIDListFromObject(pAppsFolderItem, &pidl))) {
				IShellFolder* pAppsFolderShellFolder = nullptr;
				if (SUCCEEDED(pDesktopFolder->BindToObject(pidl, NULL, IID_PPV_ARGS(&pAppsFolderShellFolder)))) {
					IEnumIDList* pEnumIDList = nullptr;
					if (SUCCEEDED(
						pAppsFolderShellFolder->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS,
							&pEnumIDList))) {
						LPITEMIDLIST pidlItem = nullptr;
						ULONG fetched = 0;
						while (pEnumIDList->Next(1, &pidlItem, &fetched) == S_OK) {
							IShellItem* pShellItem = nullptr;
							if (SUCCEEDED(
								SHCreateItemWithParent(pidl, pAppsFolderShellFolder, pidlItem,
									IID_PPV_ARGS(&pShellItem)
								))) {
								// 处理每个 ShellItem
								LPWSTR pwszParsingName = nullptr;
								if (FAILED(
									pShellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pwszParsingName))) {
									pShellItem->Release();
									continue;
								}

								// UWP应用程序通常具有“！”以他们的解析名称。
								if (wcschr(pwszParsingName, L'!') == nullptr) {
									CoTaskMemFree(pwszParsingName);
									pShellItem->Release();
									continue;
								}

								LPWSTR pwszDisplayName = nullptr;
								if (FAILED(pShellItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pwszDisplayName))) {
									CoTaskMemFree(pwszParsingName);
									pShellItem->Release();
									continue;
								}
								// 进行UWP列表项的筛选，排除，重命名
								std::wstring uwpAppName = pwszDisplayName;
								if (shouldExclude(options, uwpAppName)) {
									CoTaskMemFree(pwszParsingName);
									pShellItem->Release();
									continue;
								}
								if (const auto it = options.renameMap.find(uwpAppName); it != options.renameMap.end()) {
									uwpAppName = it->second;
								}

								HBITMAP hBitmap = nullptr;
								IShellItemImageFactory* pImageFactory = nullptr;
								// 大小可以调整。 256x256是一个很好的高质量尺寸。
								// LISTITEM_ICON_SIZE
								SIZE size = {48, 48};
								if (SUCCEEDED(pShellItem->QueryInterface(IID_PPV_ARGS(&pImageFactory)))) {
									// SIIGBF_RESIZETOFIT 即使没有确切的尺寸，也可以确保我们获得图像。
									// SIIGBF_ICONONLY 防止获得文档的缩略图预览。
									pImageFactory->GetImage(size, SIIGBF_RESIZETOFIT | SIIGBF_ICONONLY, &hBitmap);
									pImageFactory->Release();
								}

								// 构造命令字符串以启动UWP应用程序
								std::wstring uwpCommand = L"shell:AppsFolder\\";
								uwpCommand += pwszParsingName;

								std::wstring uwpCommandS = L"";
								uwpCommandS += pwszParsingName;

								// 为UWP应用程序创建一个新的RunCommandaction。
								if (hBitmap != nullptr) {
									// TODO: 使用HBITMAP并管理其生命周期
									callback(
										uwpAppName, // 逻辑名（被 rename 过）
										uwpCommand,
										uwpCommandS,
										hBitmap
									);
								} else {
									// 无法加载图标的后备
									callback(
										uwpAppName, // 逻辑名（被 rename 过）
										uwpCommand,
										uwpCommandS,
										nullptr
									);
								}

								CoTaskMemFree(pwszDisplayName);
								CoTaskMemFree(pwszParsingName);
								pShellItem->Release();
							}
							CoTaskMemFree(pidlItem);
						}
						pEnumIDList->Release();
					}
					pAppsFolderShellFolder->Release();
				}
				CoTaskMemFree(pidl);
			}
			pDesktopFolder->Release();
		}
		pAppsFolderItem->Release();
	}
	CoUninitialize();
}
