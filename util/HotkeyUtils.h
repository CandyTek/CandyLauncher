#pragma once
#include <intsafe.h>
#include <string>

#include "StringUtil.hpp"

struct ParsedHotkey {
    UINT vk = 0;
    UINT mod = 0;
    bool valid = false;

    bool matches(UINT inVk, UINT inMod) const {
        return valid && inVk == vk && inMod == mod;
    }
};

static bool ParseHotkeyString(const std::string& hotkeyStr, UINT& modifiers, UINT& vk) {
    modifiers = 0;
    vk = 0;

    size_t posStart = hotkeyStr.rfind('(');
    size_t posEnd = hotkeyStr.rfind(')');

    if (posStart == std::string::npos || posEnd == std::string::npos || posEnd <= posStart + 1) {
        return false;
    }

    const std::string vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
    try {
        vk = std::stoi(vkStr);
    } catch (...) {
        return false;
    }

    if (posStart == 0) return true;

    posEnd = posStart - 1;
    posStart = hotkeyStr.rfind('(', posEnd);
    if (posStart == std::string::npos || posEnd <= posStart + 1) {
        return true;
    }

    const std::string modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
    try {
        modifiers = std::stoi(modStr);
    } catch (...) {
        return false;
    }
    return true;
}

static ParsedHotkey ParseHotkeyString(const std::string& utf8Str) {
    if (utf8Str.empty()) return {};
    std::wstring str = utf8_to_wide(utf8Str);

    size_t posEnd = str.rfind(L')');
    size_t posStart = str.rfind(L'(', posEnd);
    if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1) return {};

    ParsedHotkey h;
    try {
        h.vk = static_cast<UINT>(std::stoi(str.substr(posStart + 1, posEnd - posStart - 1)));
    } catch (...) {
        return {};
    }

    if (posStart > 0) {
        size_t posEnd2 = posStart - 1;
        size_t posStart2 = str.rfind(L'(', posEnd2);
        if (posStart2 != std::wstring::npos && posEnd2 > posStart2) {
            try {
                h.mod = static_cast<UINT>(std::stoi(str.substr(posStart2 + 1, posEnd2 - posStart2 - 1)));
            } catch (...) {
            }
        }
    }

    h.valid = true;
    return h;
}
