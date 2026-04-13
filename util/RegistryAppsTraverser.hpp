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


// 从注册表获取已安装的应用程序，TODO: 这个功能未完善，索引出来的应用很多都是卸载exe
template <typename Callback>
static void TraverseRegistryApps(
	Callback&& callback, const TraverseOptions& options) {
	// 需要检查的注册表路径
	std::vector<std::pair<HKEY, std::wstring>> registryPaths = {
		{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
		{HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
		{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}
	};

	auto addApp = [&](const std::wstring& displayName, const std::wstring& executablePath,
					const std::wstring& iconPath = L"") {
		if (true) return;
		if (displayName.empty() || executablePath.empty()) return;

		std::wstring appName = displayName;

		// 检查排除规则
		if (shouldExclude(options, appName)) return;

		// 应用重命名映射
		if (const auto it = options.renameMap.find(appName); it != options.renameMap.end()) {
			appName = it->second;
		}

		// 获取可执行文件的父目录
		std::wstring workingDir = executablePath;
		size_t pos = workingDir.find_last_of(L"\\/");
		if (pos != std::wstring::npos) {
			workingDir = workingDir.substr(0, pos);
		}

		callback(
			appName, // 逻辑名（被 rename 过）
			executablePath, // 完整路径
			workingDir
		);
	};

	for (const auto& [hkey, subKey] : registryPaths) {
		HKEY hUninstall;
		if (RegOpenKeyExW(hkey, subKey.c_str(), 0, KEY_READ, &hUninstall) != ERROR_SUCCESS) {
			continue;
		}

		DWORD index = 0;
		wchar_t keyName[256];
		DWORD keyNameSize = sizeof(keyName) / sizeof(wchar_t);

		// 枚举所有子键
		while (RegEnumKeyExW(hUninstall, index, keyName, &keyNameSize, nullptr, nullptr, nullptr, nullptr) ==
			ERROR_SUCCESS) {
			HKEY hApp;
			if (RegOpenKeyExW(hUninstall, keyName, 0, KEY_READ, &hApp) == ERROR_SUCCESS) {
				wchar_t displayName[512] = {0};
				wchar_t executablePath[MAX_PATH] = {0};
				wchar_t iconPath[MAX_PATH] = {0};
				DWORD displayNameSize = sizeof(displayName);
				DWORD executablePathSize = sizeof(executablePath);
				DWORD iconPathSize = sizeof(iconPath);
				DWORD systemComponent = 0;
				DWORD systemComponentSize = sizeof(systemComponent);

				// 检查是否为系统组件，如果是则跳过
				RegQueryValueExW(hApp, L"SystemComponent", nullptr, nullptr, (LPBYTE)&systemComponent,
								&systemComponentSize);
				if (systemComponent == 1) {
					RegCloseKey(hApp);
					keyNameSize = sizeof(keyName) / sizeof(wchar_t);
					index++;
					continue;
				}

				// 获取显示名称
				if (RegQueryValueExW(hApp, L"DisplayName", nullptr, nullptr, (LPBYTE)displayName, &displayNameSize) ==
					ERROR_SUCCESS) {
					// 尝试获取可执行文件路径，按优先级顺序
					if (RegQueryValueExW(hApp, L"DisplayIcon", nullptr, nullptr, (LPBYTE)executablePath,
										&executablePathSize) == ERROR_SUCCESS) {
						// DisplayIcon 可能包含图标索引，需要清理
						std::wstring iconStr(executablePath);
						size_t commaPos = iconStr.find(L",");
						if (commaPos != std::wstring::npos) {
							iconStr = iconStr.substr(0, commaPos);
						}
						wcscpy_s(executablePath, iconStr.c_str());
					} else {
						executablePathSize = sizeof(executablePath);
						if (RegQueryValueExW(hApp, L"UninstallString", nullptr, nullptr, (LPBYTE)executablePath,
											&executablePathSize) == ERROR_SUCCESS) {
							// UninstallString 通常包含卸载程序路径，可能不是我们想要的主程序
							// 但有时候没有其他选择
							std::wstring uninstallStr(executablePath);

							// 尝试从卸载字符串中提取可执行文件路径
							if (uninstallStr.find(L"MsiExec.exe") != std::wstring::npos) {
								// MSI安装的程序，跳过
								RegCloseKey(hApp);
								keyNameSize = sizeof(keyName) / sizeof(wchar_t);
								index++;
								continue;
							}
						}
					}

					// 检查文件是否存在且为可执行文件
					if (wcslen(executablePath) > 0) {
						// 移除可能的引号
						std::wstring cleanPath(executablePath);
						if (cleanPath.front() == L'"' && cleanPath.back() == L'"') {
							cleanPath = cleanPath.substr(1, cleanPath.length() - 2);
						}

						DWORD fileAttrib = GetFileAttributesW(cleanPath.c_str());
						if (fileAttrib != INVALID_FILE_ATTRIBUTES && !(fileAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
							// 检查是否为可执行文件
							std::wstring ext = cleanPath.substr(cleanPath.find_last_of(L"."));
							std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

							if (ext == L".exe" || ext == L".com" || ext == L".bat" || ext == L".cmd") {
								addApp(displayName, cleanPath, iconPath);
							}
						}
					}
				}

				RegCloseKey(hApp);
			}

			keyNameSize = sizeof(keyName) / sizeof(wchar_t);
			index++;
		}

		RegCloseKey(hUninstall);
	}
}
