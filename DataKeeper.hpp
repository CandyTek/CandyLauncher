#pragma once
#include <functional>
#include <Shobjidl.h>
#include <string>

// 将修饰符和虚拟键码打包成一个 64 位键
#define MAKE_HOTKEY_KEY(modifiers, vk) \
(static_cast<UINT64>(modifiers) << 32 | static_cast<UINT64>(vk))

// 定义我们的高性能查找“数组” -> 哈希表，键是 UINT64，值是对应的动作
using HotkeyMap = std::unordered_map<UINT64, UINT64>;

// 全局常驻变量
extern HWND s_mainHwnd;
extern HWND g_settingsHwnd;
extern HKL g_hklIme;
extern UINT_PTR TimerIDSetFocusEdit;

extern HotkeyMap g_hotkeyMap;
extern std::wstring EXE_FOLDER_PATH;
extern std::string pref_force_ime_mode;

extern bool pref_show_window_and_release_modifier_key;
extern bool pref_run_item_as_admin;
extern bool pref_hide_in_fullscreen;
extern bool pref_hide_in_topmost_fullscreen;
extern bool pref_lock_window_popup_position;
extern bool pref_preserve_last_search_term;
extern bool pref_single_click_to_open;
extern bool pref_fuzzy_match;
extern bool pref_last_search_term_selected;
extern bool pref_close_after_open_item;

extern int pref_max_search_results;
extern int last_open_window_position_x;
extern int last_open_window_position_y;

extern float window_position_offset_x;
extern float window_position_offset_y;



// const int COLOR_UI_BG_VALUE = 110;
//const int COLOR_UI_BG_VALUE = 255;
const int COLOR_UI_BG_VALUE = 170;
const int COLOR_UI_BG_DEEP_VALUE = 130;

const COLORREF COLOR_UI_BG = RGB(COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE);
const COLORREF COLOR_UI_BG_DEEP = RGB(COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE);
const int fontColorGray = 50;
const int COLOR_UI_BG_GLASS = (0xCC << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
// const int COLOR_UI_BG_GLASS = (0x22 << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
