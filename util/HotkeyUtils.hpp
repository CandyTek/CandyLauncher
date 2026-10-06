#pragma once

#include <intsafe.h>
#include <string>
#include "StringUtil.hpp"
#include "common\constants.hpp"
#include "common\GlobalState.hpp"

inline static bool g_toggleMainPanelHookKeyDown = false;
inline static DWORD g_toggleMainPanelHookLastTriggerTick = 0;

#ifndef BUILDING_PLUGIN_DLL
HHOOK g_toggleMainPanelKeyboardHook = nullptr;
HHOOK g_toggleMainPanelMouseHook = nullptr;
#endif

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

static ParsedHotkey ParseHotkeyString(const std::wstring& str) {
	if (str.empty()) return {};

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

static ParsedHotkey ParseHotkeyString(const std::string& str)
{
	return  ParseHotkeyString(utf8_to_wide(str));
}

// 解析热键字符串，支持两种格式：
// 1. "Alt+G(4)(71)" 带尾随 (modifiers)(vk)
// 2. "Ctrl+Alt+F1", "Shift+Win+A" 纯文本
static bool ParseHotkey(const std::wstring& hotkeyStr, UINT& outMod, UINT& outVk) {
    outMod = 0; outVk = 0;
    if (hotkeyStr.empty()) return false;

    // 格式 1: "Alt+G(4)(71)"
    size_t p2End = hotkeyStr.rfind(L')');
    if (p2End != std::wstring::npos) {
        size_t p2Start = hotkeyStr.rfind(L'(', p2End);
        if (p2Start != std::wstring::npos && p2Start > 0) {
            size_t p1End = hotkeyStr.rfind(L')', p2Start - 1);
            if (p1End != std::wstring::npos) {
                size_t p1Start = hotkeyStr.rfind(L'(', p1End);
                if (p1Start != std::wstring::npos) {
                    try {
                        outMod = std::stoul(hotkeyStr.substr(p1Start + 1, p1End - p1Start - 1));
                        outVk  = std::stoul(hotkeyStr.substr(p2Start + 1, p2End - p2Start - 1));
                        return outVk != 0;
                    } catch (...) {}
                }
            }
        }
    }

    // 格式 2: 纯文本按键解析
    std::vector<std::wstring> tokens;
    std::wstring cur;
    for (wchar_t ch : hotkeyStr) {
        if (ch == L'+') {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else if (ch != L' ' && ch != L'\t') {
            cur += static_cast<wchar_t>(std::towlower(ch));
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    if (tokens.empty()) return false;

    std::wstring keyToken = tokens.back();
    tokens.pop_back();

    for (const auto& mod : tokens) {
        if (mod == L"ctrl" || mod == L"control") outMod |= MOD_CONTROL;
        else if (mod == L"alt")                 outMod |= MOD_ALT;
        else if (mod == L"shift")               outMod |= MOD_SHIFT;
        else if (mod == L"win")                 outMod |= MOD_WIN;
    }

    if (keyToken.size() == 1) {
        wchar_t ch = static_cast<wchar_t>(std::towupper(keyToken[0]));
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
            outVk = ch;
        } else {
            SHORT s = VkKeyScanW(ch);
            if (s != -1) outVk = LOBYTE(s);
        }
    } else if (keyToken.size() >= 2 && keyToken[0] == L'f') {
        try {
            int fn = std::stoi(keyToken.substr(1));
            if (fn >= 1 && fn <= 24) outVk = static_cast<UINT>(VK_F1 + fn - 1);
        } catch (...) {}
    }

    return outVk != 0;
}

static bool RegisterHotkeyFromString(HWND hWnd, const std::string& hotkeyStr, int hotkeyId);

// 从类似 "Ctrl+Alt+A(3)(65)" 字符串中提取并注册全局热键
static bool RegisterHotkeyFromString(HWND hWnd, const std::string& hotkeyStr, int hotkeyId) {
	UINT modifiers = 0;
	UINT vk = 0;
	if (!ParseHotkeyString(hotkeyStr, modifiers, vk)) return false;

	// 取消旧的热键（可选）
	UnregisterHotKey(hWnd, hotkeyId);

	// 注册新的热键
	return RegisterHotKey(hWnd, hotkeyId, modifiers, vk);
}


#ifndef BUILDING_PLUGIN_DLL
inline static UINT g_toggleMainPanelHookModifiers = 0;
inline static UINT g_toggleMainPanelHookVk = 0;
inline static bool g_toggleMainPanelHookUseDoubleClick = false;
inline static bool g_toggleMainPanelHookUseMouse = false;

static UINT NormalizeHotkeyVk(UINT vk) {
	if (vk == VK_LSHIFT || vk == VK_RSHIFT) return VK_SHIFT;
	if (vk == VK_LCONTROL || vk == VK_RCONTROL) return VK_CONTROL;
	if (vk == VK_LMENU || vk == VK_RMENU) return VK_MENU;
	return vk;
}

static UINT GetCurrentHotkeyModifiers() {
	UINT modifiers = 0;
	if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
	if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
	if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) modifiers |= MOD_WIN;
	return modifiers;
}

static UINT NormalizeTriggerModifiers(const UINT vk, UINT modifiers) {
	if (vk == VK_CONTROL) modifiers &= ~MOD_CONTROL;
	else if (vk == VK_SHIFT) modifiers &= ~MOD_SHIFT;
	else if (vk == VK_MENU) modifiers &= ~MOD_ALT;
	else if (vk == VK_LWIN || vk == VK_RWIN) modifiers &= ~MOD_WIN;
	return modifiers;
}

static bool IsMouseHotkeyVk(const UINT vk) {
	return vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2 || vk == VK_LBUTTON || vk == VK_RBUTTON;
}


static void TriggerMainPanelToggleHotkey() {
	if (g_mainHwnd != nullptr) {
		PostMessageW(g_mainHwnd, WM_HOTKEY, HOTKEY_ID_TOGGLE_MAIN_PANEL, 0);
	}
}

static bool TryTriggerMainPanelToggleHook(const UINT modifiers, const UINT vk) {
	const UINT normalizedModifiers = NormalizeTriggerModifiers(vk, modifiers);
	if (normalizedModifiers != g_toggleMainPanelHookModifiers || vk != g_toggleMainPanelHookVk) {
		return false;
	}

	if (!g_toggleMainPanelHookUseDoubleClick) {
		TriggerMainPanelToggleHotkey();
		return true;
	}

	const DWORD now = GetTickCount();
	const UINT doubleClickTime = GetDoubleClickTime();
	if (g_toggleMainPanelHookLastTriggerTick != 0 &&
		now - g_toggleMainPanelHookLastTriggerTick <= doubleClickTime) {
		g_toggleMainPanelHookLastTriggerTick = 0;
		TriggerMainPanelToggleHotkey();
		return true;
	}

	g_toggleMainPanelHookLastTriggerTick = now;
	return false;
}

static LRESULT CALLBACK ToggleMainPanelKeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0 && !g_toggleMainPanelHookUseMouse) {
		const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		if (info != nullptr) {
			const UINT normalizedVk = NormalizeHotkeyVk(static_cast<UINT>(info->vkCode));
			if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
				if (normalizedVk == g_toggleMainPanelHookVk) {
					g_toggleMainPanelHookKeyDown = false;
				}
			} else if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
				if (normalizedVk == g_toggleMainPanelHookVk) {
					if (!g_toggleMainPanelHookKeyDown) {
						g_toggleMainPanelHookKeyDown = true;
						const UINT modifiers = GetCurrentHotkeyModifiers();
						if (TryTriggerMainPanelToggleHook(modifiers, normalizedVk)) {
							return 1;
						}
					}
				} else {
					g_toggleMainPanelHookKeyDown = false;
				}
			}
		}
	}
	return CallNextHookEx(g_toggleMainPanelKeyboardHook, nCode, wParam, lParam);
}

static LRESULT CALLBACK ToggleMainPanelMouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	UNREFERENCED_PARAMETER(lParam);
	if (nCode >= 0 && g_toggleMainPanelHookUseMouse) {
		UINT vk = 0;
		if (wParam == WM_MBUTTONDOWN) {
			vk = VK_MBUTTON;
		} else if (wParam == WM_XBUTTONDOWN) {
			const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
			if (info == nullptr) {
				return CallNextHookEx(g_toggleMainPanelMouseHook, nCode, wParam, lParam);
			}
			const WORD xButton = HIWORD(info->mouseData);
			if (xButton == XBUTTON1) vk = VK_XBUTTON1;
			else if (xButton == XBUTTON2) vk = VK_XBUTTON2;
		}

		if (vk != 0) {
			const UINT modifiers = GetCurrentHotkeyModifiers();
			if (TryTriggerMainPanelToggleHook(modifiers, vk)) {
				return 1;
			}
		}
	}
	return CallNextHookEx(g_toggleMainPanelMouseHook, nCode, wParam, lParam);
}
static void ResetMainPanelToggleHookState() {
	g_toggleMainPanelHookKeyDown = false;
	g_toggleMainPanelHookLastTriggerTick = 0;
}

static void UnregisterMainPanelToggleHotkey(HWND hWnd) {
	UnregisterHotKey(hWnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
	if (g_toggleMainPanelKeyboardHook != nullptr) {
		UnhookWindowsHookEx(g_toggleMainPanelKeyboardHook);
		g_toggleMainPanelKeyboardHook = nullptr;
	}
	if (g_toggleMainPanelMouseHook != nullptr) {
		UnhookWindowsHookEx(g_toggleMainPanelMouseHook);
		g_toggleMainPanelMouseHook = nullptr;
	}
	ResetMainPanelToggleHookState();
}

static bool ConfigureMainPanelToggleHotkey(HWND hWnd, const std::string& mode, const std::string& hotkeyStr) {
	UnregisterMainPanelToggleHotkey(hWnd);

	if (hotkeyStr.empty()) return false;

	if (mode.empty() || mode == "key_combination") {
		return RegisterHotkeyFromString(hWnd, hotkeyStr, HOTKEY_ID_TOGGLE_MAIN_PANEL);
	}

	UINT modifiers = 0;
	UINT vk = 0;
	if (!ParseHotkeyString(hotkeyStr, modifiers, vk)) {
		return false;
	}

	g_toggleMainPanelHookModifiers = modifiers;
	g_toggleMainPanelHookVk = vk;
	g_toggleMainPanelHookUseDoubleClick = (mode == "double_click");
	g_toggleMainPanelHookUseMouse = IsMouseHotkeyVk(vk);
	ResetMainPanelToggleHookState();

	if (g_toggleMainPanelHookUseMouse) {
		g_toggleMainPanelMouseHook = SetWindowsHookExW(WH_MOUSE_LL, ToggleMainPanelMouseHookProc, g_hInst, 0);
		return g_toggleMainPanelMouseHook != nullptr;
	}

	g_toggleMainPanelKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, ToggleMainPanelKeyboardHookProc, g_hInst, 0);
	return g_toggleMainPanelKeyboardHook != nullptr;
}

#else
static void UnregisterMainPanelToggleHotkey(HWND hWnd) {
	UnregisterHotKey(hWnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
}

static bool ConfigureMainPanelToggleHotkey(HWND hWnd, const std::string& mode, const std::string& hotkeyStr) {
	UNREFERENCED_PARAMETER(mode);
	if (hotkeyStr.empty()) {
		UnregisterHotKey(hWnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
		return false;
	}
	return RegisterHotkeyFromString(hWnd, hotkeyStr, HOTKEY_ID_TOGGLE_MAIN_PANEL);
}
#endif
