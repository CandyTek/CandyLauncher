#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <windows.h>
#include <sstream>


struct TraverseOptions
{
	std::vector<std::wstring> extensions; // e.g., {L".exe", L".lnk"}
	std::vector<std::wstring> excludeNames; // 完整排除
	std::vector<std::wstring> excludeWords; // 包含关键词排除
	std::vector<std::wstring> renameSources; // 包含关键词排除
	std::vector<std::wstring> renameTargets; // 包含关键词排除
	std::unordered_map<std::wstring, std::wstring> renameMap; // name -> displayName
	std::wstring folder;
	std::wstring command;
	bool recursive = false;
};

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

// 创建默认的可执行文件索引选项
inline TraverseOptions CreateExecutableTraverseOptions() {
	TraverseOptions options;
	options.extensions = {L".exe", L".bat", L".cmd", L".lnk"};
	options.recursive = false; // PATH目录通常不需要递归搜索
	return options;
}
