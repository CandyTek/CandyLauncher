#pragma once

#include "../window/SettingsHelper.hpp"
#include "manager/SkinHelper.h"

// 只能给当前的滚动tab唯一使用
inline int g_savedScrollPos = 0;

static void UpdateScrollRange(HWND hwnd, int contentHeight) {
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	const int viewHeight = clientRect.bottom - clientRect.top;

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
	si.nMin = 0;

	// 如果内容高度小于或等于可视区域，则不需要滚动
	if (contentHeight <= viewHeight) {
		// 让滚动条消失或禁用
		si.nMax = 0;
		si.nPage = 1; // nPage不能为0
	} else {
		// 核心改动：nMax 直接等于内容高度
		// nPage 等于可视高度
		si.nMax = contentHeight;
		si.nPage = static_cast<UINT>(viewHeight);
	}

	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

	// 滚动条更新后，最好将滚动位置重置为0
	SetScrollPos(hwnd, SB_VERT, 0, TRUE);
}

// 這是專門為 ComboBox 的下拉列表視窗設計的子類化回呼函式
static LRESULT CALLBACK ComboBoxListSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
												UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	// 我們只關心滑鼠滾輪事件
	if (uMsg == WM_MOUSEWHEEL) {
		// dwRefData 儲存的是我們希望接收滾動消息的父容器句柄
		HWND hParentContainer = reinterpret_cast<HWND>(dwRefData);

		// 將滾輪消息原封不動地轉發給父容器
		SendMessage(hParentContainer, uMsg, wParam, lParam);

		// ShowErrorMsgBox(L"自己处理");
		// 返回 0，表示我們已經處理了此消息，
		// 從而阻止 ComboBox 的列表框滾動其自身內容。
		return 0;
	}

	// 對於所有其他消息，調用默認的子類化處理程序
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}


static LRESULT CALLBACK ScrollContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_VSCROLL:
		{
			// 1. 获取当前滚动信息
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			GetScrollInfo(hwnd, SB_VERT, &si);

			// 如果内容不需要滚动，直接返回
			if (si.nMax <= static_cast<int>(si.nPage)) {
				return 0;
			}

			int curPos = si.nPos;
			int newPos = curPos;

			// 2. 根据滚动条事件计算新位置
			switch (LOWORD(wParam)) {
			case SB_LINEUP: newPos -= 30; // 滚动步长
				break;
			case SB_LINEDOWN: newPos += 30;
				break;
			case SB_PAGEUP: newPos -= 100; // 翻页步长
				break;
			case SB_PAGEDOWN: newPos += 100;
				break;
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION: // 处理拖动结束
				newPos = HIWORD(wParam);
				break;
			default: break;
			}

			// 3. 限制新位置在滚动范围内
			newPos = MyMax(si.nMin, MyMin(newPos, static_cast<int>(si.nMax - si.nPage)));
			if (newPos == curPos) {
				return 0; // 位置未变，无需操作
			}

			// 4. 更新滚动条位置
			SetScrollPos(hwnd, SB_VERT, newPos, TRUE);

			// 5. 计算滚动的偏移量
			int delta = curPos - newPos;

			// 6. 移动所有子控件 (核心改动)
			// 首先，找到当前容器(hwnd)对应的 tab 索引
			auto it = std::find(tabContainers.begin(), tabContainers.end(), hwnd);
			if (it != tabContainers.end()) {
				int tabIdx = static_cast<int>(std::distance(tabContainers.begin(), it));
				const auto& ctrls_in_tab = hCtrlsByTab[tabIdx];

				for (HWND hCtrl : ctrls_in_tab) {
					if (hCtrl) // 确保句柄有效
					{
						RECT rect;
						GetWindowRect(hCtrl, &rect); // 获取屏幕坐标
						MapWindowPoints(HWND_DESKTOP, GetParent(hCtrl), reinterpret_cast<LPPOINT>(&rect), 2); // 转换到父窗口(hContainer)坐标
						// 使用 SetWindowPos 移动控件
						SetWindowPos(hCtrl, nullptr, rect.left, rect.top + delta, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
					}
				}
			}

			// 7. 让整个容器窗口无效，以便重绘背景
			// InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);

			return 0;
		}

	case WM_MOUSEWHEEL:
		{
			// 检查是否需要滚动
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			GetScrollInfo(hwnd, SB_VERT, &si);

			// 如果内容不需要滚动，直接返回
			if (si.nMax <= static_cast<int>(si.nPage)) {
				return 0;
			}

			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			int scrollAmount = (zDelta / WHEEL_DELTA) * 3; // 每次滚动3行
			for (int i = 0; i < abs(scrollAmount); ++i) {
				SendMessage(hwnd, WM_VSCROLL, zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
			}
			return 0;
		}

	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = reinterpret_cast<HDC>(wParam);
			HWND hCtrl = reinterpret_cast<HWND>(lParam);

			// 判断是不是复选框，如果是设置为白底
			wchar_t className[32];
			GetClassNameW(hCtrl, className, 32);
			if (wcscmp(className, L"Button") == 0) {
				// 设置白底
				SetBkMode(hdcStatic, TRANSPARENT);
				SetBkColor(hdcStatic, RGB(255, 255, 255));
				static HBRUSH hBrushWhite = CreateSolidBrush(RGB(255, 255, 255));
				return reinterpret_cast<INT_PTR>(hBrushWhite);
			}
			SetBkMode(hdcStatic, TRANSPARENT);
			return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
		}
	case WM_COMMAND:
		{
			// --- 2. 處理來自複選框的特定點擊事件 ---
			int ctrlId = LOWORD(wParam);
			if (ctrlId >= 3000 && ctrlId < static_cast<int>(3000 + g_settings_ui.size())) {
				SettingItem* setting = getSettingItemForCtrlSubId(ctrlId - 3000, g_settings_ui);
				if (setting == nullptr) {
					break;
				}

				if (HIWORD(wParam) == BN_CLICKED) {
					// 從ID計算出在 settings2 中的索引
					if (setting->type == "bool") {
						HWND hCheckbox = reinterpret_cast<HWND>(lParam); // 獲取複選框的句柄
						bool checkState = GetSwitchState(hCheckbox);
						if (setting->key == "pref_auto_start") {
							if (checkState) {
								MessageBoxW(nullptr, L"您啟用了「特殊功能」！", L"提示", MB_OK);
							} else {
								MessageBoxW(nullptr, L"您關閉了「特殊功能」。", L"提示", MB_OK);
							}
						} else if (setting->key == "pref_show_tray_icon") {
							if (checkState == BST_CHECKED) {
								OutputDebugStringW(L"顯示高級選項\n");
							} else {
								OutputDebugStringW(L"隱藏高級選項\n");
							}
						}
					} else if (setting->type == "button") {
						handleButtonAction(hwnd, setting->key);
					}
				} else if (HIWORD(wParam) == CBN_SELCHANGE) {
					if (setting->type == "list") {
						// 皮肤控件
						if (setting->key == "pref_skin") {
							HWND g_hSkinComboBox = reinterpret_cast<HWND>(lParam); // 獲取複選框的句柄
							int selIdx = static_cast<int>(SendMessage(g_hSkinComboBox, CB_GETCURSEL, 0, 0));
							g_currectSkinFilePath = utf8_to_wide(g_settings_ui[g_prefSkinIndex].entryValues[selIdx]);
							//ShowErrorMsgBox(g_currectSkinFilePath);
							ShowMainWindowSimple();
							SendMessage(g_mainHwnd, WM_REFRESH_SKIN, 0, 0);
						}
					}
				}

				if (setting->type == "expand") {
					// --- 展开/折叠 ---

					SCROLLINFO si;
					si.cbSize = sizeof(si);
					si.fMask = SIF_POS;
					// 记录当前滚动到的位置
					if (GetScrollInfo(hwnd, SB_VERT, &si)) {
						g_savedScrollPos = si.nPos;
					}

					setting->isExpanded = !setting->isExpanded;
					nlohmann::json _unused;
					saveSettingControlValues(_unused, true);
					SendMessage(g_settingsHwnd, WM_REFRESH_SETTING_UI, 0, reinterpret_cast<LPARAM>(hwnd));
					return 0;
				} else if (setting->type == "expandswitch") {
					// expandswitch类型可能是点击开关或点击展开
					// BN_CLICKED: 点击开关
					// BN_DOUBLECLICKED: 点击展开/折叠区域
					if (HIWORD(wParam) == BN_CLICKED) {
						// 点击的是开关，只更新开关状态，不刷新UI
						HWND hControl = reinterpret_cast<HWND>(lParam);
						bool newState = GetExpandSwitchState(hControl);
						setting->boolValue = newState;

						// 如果需要实时保存
						nlohmann::json _unused;
						saveSettingControlValues(_unused, true);
					} else if (HIWORD(wParam) == BN_DOUBLECLICKED) {
						// 处理展开/折叠事件(expandswitch专用)
						// --- 展开/折叠 ---
						SCROLLINFO si;
						si.cbSize = sizeof(si);
						si.fMask = SIF_POS;
						// 记录当前滚动到的位置
						if (GetScrollInfo(hwnd, SB_VERT, &si)) {
							g_savedScrollPos = si.nPos;
						}

						setting->isExpanded = !setting->isExpanded;
						nlohmann::json _unused;
						saveSettingControlValues(_unused, true);
						SendMessage(g_settingsHwnd, WM_REFRESH_SETTING_UI, 0, reinterpret_cast<LPARAM>(hwnd));
						return 0;
					}
				}
			}

			break;
		}
	case WM_CTLCOLOREDIT:
		{
			// 尝试解决 readyonly的edit 黑背景问题，没用
			HDC hdcChild = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdcChild, RGB(0, 0, 0));
			// 设置背景颜色为透明，这样父窗口的背景就能透过来
			SetBkMode(hdcChild, TRANSPARENT);
			return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
			// return (LRESULT)GetStockObject(WHITE_BRUSH);
		}

	default: break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}


static ATOM MyScrollViewRegisterClass() {
	WNDCLASSEXW wcex{};
	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); // 用系统白背景更稳
	//	 wcex.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); // 例如纯白色
	//	wcex.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));

	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.cbWndExtra = 0;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = ScrollContainerProc;
	wcex.hInstance = GetModuleHandle(nullptr);
	wcex.lpszMenuName = nullptr;
	// wcex.style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN;
	wcex.lpszClassName = L"SCROLLVIEW";
	return RegisterClassExW(&wcex);
}
