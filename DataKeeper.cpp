#include "DataKeeper.hpp"
#include "BaseTools.hpp"

// 全局常驻变量
HWND g_settingsHwnd = nullptr;
HWND s_mainHwnd = nullptr;
HKL g_hklIme = nullptr;
UINT_PTR TimerIDSetFocusEdit = 0;

HotkeyMap g_hotkeyMap = {};
std::wstring EXE_FOLDER_PATH=GetExecutableFolder();
std::string pref_force_ime_mode="null";

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

float window_position_offset_x = 0.5;
float window_position_offset_y = 0.5;
