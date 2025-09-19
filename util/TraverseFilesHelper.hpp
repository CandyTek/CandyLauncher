#pragma once

#include <windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <shellapi.h>
#include <Everything.h>
#include <filesystem>
#include "../common/Constants.hpp"
#include "../model/TraverseOptions.h"
#include "MainTools.hpp"
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")


namespace fs = std::filesystem;


template<typename Callback>
static void TraverseFiles(
		const std::wstring &folderPath,
		const TraverseOptions &options,
		Callback &&callback
) {
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

	auto extMatch = [&](const std::wstring &ext) -> bool {
		if (options.extensions.empty()) return true;
		return std::any_of(options.extensions.begin(), options.extensions.end(),
						   [&](const std::wstring &ex) { return _wcsicmp(ext.c_str(), ex.c_str()) == 0; });
	};

	auto addFile = [&](const fs::path &path) {
		std::wstring filename = path.stem().wstring(); // without extension

		if (shouldExclude(options, filename)) return;

		// 重命名映射
		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end()) {
			filename = it->second;
		}

		callback(
				filename,                     // 逻辑名（被 rename 过）
				path.wstring(),               // 完整路径
				path.parent_path().wstring(), // 父目录
				path.extension().wstring()    // 扩展名
		);
	};

	if (options.recursive) {
		for (const auto &entry: fs::recursive_directory_iterator(folderPath)) {
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;
			std::wcout << entry.path() << std::endl;
			addFile(entry.path());
		}
	} else {
		for (const auto &entry: fs::directory_iterator(folderPath)) {
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;
			std::wcout << entry.path() << std::endl;
			addFile(entry.path());
		}
	}
}



/**
 * 检索系统运行中的窗口
 * @tparam Callback 
 * @param callback 
 */
template<typename Callback>
static void TraverseRunningWindows(
		Callback &&callback
) {
	auto enumProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
		if (!IsWindowVisible(hwnd)) return TRUE;

		DWORD pid;
		GetWindowThreadProcessId(hwnd, &pid);

		wchar_t title[256];
		GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));
		if (wcslen(title) == 0) return TRUE; // 跳过无标题窗口

		// 获取进程名
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		std::wstring procName;
		if (hProcess) {
			wchar_t exeName[MAX_PATH];
			if (GetModuleFileNameExW(hProcess, nullptr, exeName, MAX_PATH)) {
				procName = exeName;
			}
			CloseHandle(hProcess);
		}

		// 执行跳转窗口命令 run the window
		// 构造一个“命令”字符串，这里用 activate:<hwnd>
		// wchar_t buf[64];
		// swprintf(buf, 64, L"%p", hwnd);
		std::wstring command = L"";
		// std::wstring command = L"activate:";
		// command += buf;

		// 调用 callback
		auto &cb = *reinterpret_cast<Callback*>(lParam);
		cb(
				std::wstring(title), // 窗口标题作为逻辑名
				procName,            // 所属进程路径
				std::to_wstring((uintptr_t)hwnd), // 窗口句柄字符串
				command              // 激活窗口命令
		);

		return TRUE;
	};

	EnumWindows(enumProc, reinterpret_cast<LPARAM>(&callback));
}



// 获取指定文件夹中的所有快捷方式
static std::vector<std::wstring> GetShortcutsInFolder(const std::string &folderPath) {
	std::vector<std::wstring> shortcuts;

	std::wstring wFolderPath = utf8_to_wide(folderPath);
	std::wstring searchPattern = wFolderPath + L"\\*.lnk";

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				std::wstring fileName = findData.cFileName;
				// 移除 .lnk 扩展名
				if (fileName.length() > 4 && fileName.substr(fileName.length() - 4) == L".lnk") {
					fileName = fileName.substr(0, fileName.length() - 4);
				}
				shortcuts.push_back(fileName);
			}
		} while (FindNextFileW(hFind, &findData));

		FindClose(hFind);
	}

	return shortcuts;
}

static std::vector<std::wstring> GetShortcutsInFolder(const std::wstring &folderPath) {
	return GetShortcutsInFolder(wide_to_utf8(folderPath));
}

// 从注册表获取已安装的应用程序，TODO: 这个功能未完善，索引出来的应用很多都是卸载exe
template<typename Callback>
static void TraverseRegistryApps(
		Callback &&callback, const TraverseOptions &options) {
	// 需要检查的注册表路径
	std::vector<std::pair<HKEY, std::wstring>> registryPaths = {
			{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
			{HKEY_CURRENT_USER,  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
			{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}
	};

	auto addApp = [&](const std::wstring &displayName, const std::wstring &executablePath,
					  const std::wstring &iconPath = L"") {
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
		size_t pos = workingDir.find_last_of(L"\\");
		if (pos != std::wstring::npos) {
			workingDir = workingDir.substr(0, pos);
		}

		callback(
				appName,                     // 逻辑名（被 rename 过）
				executablePath,               // 完整路径
				workingDir
		);
	};

	for (const auto &[hkey, subKey]: registryPaths) {
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
				RegQueryValueExW(hApp, L"SystemComponent", nullptr, nullptr, (LPBYTE) &systemComponent,
								 &systemComponentSize);
				if (systemComponent == 1) {
					RegCloseKey(hApp);
					keyNameSize = sizeof(keyName) / sizeof(wchar_t);
					index++;
					continue;
				}

				// 获取显示名称
				if (RegQueryValueExW(hApp, L"DisplayName", nullptr, nullptr, (LPBYTE) displayName, &displayNameSize) ==
					ERROR_SUCCESS) {
					// 尝试获取可执行文件路径，按优先级顺序
					if (RegQueryValueExW(hApp, L"DisplayIcon", nullptr, nullptr, (LPBYTE) executablePath,
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
						if (RegQueryValueExW(hApp, L"UninstallString", nullptr, nullptr, (LPBYTE) executablePath,
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

// 索引Windows应用商店应用（UWP应用），废弃的，不生效

template<typename Callback>
static void TraverseUWPApps(
		const TraverseOptions &options,
		Callback &&callback) {
	// UWP应用的包信息存储在注册表中
	HKEY hPackages;
	const std::wstring packagesKey = L"SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, packagesKey.c_str(), 0, KEY_READ, &hPackages) != ERROR_SUCCESS) {
		return;
	}

	auto addUWPApp = [&](const std::wstring &displayName, const std::wstring &packageFamilyName,
						 const std::wstring &appId = L"") {
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
				appName,                     // 逻辑名（被 rename 过）
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
			if (RegQueryValueExW(hPackage, L"DisplayName", nullptr, nullptr, (LPBYTE) displayName, &displayNameSize) ==
				ERROR_SUCCESS &&
				RegQueryValueExW(hPackage, L"PackageFamilyName", nullptr, nullptr, (LPBYTE) packageFamilyName,
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

// 获取环境变量PATH中的所有路径
inline std::vector<std::wstring> GetPATHDirectories() {
	std::vector<std::wstring> paths;

	// 获取PATH环境变量的大小
	DWORD size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
	if (size == 0) {
		return paths;
	}

	// 分配缓冲区并获取PATH内容
	std::vector<wchar_t> buffer(size);
	if (GetEnvironmentVariableW(L"PATH", buffer.data(), size) == 0) {
		return paths;
	}

	// 将PATH字符串按分号分割
	std::wstring pathStr(buffer.data());
	std::wstringstream ss(pathStr);
	std::wstring item;

	while (std::getline(ss, item, L';')) {
		// 去除前后空格
		item.erase(0, item.find_first_not_of(L" \t"));
		item.erase(item.find_last_not_of(L" \t") + 1);

		if (!item.empty()) {
			// 去除可能的引号
			if (item.front() == L'"' && item.back() == L'"' && item.length() > 1) {
				item = item.substr(1, item.length() - 2);
			}

			// 检查路径是否存在
			DWORD attrib = GetFileAttributesW(item.c_str());
			if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
				paths.push_back(item);
			}
		}
	}

	return paths;
}

// 索引PATH环境变量中的所有可执行文件，该方法很慢，sdk调用需要时间
template<typename Callback>
static void TraversePATHExecutables(Callback &&callback, TraverseOptions &options) {
	// 获取PATH中的所有目录
	std::vector<std::wstring> pathDirs = GetPATHDirectories();

	// 默认的可执行文件索引选项
	if (options.extensions == std::vector<std::wstring>{L".exe", L".lnk"}) {
		options.extensions = {L".exe", L".bat", L".cmd", L".lnk"};
	}

	options.recursive = false; // PATH目录通常不需要递归搜索

	// 遍历每个PATH目录
	for (const auto &pathDir: pathDirs) {
		try {
			// 使用TraverseFilesForEverythingSDK索引该目录，比一般的索引要更慢，可能是本来调用sdk就会有一些开销
			TraverseFilesForEverythingSDK(pathDir, options, callback);
		}
		catch (const std::exception &e) {
			// 记录错误但继续处理其他目录
			std::wcout << L"Error indexing PATH directory " << pathDir
					   << L": " << utf8_to_wide(e.what()) << std::endl;
		}
	}
}

// 索引PATH环境变量中的所有可执行文件
template<typename Callback>
static void TraversePATHExecutables2(Callback &&callback, TraverseOptions &options) {
	// 获取PATH中的所有目录
	std::vector<std::wstring> pathDirs = GetPATHDirectories();

	// 默认的可执行文件索引选项
	if (options.extensions == std::vector<std::wstring>{L".exe", L".lnk"}) {
		options.extensions = {L".exe", L".bat", L".cmd", L".lnk"};
	}

	options.recursive = false; // PATH目录通常不需要递归搜索


	// 遍历每个PATH目录
	for (const auto &pathDir: pathDirs) {
		try {
			TraverseFiles(pathDir, options, callback);
		} catch (...) {
			continue;
		}
	}
}


// 创建默认的可执行文件索引选项
inline TraverseOptions CreateDefaultPathTraverseOptions() {
	TraverseOptions options;
	options.extensions = {L".exe", L".bat", L".cmd", L".lnk"};
	options.recursive = false; // PATH目录通常不需要递归搜索
	return options;
}
