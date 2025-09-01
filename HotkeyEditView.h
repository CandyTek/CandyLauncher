#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls
#include <CommCtrl.h>

#include "BaseTools.hpp"
#include "Constants.hpp"
#include "json.hpp"
#include "DataKeeper.hpp"

// 窗口属性键名常量
static const wchar_t* HOTKEY_CTX_OWNER = L"HotkeyCtxOwner";
static const wchar_t* HOTKEY_CTX_KEY = L"HotkeyCtxKey";

// 新增：应用内自定义消息，父窗口收到后去更新 JSON 并注册热键
#ifndef WM_APP_HOTKEY_COMMIT
#define WM_APP_HOTKEY_COMMIT (WM_APP + 0x120)
#endif



static LRESULT CALLBACK HotkeyEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
											   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	// Use window property instead of static variable to avoid cross-control contamination
	static const wchar_t* HOTKEY_PROP = L"CurrentHotkey";


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
			// Clean up existing property first to prevent memory leak
			HGLOBAL hOldMem = (HGLOBAL)GetPropW(hWnd, HOTKEY_PROP);
			if (hOldMem) {
				RemovePropW(hWnd, HOTKEY_PROP);
				GlobalFree(hOldMem);
			}
			// Store hotkey string as window property
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (keyStr.length() + 1) * sizeof(wchar_t));
			if (hMem) {
				wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
				if (pMem) {
					wcscpy_s(pMem, keyStr.length() + 1, keyStr.c_str());
					GlobalUnlock(hMem);
					SetPropW(hWnd, HOTKEY_PROP, hMem);
				}
			}

			return 0;
		}
		case WM_SETFOCUS:
// 编辑开始，取消全局快捷键注册
			UnregisterHotKey(g_mainHwnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
			break;
		case WM_KILLFOCUS: {
        // 结束编辑：只“收集结果并上报”，不要在这里操作 JSON / 重注册
        std::wstring currentHotkey;

        if (HGLOBAL hMem = (HGLOBAL)GetPropW(hWnd, HOTKEY_PROP)) {
            if (wchar_t* pMem = (wchar_t*)GlobalLock(hMem)) {
                currentHotkey.assign(pMem);
                GlobalUnlock(hMem);
            }
            RemovePropW(hWnd, HOTKEY_PROP);
            GlobalFree(hMem);
        } else {
            // 保险：若属性没有（比如外部预填了内容），从控件文本取
            int len = GetWindowTextLengthW(hWnd);
            if (len > 0) {
                currentHotkey.resize(len);
                GetWindowTextW(hWnd, currentHotkey.data(), len + 1);
            }
        }

        // 从窗口属性获取上下文信息
        HWND owner = (HWND)GetPropW(hWnd, HOTKEY_CTX_OWNER);
        HGLOBAL hKeyMem = (HGLOBAL)GetPropW(hWnd, HOTKEY_CTX_KEY);
        
        if (owner && hKeyMem && !currentHotkey.empty()) {
            wchar_t* settingKey = (wchar_t*)GlobalLock(hKeyMem);
            if (settingKey) {
                SendMessageW(owner, WM_APP_HOTKEY_COMMIT,
                           reinterpret_cast<WPARAM>(settingKey),
                           reinterpret_cast<LPARAM>(currentHotkey.c_str()));
                GlobalUnlock(hKeyMem);
            }
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
// 1. 清理窗口属性中存储的内存
			HGLOBAL hMem = (HGLOBAL)GetPropW(hWnd, HOTKEY_PROP);
			if (hMem) {
				RemovePropW(hWnd, HOTKEY_PROP);
				GlobalFree(hMem);
			}

// 2. 清理上下文相关的窗口属性
			HGLOBAL hKeyMem = (HGLOBAL)GetPropW(hWnd, HOTKEY_CTX_KEY);
			if (hKeyMem) {
				RemovePropW(hWnd, HOTKEY_CTX_KEY);
				GlobalFree(hKeyMem);
			}
			RemovePropW(hWnd, HOTKEY_CTX_OWNER);

// 3. 移除窗口子类化
			RemoveWindowSubclass(hWnd, HotkeyEditSubclassProc, uIdSubclass);

// 4. 调用 DefSubclassProc 进行最终清理
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}
	}


	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// —— 安装子类时 ——
// 使用窗口属性存储上下文信息，避免堆分配

static bool InstallHotkeySubclass(HWND hEdit, HWND owner, const std::wstring& key) {
    // 将上下文信息存储为窗口属性
    SetPropW(hEdit, HOTKEY_CTX_OWNER, owner);
    
    // 为键名分配内存并存储
    HGLOBAL hKeyMem = GlobalAlloc(GMEM_MOVEABLE, (key.length() + 1) * sizeof(wchar_t));
    if (hKeyMem) {
        wchar_t* pKeyMem = (wchar_t*)GlobalLock(hKeyMem);
        if (pKeyMem) {
            wcscpy_s(pKeyMem, key.length() + 1, key.c_str());
            GlobalUnlock(hKeyMem);
            SetPropW(hEdit, HOTKEY_CTX_KEY, hKeyMem);
        }
    }
    
    return SetWindowSubclass(hEdit, HotkeyEditSubclassProc, 0, 0);
}
