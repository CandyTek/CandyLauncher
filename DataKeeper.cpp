﻿#include "DataKeeper.hpp"

// 全局常驻变量
HWND g_settingsHwnd = nullptr;
HWND s_mainHwnd = nullptr;
HKL g_hklIme = nullptr;
UINT_PTR TimerIDSetFocusEdit = 0;

HotkeyMap g_hotkeyMap = {};
std::vector<SettingItem> settings2 = {};
std::unordered_map<std::string, SettingItem> settingsMap = {};
std::wstring EXE_FOLDER_PATH = GetExecutableFolder();
std::string pref_force_ime_mode = "null";
std::string pref_hotkey_toggle_main_panel;

bool pref_show_window_and_release_modifier_key = false;
bool pref_run_item_as_admin = false;
bool pref_hide_in_fullscreen = false;
bool pref_hide_in_topmost_fullscreen = false;
bool pref_lock_window_popup_position = false;
bool pref_preserve_last_search_term = false;
bool pref_single_click_to_open = false;
bool pref_fuzzy_match = false;
bool pref_last_search_term_selected = true;
bool pref_close_after_open_item = true;


int pref_max_search_results = 0;
int last_open_window_position_x = 0;
int last_open_window_position_y = 0;
int unknown_file_icon_index = 0;
int LISTITEM_ICON_SIZE = 48;

float window_position_offset_x = 0.5;
float window_position_offset_y = 0.5;


// 皮肤
nlohmann::json g_skinJson = nullptr;
Gdiplus::Image* g_BgImage = nullptr;
Gdiplus::Image* g_editBgImage = nullptr;
Gdiplus::Image* g_listViewBgImage = nullptr;
Gdiplus::Image* g_listItemBgImage = nullptr;
Gdiplus::Image* g_listItemBgImageSelected = nullptr;

std::unique_ptr<Gdiplus::SolidBrush> g_listItemBgColorBrush = nullptr;
std::unique_ptr<Gdiplus::SolidBrush> g_listItemBgColorBrushSelected = nullptr;

std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrush1 = nullptr;
std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrush2 = nullptr;
std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrushSelected1 = nullptr;
std::unique_ptr<Gdiplus::SolidBrush> g_listItemTextColorBrushSelected2 = nullptr;

std::unique_ptr<Gdiplus::Font> g_listItemFont1 = nullptr;
std::unique_ptr<Gdiplus::Font> g_listItemFont2 = nullptr;
std::unique_ptr<Gdiplus::Font> g_listItemFontSelected1 = nullptr;
std::unique_ptr<Gdiplus::Font> g_listItemFontSelected2 = nullptr;

int g_itemIconX = 4;
int g_itemIconY = 4;
int g_itemIconSelectedX = 4;
int g_itemIconSelectedY = 4;

int g_itemTextPosX1 = 60;
int g_itemTextPosY1 = 4;
int g_itemTextPosX2 = 60;
int g_itemTextPosY2 = 22;
int g_itemTextPosSelectedX1 = 60;
int g_itemTextPosSelectedY1 = 4;
int g_itemTextPosSelectedX2 = 60;
int g_itemTextPosSelectedY2 = 22;

int g_itemListWidth = 530;
int g_itemListHeight = 380;

double g_item_font_size_1 = 14.0;
double g_item_font_size_2 = 12.0;
double g_item_font_size_selected_1 = 14.0;
double g_item_font_size_selected_2 = 12.0;

