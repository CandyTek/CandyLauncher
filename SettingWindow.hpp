#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include "resource.h"
#include <set>
#include <iostream>

#include "BaseTools.hpp"
#include "MainTools.hpp"
#include "ScrollViewHelper.hpp"
#include "SettingsHelper.hpp"
#include "DataKeeper.hpp"
#include "PinyinHelper.h"

// static std::vector<std::vector<HWND>> hCtrlsByTab; // tab下所有控件句柄
// static std::vector<HWND> tabContainers;
// static std::vector<SettingItem> settings2;
static std::vector<std::wstring> subPages; // 所有tab
static std::vector<HWND> hTabButtons; // tab按钮句柄
static int currentSubPageIndex = 0;

static HFONT hFontDefault = CreateFontW(
		20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");

static HFONT hFontButton = CreateFontW(
		18, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");

static HBRUSH hBrushButtonNormal = CreateSolidBrush(RGB(240, 240, 240));
static HBRUSH hBrushButtonHover = CreateSolidBrush(RGB(230, 230, 230));
static HBRUSH hBrushButtonPressed = CreateSolidBrush(RGB(200, 200, 200));
static HBRUSH hBrushButtonSelected = CreateSolidBrush(RGB(220, 235, 255)); // 选中状态的浅蓝色背景
static HBRUSH hBrushButtonPrimary = CreateSolidBrush(RGB(0, 120, 215));
static HBRUSH hBrushButtonSuccess = CreateSolidBrush(RGB(40, 167, 69));
static HBRUSH hBrushButtonDanger = CreateSolidBrush(RGB(220, 53, 69));

enum ButtonStyle {
	BTN_NORMAL = 0,
	BTN_PRIMARY = 1,
	BTN_SUCCESS = 2,
	BTN_DANGER = 3
};

struct ButtonInfo {
	HWND hwnd;
	ButtonStyle style;
	bool isHovered;
	bool isPressed;
	bool isSelected; // 新增：选中状态，用于tab按钮
	std::wstring text;
};

static std::vector<ButtonInfo> buttons;


static void initGlobalVariable() {
	pref_show_window_and_release_modifier_key = (settingsMap["pref_show_window_and_release_modifier_key"].defValue.get<
			int>() == 1);
	pref_run_item_as_admin = (settingsMap["pref_run_item_as_admin"].defValue.get<int>() == 1);
	pref_hide_in_fullscreen = (settingsMap["pref_hide_in_fullscreen"].defValue.get<int>() == 1);
	pref_hide_in_topmost_fullscreen = (settingsMap["pref_hide_in_topmost_fullscreen"].defValue.get<int>() == 1);
	pref_lock_window_popup_position = (settingsMap["pref_lock_window_popup_position"].defValue.get<int>() == 1);
	pref_preserve_last_search_term = (settingsMap["pref_preserve_last_search_term"].defValue.get<int>() == 1);
	pref_single_click_to_open = (settingsMap["pref_single_click_to_open"].defValue.get<int>() == 1);
	pref_fuzzy_match = (settingsMap["pref_fuzzy_match"].defValue.get<int>() == 1);
	pref_last_search_term_selected = (settingsMap["pref_last_search_term_selected"].defValue.get<int>() == 1);
	pref_close_after_open_item = (settingsMap["pref_close_after_open_item"].defValue.get<int>() == 1);
	pref_force_ime_mode = settingsMap["pref_force_ime_mode"].defValue.get<std::string>();
	pref_max_search_results = settingsMap["pref_max_search_results"].defValue.get<int>();
	pref_hotkey_toggle_main_panel = settingsMap["pref_hotkey_toggle_main_panel"].defValue.get<std::string>();
	pref_fuzzy_match_score_threshold = settingsMap["pref_fuzzy_match_score_threshold"].defValue.get<int>();

	int tempX = settingsMap["pref_window_popup_position_offset_x"].defValue.get<int>();
	int tempY = settingsMap["pref_window_popup_position_offset_y"].defValue.get<int>();
	if (tempX >= 0 && tempX <= 100) {
		window_position_offset_x = static_cast<float>(tempX) / 100.0f;
	}
	if (tempY >= 0 && tempY <= 100) {
		window_position_offset_y = static_cast<float>(tempY) / 100.0f;
	}
}

static void LoadSettingList() {
	const std::string settingsText = GetSettingsJsonText();
	try {
		settings2 = ParseConfig(settingsText);
	}
	catch (...) {
		ShowErrorMsgBox(L"解析基础设置项失败！");
		ExitProcess(1);
	}
	nlohmann::json userConfig = loadUserConfig();
	// 将用户设置合并到主要设置结构中。此此更新在UI构建之前更新每个项目的“ defvalue”。
	if (!userConfig.is_null() && !userConfig.empty()) {
		for (auto &setting: settings2) {
			if (userConfig.contains(setting.key)) {
				// 该键存在一个保存的值。用用户的值将设置的默认值置换。Nlohmann:: JSON库自动处理类型转换。
				setting.defValue = userConfig[setting.key];
			}
		}
	}
	LoadSettingsMap();
	initGlobalVariable();
	const std::string pref_pinyin_mode = settingsMap["pref_pinyin_mode"].defValue.get<std::string>();
	PinyinHelper::changePinyinType(pref_pinyin_mode);
}


static void ShowSettingsWindow(HINSTANCE hInstance, HWND hParent) {
	if (g_settingsHwnd != nullptr) {
		ShowWindow(g_settingsHwnd, SW_SHOW);
		return;
	}
	// 获取屏幕宽度和高度
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 计算居中位置
	int x = (screenWidth - SETTINGS_WINDOW_WIDTH) / 2;
	int y = (screenHeight - SETTINGS_WINDOW_HEIGHT) / 2;

	g_settingsHwnd = CreateWindowExW(
			WS_EX_ACCEPTFILES, // 扩展样式
			L"SettingsWndClass", // 窗口类名
			L"设置", // 窗口标题
			WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // 窗口样式
			x, y, // 初始位置
			SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT, // 宽度和高度
			hParent, // 父窗口
			nullptr, // 菜单
			hInstance, // 实例句柄
			nullptr // 附加参数
	);

	ShowWindow(g_settingsHwnd, SW_SHOW);
	UpdateWindow(g_settingsHwnd);
}

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
			UnregisterHotKey(s_mainHwnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
			break;
		case WM_KILLFOCUS: {
			// 编辑结束，重新注册全局快捷键，这里不能直接去访问settingmap列表的该key，不然会引起报错
			RegisterHotkeyFromString(s_mainHwnd, pref_hotkey_toggle_main_panel, HOTKEY_ID_TOGGLE_MAIN_PANEL);

			std::string hotkeyUtf8 = wide_to_utf8(currentHotkey);
			if (pItemValue)
				*pItemValue = nlohmann::json(hotkeyUtf8); // Sicher
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
			if (pItemValue) {
				delete pItemValue;
			}
			// 2. 移除窗口子类，这非常重要！
			RemoveWindowSubclass(hWnd, HotkeyEditSubclassProc, uIdSubclass);

			// 在这里必须调用 DefSubclassProc
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}
	}


	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static ButtonInfo *FindButtonInfo(HWND hwnd) {
	for (auto &btn: buttons) {
		if (btn.hwnd == hwnd) {
			return &btn;
		}
	}
	return nullptr;
}

// 添加缓存的悬停状态画刷
static HBRUSH hBrushPrimaryHover = CreateSolidBrush(RGB(0, 100, 195));
static HBRUSH hBrushSuccessHover = CreateSolidBrush(RGB(20, 147, 49));
static HBRUSH hBrushDangerHover = CreateSolidBrush(RGB(200, 33, 49));

static HBRUSH GetButtonBrush(ButtonInfo *btnInfo) {
	if (!btnInfo) return hBrushButtonNormal;

	if (btnInfo->isPressed) {
		return hBrushButtonPressed;
	}

	// 如果是选中状态的普通按钮（tab按钮），使用选中背景
	if (btnInfo->style == BTN_NORMAL && btnInfo->isSelected) {
		return hBrushButtonSelected;
	}

	switch (btnInfo->style) {
		case BTN_PRIMARY:
			return btnInfo->isHovered ? hBrushPrimaryHover : hBrushButtonPrimary;
		case BTN_SUCCESS:
			return btnInfo->isHovered ? hBrushSuccessHover : hBrushButtonSuccess;
		case BTN_DANGER:
			return btnInfo->isHovered ? hBrushDangerHover : hBrushButtonDanger;
		default:
			return btnInfo->isHovered ? hBrushButtonHover : hBrushButtonNormal;
	}
}

static LRESULT CALLBACK EnhancedButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
												   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	ButtonInfo *btnInfo = FindButtonInfo(hWnd);

	switch (uMsg) {
		case WM_MOUSEMOVE: {
			if (btnInfo && !btnInfo->isHovered) {
				btnInfo->isHovered = true;
				InvalidateRect(hWnd, nullptr, TRUE);

				TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT)};
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				TrackMouseEvent(&tme);
			}
			break;
		}
		case WM_MOUSELEAVE: {
			if (btnInfo) {
				btnInfo->isHovered = false;
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			break;
		}
		case WM_LBUTTONDOWN: {
			if (btnInfo) {
				btnInfo->isPressed = true;
				InvalidateRect(hWnd, nullptr, TRUE);
				SetCapture(hWnd);
			}
			break;
		}
		case WM_LBUTTONUP: {
			if (btnInfo) {
				btnInfo->isPressed = false;
				InvalidateRect(hWnd, nullptr, TRUE);
				ReleaseCapture();
				// 模拟按钮点击
				HWND hParent = GetParent(hWnd);
				if (hParent) {
					SendMessage(hParent, WM_COMMAND,
								MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED),
								(LPARAM) hWnd);
				}
			}
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			RECT rect;
			GetClientRect(hWnd, &rect);

			// 使用双缓冲减少闪烁
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			HBITMAP oldBitmap = (HBITMAP) SelectObject(memDC, memBitmap);

			HBRUSH hBrush = GetButtonBrush(btnInfo);
			FillRect(memDC, &rect, hBrush);

			// 如果是选中状态的普通按钮（tab按钮），绘制左侧蓝色竖条
			if (btnInfo && btnInfo->style == BTN_NORMAL && btnInfo->isSelected) {
				HBRUSH hBlueBrush = CreateSolidBrush(RGB(0, 120, 215));
				RECT blueRect = {0, 0, 3, rect.bottom}; // 3像素宽的蓝色竖条
				FillRect(memDC, &blueRect, hBlueBrush);
				DeleteObject(hBlueBrush);
			}

			SetBkMode(memDC, TRANSPARENT);
			HFONT oldFont = (HFONT) SelectObject(memDC, hFontButton);

			if (btnInfo && btnInfo->style != BTN_NORMAL) {
				SetTextColor(memDC, RGB(255, 255, 255));
			} else {
				SetTextColor(memDC, RGB(0, 0, 0));
			}

			if (btnInfo && !btnInfo->text.empty()) {
				DrawTextW(memDC, btnInfo->text.c_str(), -1, &rect,
						  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}

			// 一次性复制到屏幕
			BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

			SelectObject(memDC, oldFont);
			SelectObject(memDC, oldBitmap);
			DeleteObject(memBitmap);
			DeleteDC(memDC);

			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_NCDESTROY: {
			RemoveWindowSubclass(hWnd, EnhancedButtonSubclassProc, uIdSubclass);
			break;
		}
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static HWND CreateEnhancedButton(HWND hParent, const std::wstring &text, int x, int y,
								 int width, int height, HMENU hMenu, ButtonStyle style = BTN_NORMAL) {
	HWND hButton = CreateWindowW(L"BUTTON", text.c_str(),
//								WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
								 WS_CHILD | WS_VISIBLE,
								 x, y, width, height, hParent, hMenu, nullptr, nullptr);

	if (hButton) {
		ButtonInfo btnInfo;
		btnInfo.hwnd = hButton;
		btnInfo.style = style;
		btnInfo.isHovered = false;
		btnInfo.isPressed = false;
		btnInfo.isSelected = false;
		btnInfo.text = text;
		buttons.push_back(btnInfo);

		SetWindowSubclass(hButton, EnhancedButtonSubclassProc,
						  static_cast<UINT_PTR>(buttons.size()), 0);
		SendMessage(hButton, WM_SETFONT, (WPARAM) hFontButton, TRUE);
	}

	return hButton;
}

// 为Tab按钮创建轻量级版本，避免复杂绘制
static HWND CreateSimpleTabButton(HWND hParent, const std::wstring &text, int x, int y,
								  int width, int height, HMENU hMenu) {
	HWND hButton = CreateWindowW(L"BUTTON", text.c_str(),
								 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
								 x, y, width, height, hParent, hMenu, nullptr, nullptr);

	if (hButton) {
		SendMessage(hButton, WM_SETFONT, (WPARAM) hFontDefault, TRUE);
	}

	return hButton;
}

// 更新 tab 按钮的选中状态
static void UpdateTabButtonSelection(int selectedIndex) {
	for (size_t i = 0; i < hTabButtons.size(); ++i) {
		ButtonInfo *btnInfo = FindButtonInfo(hTabButtons[i]);
		if (btnInfo) {
			bool shouldBeSelected = (static_cast<int>(i) == selectedIndex);
			if (btnInfo->isSelected != shouldBeSelected) {
				btnInfo->isSelected = shouldBeSelected;
				InvalidateRect(hTabButtons[i], nullptr, TRUE);
			}
		}
	}
}

static void CleanupButtonResources() {
	DeleteObject(hFontButton);
	DeleteObject(hBrushButtonNormal);
	DeleteObject(hBrushButtonHover);
	DeleteObject(hBrushButtonPressed);
	DeleteObject(hBrushButtonSelected);
	DeleteObject(hBrushButtonPrimary);
	DeleteObject(hBrushButtonSuccess);
	DeleteObject(hBrushButtonDanger);
	buttons.clear();
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			MyScrollViewRegisterClass();
			// --------- 1. 收集所有subPage名 ---------
			std::set<std::wstring> tabSet;
			for (const auto &sub: settings2) {
				std::wstring name = utf8_to_wide(sub.subPage);
				if (tabSet.insert(name).second) {
					// insert() 返回 pair<iterator, bool>，第二个为true表示是新插入
					subPages.push_back(name);
				}
			}

			// --------- 2. 创建Tab按钮（左侧） ---------
			int tabY = 10;
			for (size_t i = 0; i < subPages.size(); ++i) {
				std::wstring label = tabNameMap.count(subPages[i]) ? tabNameMap[subPages[i]] : subPages[i];

				HWND hTabBtn = CreateEnhancedButton(hwnd, label, 10, tabY, 80, 35,
													(HMENU) (2000 + i), BTN_NORMAL);
				hTabButtons.push_back(hTabBtn);
				tabY += 45;
			}

			// 3. 创建每个Tab的容器
			tabContainers.resize(subPages.size());
			for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
				HWND hContainer = CreateWindowExW(
						WS_EX_ACCEPTFILES | WS_EX_COMPOSITED,
						L"SCROLLVIEW", nullptr,
						WS_CHILD | WS_VISIBLE | WS_VSCROLL,
						100, 0, 600, 380, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
				tabContainers[tabIdx] = hContainer;
			}

			// 4. 在每个容器中生成控件
			hCtrlsByTab.clear();
			hCtrlsByTab.resize(subPages.size());
			for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
				int y = 10;
				int contorlX = 200;
				std::vector<HWND> ctrls;
				HWND hParent = tabContainers[tabIdx];

				for (size_t i = 0; i < settings2.size(); ++i) {
					const auto &item = settings2[i];
					if (utf8_to_wide(item.subPage) != subPages[tabIdx])
						continue;

					std::wstring text = utf8_to_wide(item.title);

					// label
					HWND hLabel;
					int labelHeight;
					if (item.type == "text") {
						int labelWidth = 300;
						labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
						hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
											   WS_CHILD | WS_VISIBLE,
											   10, y, labelWidth, labelHeight, hParent, nullptr, nullptr, nullptr);


						SendMessageW(hLabel, WM_SETFONT, (WPARAM) hFontDefault, TRUE);
						y += labelHeight;
						continue;
					} else {
						int labelWidth = 180;
						labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
						hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
											   WS_CHILD | WS_VISIBLE,
											   10, y, labelWidth, labelHeight, hParent, nullptr, nullptr, nullptr);
						SendMessageW(hLabel, WM_SETFONT, (WPARAM) hFontDefault, TRUE);
					}

					ctrls.push_back(hLabel);

					HWND hCtrl = nullptr;
					if (item.type == "bool") {
						hCtrl = CreateWindowW(L"BUTTON", L"",
											  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
						// WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
											  contorlX, y, 20, 25, hParent, (HMENU) (3000 + i), nullptr,
											  nullptr);
						if (item.defValue.is_number_integer() && item.defValue.get<int>() == 1)
							SendMessage(hCtrl, BM_SETCHECK, BST_CHECKED, 0);
					} else if (item.type == "string") {
						hCtrl = CreateWindowW(L"EDIT",
											  utf8_to_wide(item.defValue.get<std::string>()).c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
											  contorlX, y, 200, 25, hParent, (HMENU) (3000 + i), nullptr,
											  nullptr);
					} else if (item.type == "stringArr") {
						std::wstring defVal;
						for (auto &s: item.defValue)
							defVal += utf8_to_wide(s.get<std::string>()) + L"\r\n";
						hCtrl = CreateWindowW(L"EDIT",
											  defVal.c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | WS_VSCROLL,
											  contorlX, y, 200, 50, hParent, (HMENU) (3000 + i), nullptr,
											  nullptr);
						y += 30;
					} else if (item.type == "list") {
						hCtrl = CreateWindowW(L"COMBOBOX", L"",
											  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL |
											  CBS_NOINTEGRALHEIGHT,
											  contorlX, y, 200, 300, hParent, (HMENU) (3000 + i),
											  nullptr, nullptr);
						// 1. 獲取 ComboBox 的詳細資訊，主要是為了得到下拉列表的句柄 (hwndList)
						COMBOBOXINFO cbi = {sizeof(COMBOBOXINFO)};
						GetComboBoxInfo(hCtrl, &cbi);
						if (cbi.hwndCombo) // 確保列表句柄有效
						{
							SetWindowSubclass(cbi.hwndCombo, ComboBoxListSubclassProc, 1, (DWORD_PTR) hParent);
						}
						// SetWindowSubclass(hCtrl, ComboBoxListSubclassProc, 2, (DWORD_PTR)hParent);

						const auto &entries = item.entries;
						const auto &entryValues = item.entryValues;
						int selIndex = 0;

						for (size_t j = 0; j < entries.size(); ++j) {
							std::wstring entryW = utf8_to_wide(entries[j]);
							SendMessageW(hCtrl, CB_ADDSTRING, 0, (LPARAM) entryW.c_str());

							if (entryValues[j] == item.defValue)
								selIndex = static_cast<int>(j);
						}
						SendMessageW(hCtrl, CB_SETCURSEL, selIndex, 0);
					} else if (item.type == "long") {
						hCtrl = CreateWindowW(L"EDIT",
											  std::to_wstring(item.defValue.get<long>()).c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_NUMBER,
											  contorlX, y, 100, 25, hParent, (HMENU) (3000 + i),
											  nullptr, nullptr);
					} else if (item.type == "text") {
					} else if (item.type == "hotkeystring") {
						// 在堆上创建副本
						auto *pItemData = new nlohmann::json(item.defValue);

						hCtrl = CreateWindowW(L"EDIT",
											  utf8_to_wide(item.defValue.get<std::string>()).c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
											  contorlX, y, 200, 25, hParent, (HMENU) (3000 + i),
											  nullptr, nullptr);

						SetWindowSubclass(hCtrl, HotkeyEditSubclassProc, 3, (DWORD_PTR) &pItemData); // 注册子类以处理按键
					} else if (item.type == "button") {
						ButtonStyle btnStyle = BTN_NORMAL;
						if (item.defValue.get<std::string>() == "backup_settings") {
							btnStyle = BTN_SUCCESS;
						} else if (item.defValue.get<std::string>() == "restore_settings") {
							btnStyle = BTN_DANGER;
						}

						hCtrl = CreateEnhancedButton(hParent, utf8_to_wide(item.title),
													 contorlX, y, 150, 30, (HMENU) (3000 + i), btnStyle);
					}

					ctrls.push_back(hCtrl);
					y += MyMax(35, labelHeight);
				}

				int contentHeight = y + 20; // y 是最后一个控件的底部位置
				int visibleHeight = 380;

				int maxScroll = MyMax(0, contentHeight - visibleHeight);
				SetScrollRange(hParent, SB_VERT, 0, maxScroll, TRUE);
				SetScrollPos(hParent, SB_VERT, 0, TRUE);

				hCtrlsByTab[tabIdx] = ctrls;
			}

			// --------- 5. 只显示当前tab内容 ---------
			for (size_t tabIdx = 0; tabIdx < tabContainers.size(); ++tabIdx)
				ShowWindow(tabContainers[tabIdx], tabIdx == 0 ? SW_SHOW : SW_HIDE);

			// --------- 设置第一个tab为选中状态 ---------
			UpdateTabButtonSelection(0);

			// --------- 6. 保存按钮 ---------
			CreateEnhancedButton(hwnd, L"保存", 350, 400, 80, 35, (HMENU) 9999, BTN_PRIMARY);

			// --------- 7. 重置按钮 ---------
			CreateEnhancedButton(hwnd, L"重置", 440, 400, 80, 35, (HMENU) 9998, BTN_DANGER);

			// --------- 8. 应用按钮 ---------
			CreateEnhancedButton(hwnd, L"应用", 530, 400, 80, 35, (HMENU) 9997, BTN_SUCCESS);
			break;
		}
		case WM_MOUSEWHEEL:
			SendMessage(tabContainers[currentSubPageIndex], WM_MOUSEWHEEL, wParam, lParam);
			break;
		case WM_CTLCOLORSTATIC: {
			HDC hdcStatic = (HDC) wParam;
			SetBkMode(hdcStatic, TRANSPARENT); // 设置背景透明
			return (INT_PTR) GetStockObject(NULL_BRUSH); // 返回一个透明画刷
		}

		case WM_COMMAND:
			// --- 切换Tab ---
			if (LOWORD(wParam) >= 2000 && LOWORD(wParam) < 2000 + hTabButtons.size()) {
				int newIdx = LOWORD(wParam) - 2000;
				if (newIdx == currentSubPageIndex)
					break;

				ShowWindow(tabContainers[currentSubPageIndex], SW_HIDE);
				ShowWindow(tabContainers[newIdx], SW_SHOW);
				currentSubPageIndex = newIdx;

				// 更新tab按钮的选中状态
				UpdateTabButtonSelection(newIdx);

				break;
			}
				// --- 保存 ---
			else if (LOWORD(wParam) == 9999) {
				saveConfig(hwnd, subPages, settings2, hCtrlsByTab);
				break;
			}
				// --- 重置 ---
			else if (LOWORD(wParam) == 9998) {
				int result = MessageBoxW(hwnd, L"确定要重置所有设置为默认值吗？", L"确认重置",
										 MB_YESNO | MB_ICONQUESTION);
				if (result == IDYES) {
					resetToDefaults(hwnd, subPages, settings2, hCtrlsByTab);
				}
				break;
			}
				// --- 应用 ---
			else if (LOWORD(wParam) == 9997) {
				applySettings(hwnd, subPages, settings2, hCtrlsByTab);
				break;
			}
				// --- 处理功能按钮 ---
			else if (LOWORD(wParam) >= 3000 && LOWORD(wParam) < 3000 + settings2.size()) {
				int itemIndex = LOWORD(wParam) - 3000;
				if (itemIndex < static_cast<int>(settings2.size())) {
					const auto &item = settings2[itemIndex];
					if (item.type == "button") {
						handleButtonAction(hwnd, item.defValue.get<std::string>());
					}
				}
				break;
			}
		case WM_DESTROY:
			CleanupButtonResources();
			[[fallthrough]];
		case WM_CLOSE: {
			ShowWindow(hwnd, SW_HIDE);
			return 0;
		}
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
