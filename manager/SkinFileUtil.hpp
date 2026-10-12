#pragma once
#include <string>
#include <vector>
#include <filesystem>

#include "common/GlobalState.hpp"

static std::wstring getCurrectSkinPath(std::wstring skinPath) {
    std::wstring result;
    if (skinPath == L"default") {
        result = DEFAULT_SKIN_PATH;
    } else if (skinPath == L"night_mode") {
        result = NIGHT_SKIN_PATH;
    } else {
        skinPath = NormalizePath(skinPath);
        if (StartsWith(skinPath, L"/") || StartsWith(skinPath, L"\\")) {
            result = EXE_FOLDER_PATH + skinPath;
        } else {
            result = skinPath;
        }
    }
    return result;
}

// 用来查找程序目录下的所有皮肤文件，返回相对程序目录的路径（以 \ 开头）
static std::vector<std::wstring> FindSkinFiles() {
	namespace fs = std::filesystem;
	std::vector<std::wstring> skinFiles;
	std::error_code ec;
	fs::recursive_directory_iterator it(EXE_FOLDER_PATH + LR"(\skins)", fs::directory_options::skip_permission_denied, ec);
	for (const fs::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
		if (!it->is_regular_file(ec)) continue;
		const fs::path& path = it->path();
		if (_wcsicmp(path.extension().c_str(), L".json") != 0) continue;
		if (!StartsWithIgnoreCase(path.filename().wstring(), L"skin_")) continue;
		const std::wstring fullPath = path.wstring();
		if (StartsWithIgnoreCase(fullPath, EXE_FOLDER_PATH)) {
			skinFiles.push_back(fullPath.substr(EXE_FOLDER_PATH.length()));
		}
	}
	return skinFiles;
}
