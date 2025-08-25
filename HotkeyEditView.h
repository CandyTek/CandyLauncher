#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls
#include <CommCtrl.h>

#include "BaseTools.hpp"
#include "Constants.hpp"
#include "json.hpp"
#include "DataKeeper.hpp"

static LRESULT CALLBACK HotkeyEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
											   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	static std::wstring currentHotkey;
// auto* pItem = reinterpret_cast<nlohmann::json*>(dwRefData);
	auto *pItemValue = reinterpret_cast<nlohmann::json *>(dwRefData);


	switch (uMsg) {
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN: {
// 检测按键和组合键
			UINT modifiers = 0;
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
			if (GetAsyncKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
			if (GetAsyncKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
			if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) modifiers |= MOD_WIN;

			UINT key = (UINT) wParam;

			if (key == VK_CONTROL || key == VK_SHIFT || key == VK_MENU || key == VK_LWIN || key == VK_RWIN)
				return 0; // 忽略单独的修饰键

			std::wstring keyStr;

			if (modifiers & MOD_CONTROL) keyStr += L"Ctrl+";
			if (modifiers & MOD_ALT) keyStr += L"Alt+";
			if (modifiers & MOD_SHIFT) keyStr += L"Shift+";
			if (modifiers & MOD_WIN) keyStr += L"Win+";

// 获取键名
			wchar_t keyName[64] = {0};
			UINT scanCode = MapVirtualKey(key, MAPVK_VK_TO_VSC) << 16;
			GetKeyNameTextW(scanCode, keyName, 64);
			keyStr += keyName;
			keyStr += L"(";
			keyStr += std::to_wstring(modifiers);
			keyStr += L")";
			keyStr += L"(";
			keyStr += std::to_wstring(key);
			keyStr += L")";

			SetWindowTextW(hWnd, keyStr.c_str());
			currentHotkey = keyStr;

			return 0;
		}
		case WM_SETFOCUS:
// 编辑开始，取消全局快捷键注册
			UnregisterHotKey(g_mainHwnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
			break;
		case WM_KILLFOCUS: {
// 编辑结束，重新注册全局快捷键，这里不能直接去访问settingmap列表的该key，不然会引起报错
			RegisterHotkeyFromString(g_mainHwnd, pref_hotkey_toggle_main_panel, HOTKEY_ID_TOGGLE_MAIN_PANEL);
			if (!currentHotkey.empty() && pItemValue) {
				std::string hotkeyUtf8 = wide_to_utf8(currentHotkey);
				*pItemValue = nlohmann::json(hotkeyUtf8);
			}
			break;
		}
		case WM_ERASEBKGND:
// 可以让系统处理或自己填充背景
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);

		case WM_PAINT:
// 不做特殊处理时，调用默认处理
			break;
// +++ 新增部分：处理窗口销毁 +++
		case WM_NCDESTROY: {
// 1. 释放之前用 new 分配的内存
//			if (pItemValue) {
//				delete pItemValue;
//			}
// 2. 移除窗口子类，这非常重要！
			RemoveWindowSubclass(hWnd, HotkeyEditSubclassProc, uIdSubclass);

// 在这里必须调用 DefSubclassProc
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}
	}


	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
