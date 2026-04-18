// 索引 索引
// 保存按钮
// Tab容器
// 创建Tab按钮

#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <set>
#include <filesystem>

#include "../common/I18n.hpp"
#include "../common/I18nResource.h"
#include "../common/Resource.h"
#include "../util/BaseTools.hpp"
#include "../util/MainTools.hpp"
#include "../view/ScrollViewController.hpp"
#include "SettingsManager.hpp"
#include "../common/GlobalState.hpp"
#include "../view/CustomButtonHelper.hpp"
#include "../view/HotkeyEditView.hpp"
#include "../view/CustomBgColorView.hpp"
#include "../manager/SkinHelper.hpp"
#include "view/CustomComboBox.hpp"
#include "view/CustomEdit.hpp"

static int tabBtnWidth = 120;
static HWND blankBelowTabBelow = nullptr;
inline HFONT pref_edit_hfont = nullptr;
inline bool isSettingsWindowClosingWithoutSave = false;

static std::map<std::string, UINT> tabNameMap = {
	{"plugin", IDS_I18N_TAB_PLUGIN},
	{"normal", IDS_I18N_TAB_NORMAL},
	{"feature", IDS_I18N_TAB_FEATURE},
	{"skin", IDS_I18N_TAB_SKIN},
	{"other", IDS_I18N_TAB_OTHER},
	{"hotkey", IDS_I18N_TAB_HOTKEY},
	{"about", IDS_I18N_TAB_ABOUT}
};
static std::map<std::wstring, UINT> tabIconMap = {
	{L"plugin", IDI_SETTINGS_TAB_PLUGIN},
	{L"normal", IDI_SETTINGS_TAB_NORMAL},
	{L"feature", IDI_SETTINGS_TAB_FEATURE},
	{L"skin", IDI_SETTINGS_TAB_SKIN},
	{L"other", IDI_SETTINGS_TAB_OTHER},
	{L"hotkey", IDI_SETTINGS_TAB_HOTKEY},
	{L"about", IDI_SETTINGS_TAB_ABOUT}
};

static void InitializeSettingWindowResources() {
	if (!pref_edit_hfont) {
		pref_edit_hfont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");
	}
	InitializeCustomComboBoxResources();
}


static void ShowSettingsWindow(HINSTANCE hInstance, HWND hParent, bool isShow = true) {
	if (g_settingsHwnd != nullptr) {
		RestoreWindowIfMinimized(g_settingsHwnd);
		SetForegroundWindow(g_settingsHwnd);
		// ShowWindow(g_settingsHwnd, SW_SHOWNOACTIVATE);
		ShowWindow(g_settingsHwnd, SW_SHOW);
		return;
	}
	LoadSettingList();

	// 清理静态变量，防止重复创建控件
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

	unsigned long dw_style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	if (isShow) dw_style |= WS_VISIBLE;

	g_settingsHwnd = CreateWindowExW(
		WS_EX_ACCEPTFILES, // 扩展样式
		L"SettingsWndClass", // 窗口类名
		LoadI18nString(IDS_I18N_SETTINGS_WINDOW_TITLE, L"Settings").c_str(), // 窗口标题
		dw_style, // 窗口样式
		x, y, // 初始位置
		SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT, // 宽度和高度
		hParent, // 父窗口
		nullptr, // 菜单
		hInstance, // 实例句柄
		nullptr // 附加参数
	);

	// ShowWindow(g_settingsHwnd, SW_SHOW);
	UpdateWindow(g_settingsHwnd);
}


static void CleanupSettingWindowResources() {
	// 清理字体
	if (pref_edit_hfont) {
		DeleteObject(pref_edit_hfont);
		pref_edit_hfont = nullptr;
	}
}

static void setCustomEdit(HWND hEdit, bool isNumber=false) {
	auto* data = new EditBorderData;
	if (isNumber) {
		data->oldProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)NumberEditBorder2pxProc);
	} else {
		data->oldProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditBorder2pxProc);
	}
	SetWindowLongPtrW(hEdit, GWLP_USERDATA, (LONG_PTR)data);
	
	SendMessageW(hEdit, WM_SETFONT, (WPARAM)pref_edit_hfont, TRUE);
	// 文本区域左右内缩
	SendMessageW(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(3, 3));
}

static void RestoreSavedToggleMainPanelHotkey() {
	if (!pref_hotkey_toggle_main_panel.empty()) {
		RegisterHotkeyFromString(g_mainHwnd, pref_hotkey_toggle_main_panel, HOTKEY_ID_TOGGLE_MAIN_PANEL);
	}
}

/** 
 * 创建设置控件的通用函数
 * 控件ID分配逻辑，从3000开始，每条SettingItem都会消耗一个ID
 */
static void CreateSettingControlsForTab(size_t tabIdx, HWND hwnd) {
	int y = 10;
	int contorlX = 254;
	std::vector<HWND> ctrls;
	HWND hParent = tabContainers[tabIdx];
	size_t currentCtrlSubId = 2999;

	// 创建递归函数处理设置项和子项，注意continue的逻辑
	std::function<void(const std::vector<SettingItem>&, int, int&, bool)> createSettingControls =
		[&](const std::vector<SettingItem>& settings, int indentLevel, int& currentY, bool isExpand) {
		for (const auto& item : settings) {
			currentCtrlSubId++;
			// ConsolePrintln(item.key);
			if (item.subPageIndex != tabIdx) {
				if (item.type == "expand" || item.type == "expandswitch") {
					createSettingControls(item.children, indentLevel + 1, currentY, isExpand && item.isExpanded);
				}
				continue;
			}

			std::wstring text = utf8_to_wide(item.title);
			int indentX = 10 + indentLevel * 20; // 缩进

			HWND hLabel;
			int labelHeight = 0;
			if (item.type == "text") {
				// 静态文本设置项
				int labelWidth = 420 - indentLevel * 20;
				labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
				hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
										WS_CHILD | WS_VISIBLE,
										indentX, currentY, labelWidth, labelHeight, hParent, (HMENU)currentCtrlSubId, nullptr,
										nullptr);
				SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFontDefault, TRUE);
				ctrls.push_back(hLabel);
				if (isExpand) currentY += labelHeight;
				if (!isExpand) {
					SetWindowPos(hLabel, nullptr, 0, -80, 0, 0, SWP_NOZORDER);
				}
				continue;
			} else if (item.type == "expand") {
				// 折叠按钮设置项
				// 为expand类型创建可点击的标题
				int labelWidth = 444 - indentLevel * 20;
				labelHeight = MyMax(35, GetLabelHeight(hwnd, text, labelWidth, hFontDefault));
				hLabel = CreateEnhancedButton(hParent, currentCtrlSubId, text,
											indentX, currentY, labelWidth, labelHeight, (HMENU)currentCtrlSubId,
											BTN_EXPAND);
				// 设置折叠状态
				SetExpandButtonState(hLabel, item.isExpanded);
				ctrls.push_back(hLabel);
				if (isExpand) currentY += labelHeight + 6;
				// 递归创建子控件
				createSettingControls(item.children, indentLevel + 1, currentY, isExpand && item.isExpanded);
				if (!isExpand) {
					SetWindowPos(hLabel, nullptr, 0, -80, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				}
				continue;
			} else if (item.type == "expandswitch") {
				// 折叠+开关组合按钮设置项
				int labelWidth = 444 - indentLevel * 20;
				labelHeight = MyMax(35, GetLabelHeight(hwnd, text, labelWidth, hFontDefault));
				hLabel = CreateEnhancedButton(hParent, currentCtrlSubId, text,
											indentX, currentY, labelWidth, labelHeight, (HMENU)currentCtrlSubId,
											BTN_EXPAND_SWITCH);
				// 设置折叠状态和开关状态
				SetExpandButtonState(hLabel, item.isExpanded);
				SetExpandSwitchState(hLabel, item.boolValue);
				ctrls.push_back(hLabel);
				if (isExpand) currentY += labelHeight + 6;
				// 递归创建子控件
				createSettingControls(item.children, indentLevel + 1, currentY, isExpand && item.isExpanded);
				if (!isExpand) {
					SetWindowPos(hLabel, nullptr, 0, -80, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				}
				continue;
			}

			// 设置项的标题 label
			int labelWidth = 220 - indentLevel * 20;
			labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
			hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
									WS_CHILD | WS_VISIBLE,
									indentX, currentY, labelWidth, labelHeight, hParent, nullptr, nullptr,
									nullptr);
			SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFontDefault, TRUE);

			if (!isExpand) {
				SetWindowPos(hLabel, nullptr, 0, -80, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			}
			ctrls.push_back(hLabel);


			HWND hCtrl = nullptr;
			if (item.type == "bool") {
				hCtrl = CreateSwitchControl(hParent, currentCtrlSubId, contorlX, currentY, 50, 25,
											(HMENU)currentCtrlSubId, item.boolValue);
			} else if (item.type == "string") {
				hCtrl = CreateWindowW(L"EDIT",
									utf8_to_wide(item.stringValue).c_str(),
									WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT |WS_BORDER,
									contorlX, currentY, 200, 25, hParent, (HMENU)currentCtrlSubId, nullptr,
									nullptr);
				setCustomEdit(hCtrl);
			} else if (item.type == "stringArr") {
				std::wstring defVal;
				for (auto& s : item.stringArr) defVal += utf8_to_wide(s) + L"\r\n";
				hCtrl = CreateWindowW(L"EDIT",
									defVal.c_str(),
									WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE | WS_VSCROLL| ES_AUTOHSCROLL|ES_AUTOVSCROLL|WS_BORDER,
									contorlX, currentY, 200, 50, hParent, (HMENU)currentCtrlSubId, nullptr,
									nullptr);
				setCustomEdit(hCtrl);
				if (isExpand) currentY += 30;
			} else if (item.type == "list") {
				hCtrl = CreateEnhancedComboBox(hParent, currentCtrlSubId, contorlX, currentY, 200, 300, (HMENU)currentCtrlSubId);
				// 1. 獲取 ComboBox 的詳細資訊，主要是為了得到下拉列表的句柄 (hwndList)
				COMBOBOXINFO cbi = {sizeof(COMBOBOXINFO)};
				GetComboBoxInfo(hCtrl, &cbi);
				if (cbi.hwndCombo) // 確保列表句柄有效
				{
					SetWindowSubclass(cbi.hwndCombo, ComboBoxListSubclassProc, 1, (DWORD_PTR)hParent);
				}
				const auto& entries = item.entries;
				const auto& entryValues = item.entryValues;
				int selIndex = 0;

				for (size_t j = 0; j < entries.size(); ++j) {
					std::wstring entryW = utf8_to_wide(entries[j]);
					SendMessageW(hCtrl, CB_ADDSTRING, 0, (LPARAM)entryW.c_str());

					if (entryValues[j] == item.stringValue) selIndex = static_cast<int>(j);
				}
				SendMessageW(hCtrl, CB_SETCURSEL, selIndex, 0);
			} else if (item.type == "long") {
				hCtrl = CreateWindowW(L"EDIT",
									std::to_wstring(item.intValue).c_str(),
									WS_CHILD | WS_VISIBLE | ES_LEFT | ES_NUMBER | ES_AUTOHSCROLL|WS_BORDER,
									contorlX, currentY, 200, 25, hParent, (HMENU)currentCtrlSubId,
									nullptr, nullptr);
				setCustomEdit(hCtrl);
			} else if (item.type == "hotkeystring") {
				hCtrl = CreateWindowW(L"EDIT",
									utf8_to_wide(item.stringValue).c_str(),
									WS_CHILD | WS_VISIBLE | ES_LEFT| ES_AUTOHSCROLL|WS_BORDER,
									contorlX, currentY, 200, 25, hParent, (HMENU)currentCtrlSubId,
									nullptr, nullptr);
				SendMessageW(hCtrl, WM_SETFONT, (WPARAM)pref_edit_hfont, TRUE);
				InstallHotkeySubclass(hCtrl, hwnd, utf8_to_wide(item.key));
				SendMessageW(hCtrl, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(3, 3));
			} else if (item.type == "button") {
				ButtonStyle btnStyle = BTN_NORMAL;
				if (item.key == "pref_backup_settings") {
					btnStyle = BTN_SUCCESS;
				} else if (item.key == "pref_restore_settings") {
					btnStyle = BTN_DANGER;
				}
				hCtrl = CreateEnhancedButton(hParent, currentCtrlSubId, utf8_to_wide(item.title),
											contorlX, currentY, 150, 30, (HMENU)currentCtrlSubId, btnStyle);
			}

			if (hCtrl) {
				ctrls.push_back(hCtrl);
				if (!isExpand) {
					SetWindowPos(hCtrl, nullptr, 0, -80, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
				}
			}
			if (isExpand) currentY += MyMax(35, labelHeight);
		}
	};
	createSettingControls(g_settings_ui, 0, y, true);

	int contentHeight = y + 20; // y 是最后一个控件的底部+边距
	UpdateScrollRange(hParent, contentHeight); // 调用正确的辅助函数

	hCtrlsByTab[tabIdx] = ctrls;
}

// 重新创建设置界面的函数
static void RefreshSettingsUI(HWND hwnd, HWND tabHwnd) {
	// 清理现有控件
	for (size_t tabIdx = 0; tabIdx < tabContainers.size(); ++tabIdx) {
		if (tabContainers[tabIdx] && tabHwnd == tabContainers[tabIdx]) {
			// 销毁容器中的所有子窗口
			HWND hChild = GetWindow(tabContainers[tabIdx], GW_CHILD);
			while (hChild) {
				HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
				DestroyWindow(hChild);
				hChild = hNext;
			}
			// 重新创建控件
			CreateSettingControlsForTab(tabIdx, hwnd);
			return;
		}
	}
}


static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		{
			isSettingsWindowClosingWithoutSave = false;
			SendMessage(g_settingsHwnd, WM_SETREDRAW, FALSE, 0);
			RefreshSkinFile();
			AddAppStartupTime();
			originalSkinPath = g_currectSkinFilePath;
			MyScrollViewRegisterClass();
			g_settings_ui = g_settings_ui_last_save;
			// 收集所有subPage名已在ParseConfig时完成，无需再次收集
			// subPageTabs已经在ParseConfig中填充好了

			// 创建Tab按钮（左侧）
			int tabY = 0;
			for (size_t i = 0; i < subPageTabs.size(); ++i) {
				std::wstring key = utf8_to_wide(subPageTabs[i]);
				std::wstring label;
				if (auto it = tabNameMap.find(subPageTabs[i]); it != tabNameMap.end()) {
					label = LoadI18nString(it->second, label);
				}
				// 用 key 查图标资源ID
				const UINT iconId = tabIconMap[key];
				const HICON hIcon = (HICON)LoadImage(g_hInst,MAKEINTRESOURCE(iconId),IMAGE_ICON, 16, 16,LR_DEFAULTCOLOR);
				HWND hTabBtn = CreateEnhancedButton(hwnd, (2000 + i), label, 0, tabY, tabBtnWidth, 48,
													(HMENU)(2000 + i), BTN_NORMAL, hIcon, TEXT_LEFT);
				hTabButtons.push_back(hTabBtn);
				tabY += 48;
			}
			blankBelowTabBelow = CreateWindowW(L"STATIC", L"",
												WS_CHILD | WS_VISIBLE,
												0, tabY, tabBtnWidth, SETTINGS_WINDOW_HEIGHT, hwnd, nullptr, nullptr,
												nullptr);
			// 给控件设置浅灰背景
			SetWindowSubclass(blankBelowTabBelow, CustomBgcolorControlClassProc, 1, (DWORD_PTR)RGB(230, 230, 230));

			// 创建每个Tab容器
			tabContainers.resize(subPageTabs.size());
			for (size_t tabIdx = 0; tabIdx < subPageTabs.size(); ++tabIdx) {
				HWND hContainer = CreateWindowExW(
					WS_EX_ACCEPTFILES | WS_EX_COMPOSITED,
					L"SCROLLVIEW", nullptr,
					WS_CHILD | WS_VISIBLE | WS_VSCROLL,
					tabBtnWidth + 4, 0, (SETTINGS_WINDOW_WIDTH - tabBtnWidth - 16 - 4), SETTINGS_WINDOW_HEIGHT-88, hwnd, nullptr,
					GetModuleHandle(nullptr), nullptr);
				tabContainers[tabIdx] = hContainer;
			}

			// 在每个容器中生成控件
			hCtrlsByTab.clear();
			hCtrlsByTab.resize(subPageTabs.size());
			for (size_t tabIdx = 0; tabIdx < subPageTabs.size(); ++tabIdx) {
				CreateSettingControlsForTab(tabIdx, hwnd);
			}

			// 只显示当前tab内容
			for (size_t tabIdx = 0; tabIdx < tabContainers.size(); ++tabIdx)
				ShowWindow(
					tabContainers[tabIdx], tabIdx == 0 ? SW_SHOW : SW_HIDE);

			// 设置第一个tab为选中状态
			UpdateTabButtonSelection(hTabButtons, 0);

			// 保存按钮
			CreateEnhancedButton(hwnd, 1, LoadI18nString(IDS_I18N_BTN_SAVE, L"Save"),
								SETTINGS_WINDOW_WIDTH - 200, SETTINGS_WINDOW_HEIGHT - 80, 80, 35,
								(HMENU)9999, BTN_PRIMARY);

			// 取消按钮
			CreateEnhancedButton(hwnd, 1, LoadI18nString(IDS_I18N_BTN_CANCEL, L"Cancel"),
								SETTINGS_WINDOW_WIDTH - 110, SETTINGS_WINDOW_HEIGHT - 80, 80, 35,
								(HMENU)9995, BTN_NORMAL);

			//			CreateWindowW(...); // Reset button - removed
			// 应用按钮
			//			CreateWindowW(...); // Apply button - removed
			SendMessage(g_settingsHwnd, WM_SETREDRAW, TRUE, 0);
			InvalidateRect(g_settingsHwnd, nullptr, TRUE);
			break;
		}
	case WM_MOUSEWHEEL:
		SendMessage(tabContainers[currentSubPageIndex], WM_MOUSEWHEEL, wParam, lParam);
		break;
	case WM_CTLCOLORBTN:
		{
			HDC hdc = (HDC)wParam;

			// 设置文字背景透明
			SetBkMode(hdc, TRANSPARENT);
			// 设置背景色为白色
			SetBkColor(hdc, RGB(255, 255, 255));

			static HBRUSH hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));
			return (INT_PTR)hBrushWhite;
		}

	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			SetBkMode(hdcStatic, TRANSPARENT); // 设置背景透明
			return (INT_PTR)GetStockObject(NULL_BRUSH); // 返回一个透明画刷
		}

	case WM_COMMAND:
		// --- 切换Tab ---
		if (LOWORD(wParam) >= 2000 && LOWORD(wParam) < 2000 + hTabButtons.size()) {
			int newIdx = LOWORD(wParam) - 2000;
			if (newIdx == currentSubPageIndex) break;

			ShowWindow(tabContainers[currentSubPageIndex], SW_HIDE);
			ShowWindow(tabContainers[newIdx], SW_SHOW);
			currentSubPageIndex = newIdx;

			// 更新tab按钮的选中状态
			UpdateTabButtonSelection(hTabButtons, newIdx);
		}
		// --- 保存 ---
		else if (LOWORD(wParam) == 9999) {
			// 先同步当前显示的控件值到temp
			nlohmann::json newConfig;
			size_t ctrlIndex = 2999;
			saveSettingControlValues(newConfig);
			// 使用temp数据保存到文件
			saveConfigToFile(USER_SETTINGS_PATH, newConfig);
			// 将temp数据复制回原始数据
			g_settings_ui_last_save = g_settings_ui;
			// 重新加载设置和当前语言
			LoadSettingList();
			DestroyWindow(hwnd);
			SendMessage(g_mainHwnd, WM_CONFIG_SAVED, 1, 0);
			SendMessage(g_mainHwnd, WM_REFRESH_SKIN, 0, 0);
		}
		// --- 取消 ---
		else if (LOWORD(wParam) == 9995) {
			isSettingsWindowClosingWithoutSave = true;
			// 取消时不保存temp数据，直接恢复原始皮肤路径并销毁窗口
			g_currectSkinFilePath = originalSkinPath;
			SendMessage(g_mainHwnd, WM_REFRESH_SKIN, 0, 0);
			RestoreSavedToggleMainPanelHotkey();

			// 清理temp数据（可选，因为下次打开设置窗口时会重新初始化）
			g_settings_ui.clear();

			DestroyWindow(hwnd);
			return 0;
		}
		// --- 重置 ---
		else if (LOWORD(wParam) == 9998) {
			int result = MessageBoxW(
				hwnd,
				LoadI18nString(IDS_I18N_RESET_CONFIRM_TEXT, L"Reset all settings to their default values?").c_str(),
				LoadI18nString(IDS_I18N_RESET_CONFIRM_TITLE, L"Confirm Reset").c_str(),
				MB_YESNO | MB_ICONQUESTION);
			if (result == IDYES) {
				resetToDefaults(hwnd, subPageTabs, g_settings_ui_last_save, hCtrlsByTab);
			}
		}
		// --- 应用 ---
		else if (LOWORD(wParam) == 9997) {
			applySettings(hwnd, subPageTabs, g_settings_ui_last_save, hCtrlsByTab);
		}
		break;
	// 处理设置功能按钮，在ScrollViewHelper ScrollContainerProc里面处理

	case WM_DESTROY: CleanupSettingWindowResources();
		// 清理静态变量
		hTabButtons.clear();
		tabContainers.clear();
		hCtrlsByTab.clear();
		currentSubPageIndex = 0;
		g_settingsHwnd = nullptr;
		blankBelowTabBelow = nullptr;
		isSettingsWindowClosingWithoutSave = false;
		// 清理临时设置数据
		g_settings_ui.clear();
		break;
	case WM_CLOSE:
		{
			isSettingsWindowClosingWithoutSave = true;
			g_currectSkinFilePath = originalSkinPath;
			SendMessage(g_mainHwnd, WM_REFRESH_SKIN, 0, 0);
			RestoreSavedToggleMainPanelHotkey();
			// 清理临时设置数据
			g_settings_ui.clear();
			DestroyWindow(hwnd);
			return 0;
		}
	case WM_REFRESH_SETTING_UI:
		{
			SendMessage(tabContainers[currentSubPageIndex], WM_SETREDRAW, FALSE, 0);
			RefreshSettingsUI(g_settingsHwnd, reinterpret_cast<HWND>(lParam));

			// 仅在UI重建时恢复保存的滚动位置
			RECT clientRect;
			GetClientRect(tabContainers[currentSubPageIndex], &clientRect);
			const int viewHeight = clientRect.bottom - clientRect.top;

			if (g_savedScrollPos > 0) {
				// 计算有效的滚动位置范围
				const int maxPos = viewHeight;
				int restorePos = MyMin(g_savedScrollPos, maxPos);
				restorePos = MyMax(0, restorePos);
				// 同步控件位置到恢复的滚动位置
				SendMessage(tabContainers[currentSubPageIndex], WM_VSCROLL, MAKELONG(SB_THUMBPOSITION, restorePos), 0);
			}
			SendMessage(tabContainers[currentSubPageIndex], WM_SETREDRAW, TRUE, 0);
			InvalidateRect(tabContainers[currentSubPageIndex], nullptr, TRUE);

			return 0;
		}
	case WM_APP_HOTKEY_COMMIT:
		{
			const auto* key = reinterpret_cast<const wchar_t*>(wParam);
			const auto* value = reinterpret_cast<const wchar_t*>(lParam);

			if (isSettingsWindowClosingWithoutSave) {
				RestoreSavedToggleMainPanelHotkey();
				return 0;
			}

			if (key && value && std::wstring(key) == L"pref_hotkey_toggle_main_panel") {
				RegisterHotkeyFromString(g_mainHwnd, wide_to_utf8(value), HOTKEY_ID_TOGGLE_MAIN_PANEL);
			}

			return 0;
		}

	default: return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// 注册设置窗口类。
inline ATOM SettingWindowRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEXW wc{};

	wc.cbSize = sizeof(WNDCLASSEX);
	//wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = SettingsWndProc;
	wc.hInstance = hInstance;
	//wc.cbClsExtra = 0;
	//wc.cbWndExtra = 0;
	wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CANDYLAUNCHER));
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CANDYLAUNCHER));
	wc.lpszClassName = L"SettingsWndClass";
	return RegisterClassExW(&wc);
}
