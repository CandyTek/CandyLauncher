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


struct TraverseOptions {
	std::vector<std::wstring> extensions; // e.g., {L".exe", L".lnk"}
	std::vector<std::wstring> excludeNames; // 完整排除，需完全匹配字符，大小写
	std::vector<std::wstring> excludeWords; // 包含关键词排除，忽略大小写
	std::vector<std::wstring> renameSources; // 包含关键词排除，大小写匹配
	std::vector<std::wstring> renameTargets; // 包含关键词排除
	std::unordered_map<std::wstring, std::wstring> renameMap; // name -> displayName
	std::wstring type;
	std::wstring folder;
	std::wstring command;
	bool recursive = false;
};

// 需匹配大小写
static auto shouldExclude = [](const TraverseOptions& options, const std::wstring& name) -> bool {
	if (std::find(options.excludeNames.begin(),
				options.excludeNames.end(),
				name) != options.excludeNames.end()) {
		return true;
				}
	std::wstring nameLower = name;
	std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
	for (const auto& word : options.excludeWords) {
		if (nameLower.find(word) != std::wstring::npos) return true;
	}
	return false;
};
