#pragma once

#include <Shobjidl.h>
#include <stdexcept>
#include <string>
#include <unordered_set>

#define WM_EDIT_CONTROL_HOTKEY (WM_USER + 100)
#define WM_EDIT_OPEN_PATH (WM_USER + 101)
#define WM_EDIT_OPEN_TARGET_PATH (WM_USER + 102)
#define WM_WINDOWS_HIDE (WM_USER + 103)
#define WM_CONFIG_SAVED (WM_USER + 104)
#define WM_FOCUS_EDIT (WM_USER + 105)
#define WM_NOTIFY_HEDIT_REFRESH_SKIN (WM_USER + 106)
#define WM_LISTVIEW_REFRESH_RESOURCE (WM_USER + 107)
#define WM_REFRESH_SKIN (WM_USER + 108)
#define WM_REFRESH_SETTING_UI (WM_USER + 109)

#define TIMER_SETFOCUS_EDIT  109
#define TIMER_SET_GLOBAL_HOTKEY  110
#define TIMER_SHOW_WINDOW  111
#define TIMER_DELAY_HIDE_WINDOW 112
#define TIMER_DETERMINE_FILE_DRAG_AND_DROP 113


#define HOTKEY_ID_TOGGLE_MAIN_PANEL (100)

#define HOTKEY_ID_RUN_ITEM (110)
#define HOTKEY_ID_RUN_ITEM_AS_ADMIN (111)

#define HOTKEY_ID_OPEN_FILE_LOCATION (101)
#define HOTKEY_ID_OPEN_TARGET_LOCATION (102)
#define HOTKEY_ID_OPEN_WITH_CLIPBOARD_PARAMS (103)
#define HOTKEY_ID_REFRESH_SKIN (104)
#define HOTKEY_ID_SHOW_SETTING_WINDOW (105)

// Hotkey IDs for Ctrl+numeric keys (1-9)
#define HOTKEY_ID_INVOKE_ITEM_1 (120)
#define HOTKEY_ID_INVOKE_ITEM_2 (121)
#define HOTKEY_ID_INVOKE_ITEM_3 (122)
#define HOTKEY_ID_INVOKE_ITEM_4 (123)
#define HOTKEY_ID_INVOKE_ITEM_5 (124)
#define HOTKEY_ID_INVOKE_ITEM_6 (125)
#define HOTKEY_ID_INVOKE_ITEM_7 (126)
#define HOTKEY_ID_INVOKE_ITEM_8 (127)
#define HOTKEY_ID_INVOKE_ITEM_9 (128)

// 下面的键位没有用
constexpr UINT MOD_CTRL_KEY = 0x02;
constexpr UINT MOD_SHIFT_KEY = 0x04;
constexpr UINT MOD_ALT_KEY = 0x01;
constexpr UINT MOD_WIN_KEY = 0x08;

constexpr UINT MOD_CTRL_SHIFT_KEY = 0x06;
constexpr UINT MOD_CTRL_ALT_KEY = 0x03;
constexpr UINT MOD_CTRL_SHIFT_ALT_KEY = 0x07;
constexpr UINT MOD_SHIFT_ALT_KEY = 0x05;

constexpr UINT MOD_CTRL_WIN_KEY = 10;
constexpr UINT MOD_SHIFT_WIN_KEY = 12;
constexpr UINT MOD_ALT_WIN_KEY = 9;

constexpr UINT MOD_CTRL_SHIFT_WIN_KEY = 14;
constexpr UINT MOD_CTRL_ALT_WIN_KEY = 11;
constexpr UINT MOD_SHIFT_ALT_WIN_KEY = 13;

constexpr UINT MOD_CTRL_SHIFT_ALT_WIN_KEY = 15;


constexpr int SETTINGS_WINDOW_WIDTH = 640;
constexpr int SETTINGS_WINDOW_HEIGHT = 680;


constexpr const char* SETTINGS_PREF_TYPE_STRING = "string";
//constexpr int settings_pref_type_ = 2;

inline std::string GetAppResourceText(int idr) {
	HMODULE hModule = GetModuleHandleW(nullptr);

	HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(idr), RT_RCDATA);
	if (!hRes) throw std::runtime_error("Resource not found");

	HGLOBAL hResLoad = LoadResource(hModule, hRes);
	if (!hResLoad) throw std::runtime_error("LoadResource failed");

	DWORD size = SizeofResource(hModule, hRes);
	const void* pData = LockResource(hResLoad);
	if (!pData) throw std::runtime_error("LockResource failed");

	std::string settingsText(static_cast<const char*>(pData), size);
	return settingsText;
}
