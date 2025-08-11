#pragma once
#include <functional>
#include <Shobjidl.h>
#include <string>

#include "json.hpp"
#include "BaseTools.hpp"

// 将修饰符和虚拟键码打包成一个 64 位键
#define MAKE_HOTKEY_KEY(modifiers, vk) \
(static_cast<UINT64>(modifiers) << 32 | static_cast<UINT64>(vk))

// 定义我们的高性能查找“数组” -> 哈希表，键是 UINT64，值是对应的动作
using HotkeyMap = std::unordered_map<UINT64, UINT64>;

// 配置项结构
struct SettingItem
{
	std::string key;
	std::string type;
	std::string title;
	std::string subPage;
	std::vector<std::string> entries;
	std::vector<std::string> entryValues;
	nlohmann::json defValue;
};


// 全局常驻变量
extern HWND s_mainHwnd;
extern HWND g_settingsHwnd;
extern HKL g_hklIme;
extern UINT_PTR TimerIDSetFocusEdit;

extern HotkeyMap g_hotkeyMap;
extern std::vector<SettingItem> settings2;
extern std::unordered_map<std::string, SettingItem> settingsMap;

extern std::wstring EXE_FOLDER_PATH;
extern std::string pref_force_ime_mode;
extern std::string pref_hotkey_toggle_main_panel;

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
extern int unknown_file_icon_index;

extern float window_position_offset_x;
extern float window_position_offset_y;

extern int lastWindowCenterX ;
extern int lastWindowCenterY ;


// const int COLOR_UI_BG_VALUE = 110;
//const int COLOR_UI_BG_VALUE = 255;
extern int LISTITEM_ICON_SIZE;
extern int pref_fuzzy_match_score_threshold;


const int COLOR_UI_BG_VALUE = 170;
const int COLOR_UI_BG_DEEP_VALUE = 130;

const COLORREF COLOR_UI_BG = RGB(COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE);
const COLORREF COLOR_UI_BG_DEEP = RGB(COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE);
const int fontColorGray = 50;
const int COLOR_UI_BG_GLASS = (0x22 << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
// const int COLOR_UI_BG_GLASS = (0xCC << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
// const int COLOR_UI_BG_GLASS = (0x22 << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;


// 皮肤
extern nlohmann::json g_skinJson;
extern Gdiplus::Image* g_BgImage;
extern Gdiplus::Image* g_editBgImage;
extern Gdiplus::Image* g_listViewBgImage;
extern Gdiplus::Image* g_listItemBgImage;
extern Gdiplus::Image* g_listItemBgImageSelected;
extern Gdiplus::Color g_listItemBgColor;
extern Gdiplus::Color g_listItemBgColorSelected ;

// extern Gdiplus::Color g_listItemBgColor;
// extern Gdiplus::Color g_listItemBgColorSelected;

extern std::unique_ptr<Gdiplus::SolidBrush> g_listItemBgColorBrush;
extern std::unique_ptr<Gdiplus::SolidBrush> g_listItemBgColorBrushSelected;



extern std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrush1;
extern std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrush2;
extern std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrushSelected1;
extern std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrushSelected2;

extern std::unique_ptr<Gdiplus::Font> g_listItemFont1;
extern std::unique_ptr<Gdiplus::Font> g_listItemFont2;
extern std::unique_ptr<Gdiplus::Font> g_listItemFontSelected1;
extern std::unique_ptr<Gdiplus::Font> g_listItemFontSelected2;

extern  int g_lastWindowOpacity;

extern  int g_itemIconX;
extern  int g_itemIconY;
extern  int g_itemIconSelectedX;
extern  int g_itemIconSelectedY;

extern  int g_itemTextPosX1;
extern  int g_itemTextPosY1;
extern  int g_itemTextPosX2;
extern  int g_itemTextPosY2;
extern  int g_itemTextPosSelectedX1;
extern  int g_itemTextPosSelectedY1;
extern  int g_itemTextPosSelectedX2;
extern  int g_itemTextPosSelectedY2;

extern  int g_itemListWidth;
extern  int g_itemListHeight;

extern double g_item_font_size_1;
extern double g_item_font_size_2;
extern double g_item_font_size_selected_1;
extern double g_item_font_size_selected_2;

extern int MAIN_WINDOW_WIDTH ;
extern int MAIN_WINDOW_HEIGHT ;

constexpr int DEFAULT_MAIN_WINDOW_WIDTH = 620;
constexpr int DEFAULT_MAIN_WINDOW_HEIGHT = 480;
