#pragma once

#include <windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <shellapi.h>
#include <Everything.h>
#include <filesystem>
#include "RunCommandAction.hpp"
#include "Constants.hpp"
#include "TraverseOptions.h"
#include "MainTools.hpp"


namespace fs = std::filesystem;



static void TraverseFiles(
		const std::wstring& folderPath,
		const TraverseOptions& options,
		std::vector<std::shared_ptr<RunCommandAction>>& outActions
)
{
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

	auto extMatch = [&](const std::wstring& ext) -> bool
	{
		if (options.extensions.empty()) return true;
		return std::any_of(options.extensions.begin(), options.extensions.end(),
						   [&](const std::wstring& ex) { return _wcsicmp(ext.c_str(), ex.c_str()) == 0; });
	};

	auto addFile = [&](const fs::path& path)
	{
		std::wstring filename = path.stem().wstring(); // without extension

		if (shouldExclude(options,filename)) return;

		// 重命名映射
		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end())
		{
			filename = it->second;
		}

		const auto action = std::make_shared<RunCommandAction>(
				filename, path.wstring(), false, true, path.parent_path().wstring()
		);

		outActions.push_back(action);
	};

	if (options.recursive)
	{
		for (const auto& entry : fs::recursive_directory_iterator(folderPath))
		{
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;
			std::cout << entry.path() << std::endl;
			addFile(entry.path());
		}
	}
	else
	{
		for (const auto& entry : fs::directory_iterator(folderPath))
		{
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;
			std::cout << entry.path() << std::endl;
			addFile(entry.path());
		}
	}
}

static void TraverseFilesForEverything(
		const std::wstring& folderPath,
		const TraverseOptions& options,
		std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

	// addFile lambda 保持不变，可以完美复用
	auto addFile = [&](const fs::path& path)
	{
		std::wstring filename = path.stem().wstring(); // without extension

		if (shouldExclude(options, filename)) return;

		// 重命名映射
		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end())
		{
			filename = it->second;
		}

		const auto action = std::make_shared<RunCommandAction>(
				filename, path.wstring(), false, true, path.parent_path().wstring()
		);

		outActions.push_back(action);
	};

	// 1. 构建 Everything 查询命令
	std::wstringstream command;
	// 假设 es.exe 在 PATH 中，或者提供完整路径
	command << L"D:\\Download3\\ES-1.1.0.30.x64\\es.exe -utf8-bom ";

	// -p 指定搜索路径
	command << L"-p \"" << folderPath << L"\" ";

	// 从扩展名列表动态生成正则表达式
	if (!options.extensions.empty())
	{
		std::wstringstream regex_stream;
		regex_stream << L"\"("; // 正则表达式部分用引号括起来

		for (size_t i = 0; i < options.extensions.size(); ++i)
		{
			std::wstring ext = options.extensions[i];

			// 为正则表达式转义特殊字符，尤其是 '.'
			std::wstring escaped_ext;
			for (wchar_t c : ext)
			{
				if (c == L'.' || c == L'\\' || c == L'?' || c == L'*' || c == L'+' || c == L'(' || c == L')' || c == L'[' || c == L']' || c == L'{' || c == L'}' || c == L'^' || c == L'$')
				{
					escaped_ext += L'\\';
				}
				escaped_ext += c;
			}

			regex_stream << escaped_ext << (i < options.extensions.size() - 1 ? L"|" : L"");
		}
		regex_stream << L")$\""; // 以 $ 结尾，确保是文件扩展名

		// -r 指定正则表达式
		command << L"-r " << regex_stream.str();
	}

	try
	{
		// 2. 执行命令并获取纯文本输出
		std::string commandOutput = ExecuteCommandAndGetOutput(command.str());
//        std::cout << commandOutput << std::endl;

		if (commandOutput.empty()) return;

		// 3. 逐行解析输出
		std::stringstream ss(commandOutput);
		std::string line;
		fs::path searchFolderPath(folderPath); // 预先创建path对象用于比较

		while (std::getline(ss, line))
		{
			if (line.empty()) continue;
			line.erase(line.find_last_not_of("\r\n") + 1);
			// es.exe 的输出是UTF-8编码，使用u8path可以正确处理包含非英文字符的路径
//            fs::path filePath = fs::u8path(line);
			std::wstring wide_path_str = MultiByteToWide(line, CP_ACP); // CP_ACP 表示系统的当前活动代码页
			// 直接用 wstring 构造 path 对象，这是在Windows上最可靠的方式
//            std::cout << WStringToUtf8(wide_path_str) << std::endl;

			fs::path filePath(wide_path_str);
//            std::cout << WStringToUtf8(filePath.wstring()) << std::endl;

			// 4. 如果是非递归搜索，需要额外判断父目录是否匹配
			if (!options.recursive)
			{
				if (filePath.parent_path() != searchFolderPath)
				{
					continue; // 如果父目录不匹配，则跳过此文件
				}
			}

			// 复用 addFile 逻辑
//            std::cout << filePath << std::endl;
			addFile(filePath);
		}
	}
	catch (const std::exception& e)
	{
		// 错误处理，可以考虑回退到原始的文件系统遍历方法
		// e.g., LogError("Failed to search with Everything: " + std::string(e.what()));
		std::cout << ("Failed to search with Everything: " + std::string(e.what())) << std::endl;

	}
}

template <typename Callback>
static void TraverseFilesForEverythingSDK(
		const std::wstring& folderPath,
		const TraverseOptions& options,
		Callback&& callback)
{
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath) || options.extensions.empty()) return;

	auto addFile = [&](const fs::path& path)
	{
		std::wstring filename = path.stem().wstring();
		if (shouldExclude(options, filename)) return;

		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end())
			filename = it->second;

		// 这里不限定 callback 的参数，可以传多种信息
		callback(
				filename,                     // 逻辑名（被 rename 过）
				path.wstring(),               // 完整路径
				path.parent_path().wstring(), // 父目录
				path.extension().wstring()    // 扩展名
		);
	};


	// 构造更高效的搜索查询
	// 例如: parent:"C:\ProgramData\Microsoft\Windows\Start Menu\Programs" ext:exe;lnk
	std::wstringstream search_query;
	if (options.recursive)
	{
		// 如果是递归搜索，则使用原始的路径搜索
		search_query << L"\"" << folderPath << L"\" ";
	}
	else
	{
		// 如果不递归，使用 parent: 函数更精确、更高效
		search_query << L"parent:\"" << folderPath << L"\" ";
	}

	// 使用 ext: 过滤器，比正则表达式更简单快速
	search_query << L"ext:";
	for (size_t i = 0; i < options.extensions.size(); ++i)
	{
		std::wstring ext = options.extensions[i];
		// 去掉可能存在的点
		if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
		search_query << ext;
		if (i < options.extensions.size() - 1) search_query << L";";
	}

	std::wcout << L"Executing optimized Everything search: " << search_query.str() << std::endl;

	// 使用 Everything SDK
	Everything_SetSearchW(search_query.str().c_str());
	Everything_QueryW(TRUE);

	DWORD numResults = Everything_GetNumResults();

	for (DWORD i = 0; i < numResults; i++)
	{
		wchar_t fullPath[MAX_PATH];
		Everything_GetResultFullPathNameW(i, fullPath, MAX_PATH);

		// 因为查询已经精确过滤，不再需要手动判断父目录了
		// if (!options.recursive) { ... } 这段逻辑可以移除
		addFile(fs::path(fullPath));
	}
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

// 从注册表获取已安装的应用程序
static void TraverseRegistryApps(
		const TraverseOptions& options,
		std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	// 需要检查的注册表路径
	std::vector<std::pair<HKEY, std::wstring>> registryPaths = {
		{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
		{HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
		{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}
	};

	auto addApp = [&](const std::wstring& displayName, const std::wstring& executablePath, const std::wstring& iconPath = L"") {
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

		const auto action = std::make_shared<RunCommandAction>(
			appName, executablePath, false, true, workingDir
		);

		outActions.push_back(action);
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
		while (RegEnumKeyExW(hUninstall, index, keyName, &keyNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
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
				RegQueryValueExW(hApp, L"SystemComponent", nullptr, nullptr, (LPBYTE)&systemComponent, &systemComponentSize);
				if (systemComponent == 1) {
					RegCloseKey(hApp);
					keyNameSize = sizeof(keyName) / sizeof(wchar_t);
					index++;
					continue;
				}

				// 获取显示名称
				if (RegQueryValueExW(hApp, L"DisplayName", nullptr, nullptr, (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS) {
					// 尝试获取可执行文件路径，按优先级顺序
					if (RegQueryValueExW(hApp, L"DisplayIcon", nullptr, nullptr, (LPBYTE)executablePath, &executablePathSize) == ERROR_SUCCESS) {
						// DisplayIcon 可能包含图标索引，需要清理
						std::wstring iconStr(executablePath);
						size_t commaPos = iconStr.find(L",");
						if (commaPos != std::wstring::npos) {
							iconStr = iconStr.substr(0, commaPos);
						}
						wcscpy_s(executablePath, iconStr.c_str());
					} else {
						executablePathSize = sizeof(executablePath);
						if (RegQueryValueExW(hApp, L"UninstallString", nullptr, nullptr, (LPBYTE)executablePath, &executablePathSize) == ERROR_SUCCESS) {
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

// 索引Windows应用商店应用（UWP应用）
static void TraverseUWPApps(
		const TraverseOptions& options,
		std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	// UWP应用的包信息存储在注册表中
	HKEY hPackages;
	const std::wstring packagesKey = L"SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
	
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, packagesKey.c_str(), 0, KEY_READ, &hPackages) != ERROR_SUCCESS) {
		return;
	}

	auto addUWPApp = [&](const std::wstring& displayName, const std::wstring& packageFamilyName, const std::wstring& appId = L"") {
		if (displayName.empty() || packageFamilyName.empty()) return;
		
		std::wstring appName = displayName;
		
		// 检查排除规则
		if (shouldExclude(options, appName)) return;
		
		// 应用重命名映射
		if (const auto it = options.renameMap.find(appName); it != options.renameMap.end()) {
			appName = it->second;
		}

		// UWP应用的启动命令格式
		std::wstring command = L"shell:AppsFolder\\" + packageFamilyName;
		if (!appId.empty()) {
			command += L"!" + appId;
		}

		const auto action = std::make_shared<RunCommandAction>(
			appName, command, false, true, L""
		);

		outActions.push_back(action);
	};

	DWORD index = 0;
	wchar_t packageName[256];
	DWORD packageNameSize = sizeof(packageName) / sizeof(wchar_t);

	// 枚举所有UWP包
	while (RegEnumKeyExW(hPackages, index, packageName, &packageNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
		HKEY hPackage;
		if (RegOpenKeyExW(hPackages, packageName, 0, KEY_READ, &hPackage) == ERROR_SUCCESS) {
			wchar_t displayName[512] = {0};
			wchar_t packageFamilyName[256] = {0};
			DWORD displayNameSize = sizeof(displayName);
			DWORD packageFamilyNameSize = sizeof(packageFamilyName);

			// 获取显示名称和包族名称
			if (RegQueryValueExW(hPackage, L"DisplayName", nullptr, nullptr, (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS &&
				RegQueryValueExW(hPackage, L"PackageFamilyName", nullptr, nullptr, (LPBYTE)packageFamilyName, &packageFamilyNameSize) == ERROR_SUCCESS) {
				
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

// 综合索引方法：同时索引传统应用和UWP应用
static void TraverseInstalledApps(
		const TraverseOptions& options,
		std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	// 索引注册表中的传统应用
	TraverseRegistryApps(options, outActions);
	
	// 索引UWP应用
	TraverseUWPApps(options, outActions);
}

// 索引PATH环境变量中的所有可执行文件
template <typename Callback>
static void TraversePATHExecutables(Callback&& callback)
{
	// 获取PATH中的所有目录
	std::vector<std::wstring> pathDirs = GetPATHDirectories();
	
	// 创建默认的可执行文件索引选项
	TraverseOptions options = CreateExecutableTraverseOptions();
	
	// 遍历每个PATH目录
	for (const auto& pathDir : pathDirs) {
		try {
			// 使用TraverseFilesForEverythingSDK索引该目录
			TraverseFilesForEverythingSDK(pathDir, options, callback);
		}
		catch (const std::exception& e) {
			// 记录错误但继续处理其他目录
			std::wcout << L"Error indexing PATH directory " << pathDir 
			          << L": " << utf8_to_wide(e.what()) << std::endl;
		}
	}
}

// 索引PATH环境变量中的所有可执行文件
static void TraversePATHExecutables2(std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	// 获取PATH中的所有目录
	std::vector<std::wstring> pathDirs = GetPATHDirectories();
	
	// 创建默认的可执行文件索引选项
	TraverseOptions options = CreateExecutableTraverseOptions();
	
	// 遍历每个PATH目录
	for (const auto& pathDir : pathDirs) {
		try{
			
			TraverseFiles(pathDir, options, outActions);
		} catch (...) {
			continue;
		}
	}
}

// 重载版本：返回RunCommandAction向量
static void TraversePATHExecutables(std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	TraversePATHExecutables([&](const std::wstring& name, const std::wstring& fullPath,
	                           const std::wstring& parent, const std::wstring& ext) {
		const auto action = std::make_shared<RunCommandAction>(
			name, fullPath, false, true, parent
		);
		outActions.push_back(action);
	});
}

// 全面索引方法：索引已安装应用 + PATH可执行文件
static void TraverseAllExecutables(std::vector<std::shared_ptr<RunCommandAction>>& outActions)
{
	// 创建默认选项用于已安装应用索引
	TraverseOptions options = CreateExecutableTraverseOptions();
	
	// 1. 索引已安装的应用程序（注册表 + UWP）
	TraverseInstalledApps(options, outActions);
	
	// 2. 索引PATH环境变量中的可执行文件
	TraversePATHExecutables(outActions);
}
