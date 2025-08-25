// 索引 索引
// 保存按钮
// Tab容器
// 创建Tab按钮

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
#include "CustomButtonHelper.hpp"
#include "HotkeyEditView.h"

// static std::vector<std::vector<HWND>> hCtrlsByTab; // tab下所有控件句柄
// static std::vector<HWND> tabContainers;
// static std::vector<SettingItem> settings2;
inline std::vector<std::wstring> subPages; // 所有tab
inline std::vector<HWND> hTabButtons; // tab按钮句柄
inline int currentSubPageIndex = 0;


static std::map<std::wstring, std::wstring> tabNameMap = {
		{L"normal",  L"常规"},
		{L"feature", L"功能"},
		{L"other",   L"其他"},
		{L"hotkey",  L"快捷键"},
		{L"index",   L"索引"},
		{L"about",   L"关于"}
};

inline HFONT pref_edit_hfont = nullptr;


static void InitializeSettingWindowResources() {
	if (!pref_edit_hfont) {
		pref_edit_hfont = CreateFontW(
			-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");
	}
}


static void ShowSettingsWindow(HINSTANCE hInstance, HWND hParent) {
	if (g_settingsHwnd != nullptr) {
		ShowWindow(g_settingsHwnd, SW_SHOW);
		return;
	}
	
	// 清理静态变量，防止重复创建控件
	subPages.clear();
	hTabButtons.clear();
	tabContainers.clear();
	hCtrlsByTab.clear();
	currentSubPageIndex = 0;
	
	// 初始化绘图资源
	InitializeSettingWindowResources();
	
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


static void CleanupSettingWindowResources() {
	// 清理字体
	if (pref_edit_hfont) { DeleteObject(pref_edit_hfont); pref_edit_hfont = nullptr; }
}

static int FindTabIndexByName(const std::wstring &name) {
	for (size_t i = 0; i < subPages.size(); ++i) {
		if (subPages[i] == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}


/**
 * 更改tab页，使用模拟点击的方法
 * @param tabName 
 */
static void SwitchToTab(const std::wstring &tabName) {
	int newIdx = FindTabIndexByName(tabName);
	if (newIdx >= 0 && newIdx < static_cast<int>(hTabButtons.size())) {
		HWND hBtn = hTabButtons[newIdx];
		HWND hParent = GetParent(hBtn);

		// 模拟用户点击按钮，发送 WM_COMMAND
		SendMessage(hParent, WM_COMMAND,
					MAKEWPARAM(GetDlgCtrlID(hBtn), BN_CLICKED),
					(LPARAM)hBtn);
	}
}


static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			MyScrollViewRegisterClass();
			// --------- 1. 收集所有subPage名 ---------
			std::set<std::wstring> tabSet;
			for (const auto &sub: g_settings2) {
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

//				HWND hTabBtn = CreateWindowW(L"BUTTON", label.c_str(),
//											 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
//											 0, tabY, 120, 46, hwnd, (HMENU) (2000 + i), nullptr, nullptr);
				HWND hTabBtn = CreateEnhancedButton(hwnd, (2000 + i), label, 0, tabY, 120, 46,
													(HMENU) (2000 + i), BTN_NORMAL);

				hTabButtons.push_back(hTabBtn);
				tabY += 48;
			}

			// 3. 创建每个Tab容器
			tabContainers.resize(subPages.size());
			for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
				HWND hContainer = CreateWindowExW(
						WS_EX_ACCEPTFILES | WS_EX_COMPOSITED,
						L"SCROLLVIEW", nullptr,
						WS_CHILD | WS_VISIBLE | WS_VSCROLL,
						124, 0, 600, 380, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
				// 设置为白色
//				SetClassLongPtr(hContainer, GCLP_HBRBACKGROUND, (LONG_PTR)(HBRUSH)(COLOR_WINDOW+1));
				tabContainers[tabIdx] = hContainer;
			}

			// 4. 在每个容器中生成控件
			hCtrlsByTab.clear();
			hCtrlsByTab.resize(subPages.size());
			for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx) {
				int y = 10;
				int contorlX = 224;
				std::vector<HWND> ctrls;
				HWND hParent = tabContainers[tabIdx];

				for (size_t i = 0; i < g_settings2.size(); ++i) {
					const auto &item = g_settings2[i];
					if (utf8_to_wide(item.subPage) != subPages[tabIdx])
						continue;

					std::wstring text = utf8_to_wide(item.title);

					// label
					HWND hLabel;
					int labelHeight;
					if (item.type == "text") {
						int labelWidth = 420;
						labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
						hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
											   WS_CHILD | WS_VISIBLE,
											   10, y, labelWidth, labelHeight, hParent, nullptr, nullptr, nullptr);


						SendMessageW(hLabel, WM_SETFONT, (WPARAM) hFontDefault, TRUE);
						y += labelHeight;
						continue;
					} else {
						int labelWidth = 220;
						labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
						hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
											   WS_CHILD | WS_VISIBLE,
											   10, y, labelWidth, labelHeight, hParent, nullptr, nullptr, nullptr);
						SendMessageW(hLabel, WM_SETFONT, (WPARAM) hFontDefault, TRUE);
					}

					ctrls.push_back(hLabel);

					HWND hCtrl = nullptr;
					if (item.type == "bool") {
						hCtrl = CreateSwitchControl(hParent, (3000 + i), contorlX, y, 50, 25, 
												   (HMENU) (3000 + i), item.boolValue);
					} else if (item.type == "string") {
						hCtrl = CreateWindowW(L"EDIT",
											  utf8_to_wide(item.stringValue).c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
											  contorlX, y, 200, 25, hParent, (HMENU) (3000 + i), nullptr,
											  nullptr);
						SendMessageW(hCtrl, WM_SETFONT, (WPARAM)pref_edit_hfont, TRUE);
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
											  std::to_wstring(item.intValue).c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_NUMBER,
											  contorlX, y, 100, 25, hParent, (HMENU) (3000 + i),
											  nullptr, nullptr);
					} else if (item.type == "text") {
					} else if (item.type == "hotkeystring") {
						// 在堆上创建副本
						auto *pItemData = new nlohmann::json(item.defValue);

						hCtrl = CreateWindowW(L"EDIT",
											  utf8_to_wide(item.stringValue).c_str(),
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
											  contorlX, y, 200, 25, hParent, (HMENU) (3000 + i),
											  nullptr, nullptr);

						SetWindowSubclass(hCtrl, HotkeyEditSubclassProc, 3, (DWORD_PTR) &pItemData); // 注册子类以处理按键
					} else if (item.type == "button") {
						ButtonStyle btnStyle = BTN_NORMAL;
						if (item.key == "pref_backup_settings") {
							btnStyle = BTN_SUCCESS;
						} else if (item.key == "pref_restore_settings") {
							btnStyle = BTN_DANGER;
						}

						hCtrl = CreateEnhancedButton(hParent, (3000 + i), utf8_to_wide(item.title),
													 contorlX, y, 150, 30, (HMENU) (3000 + i), btnStyle);

//						hCtrl = CreateWindowW(L"BUTTON", utf8_to_wide(item.title).c_str(),
//											 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
//											 contorlX, y, 150, 30, hParent, (HMENU) (3000 + i), nullptr, nullptr);
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
			UpdateTabButtonSelection(hTabButtons, 0);

			// --------- 6. 保存按钮 ---------
			CreateEnhancedButton(hwnd, 1, L"保存", SETTINGS_WINDOW_WIDTH - 200, SETTINGS_WINDOW_HEIGHT - 80, 80, 35, (HMENU) 9999, BTN_PRIMARY);

			// --------- 7. 取消按钮 ---------
			CreateEnhancedButton(hwnd, 1, L"取消", SETTINGS_WINDOW_WIDTH - 110, SETTINGS_WINDOW_HEIGHT - 80, 80, 35, (HMENU) 9995, BTN_NORMAL);
			
//			CreateWindowW(...); // Reset button - removed
			// --------- 8. 应用按钮 ---------
//			CreateWindowW(...); // Apply button - removed
			break;
		}
		case WM_MOUSEWHEEL:
			SendMessage(tabContainers[currentSubPageIndex], WM_MOUSEWHEEL, wParam, lParam);
			break;
		case WM_CTLCOLORBTN:
		{
			HDC hdc = (HDC)wParam;
			HWND hButton = (HWND)lParam;

			// 设置文字背景透明
			SetBkMode(hdc, TRANSPARENT);
			// 设置背景色为白色
			SetBkColor(hdc, RGB(255, 255, 255));

			static HBRUSH hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));
			return (INT_PTR)hBrushWhite;
		}

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
				UpdateTabButtonSelection(hTabButtons, newIdx);
				break;
			}
				// --- 保存 ---
			else if (LOWORD(wParam) == 9999) {
				saveConfig(hwnd, subPages, g_settings2, hCtrlsByTab);
				break;
			}
				// --- 取消 ---
			else if (LOWORD(wParam) == 9995) {
				DestroyWindow(hwnd);
				return 0;
			}
				// --- 重置 ---
			else if (LOWORD(wParam) == 9998) {
				int result = MessageBoxW(hwnd, L"确定要重置所有设置为默认值吗？", L"确认重置",
										 MB_YESNO | MB_ICONQUESTION);
				if (result == IDYES) {
					resetToDefaults(hwnd, subPages, g_settings2, hCtrlsByTab);
				}
				break;
			}
				// --- 应用 ---
			else if (LOWORD(wParam) == 9997) {
				applySettings(hwnd, subPages, g_settings2, hCtrlsByTab);
				break;
			}
			// 处理设置功能按钮，在ScrollViewHelper ScrollContainerProc里面处理
			
		case WM_DESTROY:
			CleanupSettingWindowResources();
			// 清理静态变量
			subPages.clear();
			hTabButtons.clear();
			tabContainers.clear();
			hCtrlsByTab.clear();
			currentSubPageIndex = 0;
			g_settingsHwnd = nullptr;
			break;
		case WM_CLOSE: {
			DestroyWindow(hwnd);
			return 0;
		}
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
