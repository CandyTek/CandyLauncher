#pragma once

#include <windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <shellapi.h>
#include <filesystem>
#include "../common/Constants.hpp"
#include "../model/TraverseOptions.hpp"
#include "MainTools.hpp"
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")


template <typename Callback>
static void TraverseFiles(
	const std::wstring& folderPath2,
	const TraverseOptions& options,
	const std::wstring& exeFolderPath,
	Callback&& callback
) {
	namespace fs = std::filesystem;
	const std::wstring folderPath = ExpandEnvironmentVariables(folderPath2, exeFolderPath);
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

	auto extMatch = [&](const std::wstring& ext) -> bool {
		if (options.extensions.empty()) return true;
		return std::any_of(options.extensions.begin(), options.extensions.end(),
							[&](const std::wstring& ex) {
								return _wcsicmp(ext.c_str(), ex.c_str()) == 0;
							});
	};

	auto addFile = [&](const fs::path& path) {
		std::wstring filename = path.stem().wstring(); // without extension

		if (shouldExclude(options, filename)) return;

		// 重命名映射
		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end()) {
			filename = it->second;
		}

		callback(
			filename, // 逻辑名（被 rename 过）
			path.wstring(), // 完整路径
			path.parent_path().wstring(), // 父目录
			path.extension().wstring() // 扩展名
		);
	};

	if (options.recursive) {
		for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;
			// std::wcout << entry.path() << std::endl;
			addFile(entry.path());
		}
	} else {
		for (const auto& entry : fs::directory_iterator(folderPath)) {
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;
			// std::wcout << entry.path() << std::endl;
			addFile(entry.path());
		}
	}
}


// 获取指定文件夹中的所有快捷方式
static std::vector<std::wstring> GetShortcutsInFolder(const std::string& folderPath) {
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

static std::vector<std::wstring> GetShortcutsInFolder(const std::wstring& folderPath) {
	return GetShortcutsInFolder(wide_to_utf8(folderPath));
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
template <typename Callback>
static void TraversePATHExecutables(Callback&& callback, TraverseOptions& options) {
	// 获取PATH中的所有目录
	std::vector<std::wstring> pathDirs = GetPATHDirectories();
	options.recursive = false; // PATH目录通常不需要递归搜索

	// 遍历每个PATH目录
	for (const auto& pathDir : pathDirs) {
		try {
			// 使用TraverseFilesForEverythingSDK索引该目录，比一般的索引要更慢，可能是本来调用sdk就会有一些开销
			// TraverseFilesForEverythingSDK(pathDir, options, callback);
		} catch (const std::exception& e) {
			// 记录错误但继续处理其他目录
			std::wcout << L"Error indexing PATH directory " << pathDir
				<< L": " << utf8_to_wide(e.what()) << std::endl;
		}
	}
}

// 索引PATH环境变量中的所有可执行文件
template <typename Callback>
static void TraversePATHExecutables2(Callback&& callback, TraverseOptions& options, const std::wstring& exeFolderPath) {
	// 获取PATH中的所有目录
	std::vector<std::wstring> pathDirs = GetPATHDirectories();
	options.recursive = false; // PATH目录通常不需要递归搜索


	// 遍历每个PATH目录
	for (const auto& pathDir : pathDirs) {
		try {
			TraverseFiles(pathDir, options, exeFolderPath, callback);
		} catch (...) {
			continue;
		}
	}
}


// 创建默认的可执行文件索引选项
inline TraverseOptions CreateDefaultPathTraverseOptions() {
	TraverseOptions options;
	options.extensions = DEFAULT_EXTENSIONS;
	options.recursive = false; // PATH目录通常不需要递归搜索
	return options;
}
