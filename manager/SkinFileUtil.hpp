#pragma once
#include <string>

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
