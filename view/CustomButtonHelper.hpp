#pragma once

#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <commctrl.h>

// Button styles enumeration
enum ButtonStyle {
	BTN_NORMAL = 0,
	BTN_PRIMARY = 1,
	BTN_SUCCESS = 2,
	BTN_DANGER = 3,
	BTN_EXPAND = 4, // 折叠按钮样式
	BTN_EXPAND_SWITCH = 5 // 折叠按钮+开关组合样式
};

// Text alignment enumeration
enum TextAlignment {
	TEXT_CENTER = 0,
	TEXT_LEFT = 1,
	TEXT_RIGHT = 2
};

// Button information structure
struct ButtonInfo {
	HWND hwnd{};
	ButtonStyle style;
	bool isHovered{};
	bool isPressed{};
	bool isSelected{};
	bool isExpanded{}; // 折叠状态
	bool switchState{}; // 开关状态(用于BTN_EXPAND_SWITCH)
	bool switchHovered{}; // 开关悬停状态
	std::wstring text;
	HICON hIcon{}; // Icon handle for left-side icon display
	TextAlignment textAlign; // Text alignment
};

// Global button list

// Font handles
inline HFONT hFontDefault = nullptr;
inline HFONT hFontButton = nullptr;

// Button brushes
inline HBRUSH hBrushPrimaryHover = nullptr;
inline HBRUSH hBrushSuccessHover = nullptr;
inline HBRUSH hBrushDangerHover = nullptr;

inline HBRUSH hBrushButtonNormal = nullptr;
inline HBRUSH hBrushButtonHover = nullptr;
inline HBRUSH hBrushButtonPressed = nullptr;
inline HBRUSH hBrushButtonSelected = nullptr;
inline HBRUSH hBrushButtonPrimary = nullptr;
inline HBRUSH hBrushButtonSuccess = nullptr;
inline HBRUSH hBrushButtonDanger = nullptr;
inline HBRUSH hBrushButtonExpand = nullptr;
inline HBRUSH hBrushButtonExpandHover = nullptr;

// Initialize button resources
static void InitializeCustomButtonResources() {
	// Initialize fonts
	if (!hFontDefault) {
		hFontDefault = CreateFontW(
			20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");
	}

	if (!hFontButton) {
		hFontButton = CreateFontW(
			20, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");
	}

	// Initialize brushes
	if (!hBrushButtonNormal) hBrushButtonNormal = CreateSolidBrush(RGB(230, 230, 230));
	if (!hBrushButtonHover) hBrushButtonHover = CreateSolidBrush(RGB(207, 207, 207));
	if (!hBrushButtonPressed) hBrushButtonPressed = CreateSolidBrush(RGB(184, 184, 184));
	if (!hBrushButtonSelected) hBrushButtonSelected = CreateSolidBrush(RGB(199,220,251));
	if (!hBrushButtonPrimary) hBrushButtonPrimary = CreateSolidBrush(RGB(0, 120, 215));
	if (!hBrushButtonSuccess) hBrushButtonSuccess = CreateSolidBrush(RGB(40, 167, 69));
	if (!hBrushButtonDanger) hBrushButtonDanger = CreateSolidBrush(RGB(220, 53, 69));
	if (!hBrushPrimaryHover) hBrushPrimaryHover = CreateSolidBrush(RGB(56, 165, 250));
	if (!hBrushSuccessHover) hBrushSuccessHover = CreateSolidBrush(RGB(20, 147, 49));
	if (!hBrushDangerHover) hBrushDangerHover = CreateSolidBrush(RGB(200, 33, 49));
	if (!hBrushButtonExpand) hBrushButtonExpand = CreateSolidBrush(RGB(238, 238, 238));
	if (!hBrushButtonExpandHover) hBrushButtonExpandHover = CreateSolidBrush(RGB(228, 228, 228));
}

// Get appropriate brush for button state
static HBRUSH GetButtonBrush(ButtonInfo* btnInfo) {
	if (!btnInfo) return hBrushButtonNormal;

	if (btnInfo->isPressed) {
		return hBrushButtonPressed;
	}

	// If it's a selected normal button (tab button), use selected background
	if (btnInfo->style == BTN_NORMAL && btnInfo->isSelected) {
		return hBrushButtonSelected;
	}
	// 按钮样式
	switch (btnInfo->style) {
	case BTN_PRIMARY: return btnInfo->isHovered ? hBrushPrimaryHover : hBrushButtonPrimary;
	case BTN_SUCCESS: return btnInfo->isHovered ? hBrushSuccessHover : hBrushButtonSuccess;
	case BTN_DANGER: return btnInfo->isHovered ? hBrushDangerHover : hBrushButtonDanger;
	case BTN_EXPAND: return btnInfo->isHovered ? hBrushButtonExpandHover : hBrushButtonExpand;
	case BTN_EXPAND_SWITCH: return btnInfo->isHovered ? hBrushButtonExpandHover : hBrushButtonExpand;
	default: return btnInfo->isHovered ? hBrushButtonHover : hBrushButtonNormal;
	}
}

// 绘制折叠箭头
static void DrawExpandArrow(HDC hdc, RECT rect, bool isExpanded, bool isHovered) {
	// 计算箭头位置
	int centerX = rect.left + 12; // 左侧留12px边距
	int centerY = rect.top + (rect.bottom - rect.top) / 2;

	// 设置画笔和画刷
	COLORREF arrowColor = isHovered ? RGB(0, 120, 215) : RGB(100, 100, 100);
	HPEN hPen = CreatePen(PS_SOLID, 2, arrowColor);
	HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

	// 绘制箭头
	if (isExpanded) {
		// 向下箭头 ▼
		MoveToEx(hdc, centerX - 4, centerY - 2, NULL);
		LineTo(hdc, centerX, centerY + 2);
		LineTo(hdc, centerX + 4, centerY - 2);
	} else {
		// 向右箭头 ▶
		MoveToEx(hdc, centerX - 2, centerY - 4, NULL);
		LineTo(hdc, centerX + 2, centerY);
		LineTo(hdc, centerX - 2, centerY + 4);
	}

	SelectObject(hdc, hOldPen);
	DeleteObject(hPen);
}

// 绘制简化开关(用于组合控件) - 复用Win8风格
static void DrawSwitchWin8UI(HDC hdc, int x, int y, bool isOn, bool isHovered) {
	// Win8.1 风格尺寸（扁平、矩形）
	constexpr int switchWidth = 48;
	constexpr int switchHeight = 20;
	constexpr int thumbW = 12;
	constexpr int thumbH = 22;

	// 创建双缓冲
	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, switchWidth, switchHeight);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

	// 背景透明
	HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 245));
	RECT bgRect = {0, 0, switchWidth, switchHeight};
	FillRect(memDC, &bgRect, bgBrush);
	DeleteObject(bgBrush);

	// Win8.1 扁平色系
	const COLORREF trackColorOff = isHovered ? RGB(203, 203, 203) : RGB(173, 173, 173);
	const COLORREF trackColorOn = isHovered ? RGB(56, 165, 250) : RGB(0, 120, 215);
	const COLORREF thumbColor = RGB(0, 0, 0); // 黑色滑块
	const COLORREF borderColorOff = RGB(173, 173, 173);
	const COLORREF borderColorOn = borderColorOff;

	const COLORREF currentTrackColor = isOn ? trackColorOn : trackColorOff;
	const COLORREF currentBorderColor = isOn ? borderColorOn : borderColorOff;

	// 计算几何矩形
	RECT outerRect = {0, 0, switchWidth, switchHeight};
	RECT trackRect = outerRect;
	InflateRect(&trackRect, -1, -1);
	RECT realTrackRect = outerRect;
	InflateRect(&realTrackRect, -3, -3);

	// thumb 位置（在 track 内左右平移）
	const int travel = (trackRect.right - trackRect.left) - thumbW;
	int thumbX = trackRect.left + (isOn ? travel : 0);
	int thumbY = trackRect.top + ((trackRect.bottom - trackRect.top) - thumbH) / 2;


	RECT thumbRect = {thumbX, thumbY, thumbX + thumbW, thumbY + thumbH};

	// 绘制外边框（空心矩形）
	HPEN borderPen = CreatePen(PS_SOLID, 3, currentBorderColor);
	HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
	HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
	HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, nullBrush);
	Rectangle(memDC, outerRect.left, outerRect.top, outerRect.right, outerRect.bottom);

	// 填充 track（纯色长矩形）
	HBRUSH trackBrush = CreateSolidBrush(currentTrackColor);
	FillRect(memDC, &realTrackRect, trackBrush);

	// 绘制 thumb（纯色小矩形）
	HBRUSH thumbBrush = CreateSolidBrush(thumbColor);
	FillRect(memDC, &thumbRect, thumbBrush);

	// 复制到目标DC
	BitBlt(hdc, x, y, switchWidth, switchHeight, memDC, 0, 0, SRCCOPY);

	// 清理 GDI 对象
	SelectObject(memDC, oldPen);
	SelectObject(memDC, oldBrush);
	DeleteObject(borderPen);
	DeleteObject(trackBrush);
	DeleteObject(thumbBrush);
	SelectObject(memDC, oldBitmap);
	DeleteObject(memBitmap);
	DeleteDC(memDC);
}

// 获取开关区域(用于BTN_EXPAND_SWITCH)
static RECT GetSwitchRect(const RECT& btnRect) {
	RECT switchRect;
	const int switchWidth = 46; // 匹配Win8风格尺寸
	const int switchHeight = 18;
	switchRect.right = btnRect.right - 8;
	switchRect.left = switchRect.right - switchWidth;
	switchRect.top = btnRect.top + (btnRect.bottom - btnRect.top - switchHeight) / 2;
	switchRect.bottom = switchRect.top + switchHeight;
	return switchRect;
}

// Enhanced button subclass procedure
static LRESULT CALLBACK EnhancedButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
													UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	ButtonInfo* btnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (uMsg) {
	case WM_MOUSEMOVE:
		{
			if (btnInfo && !btnInfo->isHovered) {
				btnInfo->isHovered = true;
				InvalidateRect(hWnd, nullptr, TRUE);

				TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT)};
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				TrackMouseEvent(&tme);
			}

			// 检查是否在开关区域悬停(仅BTN_EXPAND_SWITCH)
			if (btnInfo && btnInfo->style == BTN_EXPAND_SWITCH) {
				POINT pt;
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);

				RECT rect;
				GetClientRect(hWnd, &rect);
				RECT switchRect = GetSwitchRect(rect);

				bool oldSwitchHovered = btnInfo->switchHovered;
				btnInfo->switchHovered = PtInRect(&switchRect, pt);

				if (oldSwitchHovered != btnInfo->switchHovered) {
					InvalidateRect(hWnd, nullptr, TRUE);
				}
			}
			break;
		}
	case WM_MOUSELEAVE:
		{
			if (btnInfo) {
				btnInfo->isHovered = false;
				btnInfo->switchHovered = false;
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			break;
		}
	case WM_LBUTTONDOWN:
		{
			if (btnInfo) {
				btnInfo->isPressed = true;
				InvalidateRect(hWnd, nullptr, TRUE);
				SetCapture(hWnd);
			}
			break;
		}
	case WM_LBUTTONUP:
		{
			if (btnInfo) {
				btnInfo->isPressed = false;
				ReleaseCapture();

				// 获取点击位置
				POINT pt;
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);

				// BTN_EXPAND_SWITCH: 区分点击开关还是展开区域
				if (btnInfo->style == BTN_EXPAND_SWITCH) {
					RECT rect;
					GetClientRect(hWnd, &rect);
					RECT switchRect = GetSwitchRect(rect);

					if (PtInRect(&switchRect, pt)) {
						// 点击在开关区域,切换开关状态
						btnInfo->switchState = !btnInfo->switchState;
						InvalidateRect(hWnd, nullptr, TRUE);

						// 发送自定义消息通知开关状态改变
						HWND hParent = GetParent(hWnd);
						if (hParent) {
							SendMessage(hParent, WM_COMMAND,
										MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED),
										(LPARAM)hWnd);
						}
					} else {
						// 点击在其他区域,切换展开状态
						btnInfo->isExpanded = !btnInfo->isExpanded;
						InvalidateRect(hWnd, nullptr, TRUE);

						// 发送自定义消息通知展开状态改变
						HWND hParent = GetParent(hWnd);
						if (hParent) {
							// 使用特殊通知码表示展开/折叠事件
							SendMessage(hParent, WM_COMMAND,
										MAKEWPARAM(GetDlgCtrlID(hWnd), BN_DOUBLECLICKED),
										(LPARAM)hWnd);
						}
					}
				} else {
					// 其他按钮类型的正常处理
					InvalidateRect(hWnd, nullptr, TRUE);
					HWND hParent = GetParent(hWnd);
					if (hParent) {
						SendMessage(hParent, WM_COMMAND,
									MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED),
									(LPARAM)hWnd);
					}
				}
			}
			break;
		}
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			RECT rect;
			GetClientRect(hWnd, &rect);

			// Use double buffering to reduce flicker
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

			HBRUSH hBrush = GetButtonBrush(btnInfo);
			FillRect(memDC, &rect, hBrush);

			// If it's a selected normal button (tab button), draw blue left border
			if (btnInfo && btnInfo->style == BTN_NORMAL && btnInfo->isSelected) {
				HBRUSH hBlueBrush = CreateSolidBrush(RGB(0, 120, 215));
				RECT blueRect = {0, 0, 4, rect.bottom}; // 3px wide blue border
				FillRect(memDC, &blueRect, hBlueBrush);
				DeleteObject(hBlueBrush);
			}

			SetBkMode(memDC, TRANSPARENT);
			HFONT oldFont = (HFONT)SelectObject(memDC, hFontButton);
			// 设置字体颜色
			if (btnInfo && btnInfo->style != BTN_NORMAL && btnInfo->style != BTN_EXPAND && btnInfo->style != BTN_EXPAND_SWITCH) {
				SetTextColor(memDC, RGB(255, 255, 255));
			} else {
				SetTextColor(memDC, RGB(0, 0, 0));
			}

			// Calculate icon and text positions
			int iconSize = 16; // Standard icon size
			int iconMargin = 8; // Margin between icon and text
			int totalContentWidth = 0;
			int iconX = 0, textX = 0;

			if (btnInfo && btnInfo->hIcon) {
				totalContentWidth = iconSize + iconMargin;
				if (!btnInfo->text.empty()) {
					SIZE textSize;
					GetTextExtentPoint32W(memDC, btnInfo->text.c_str(), static_cast<int>(btnInfo->text.length()), &textSize);
					totalContentWidth += textSize.cx;
				}

				// Position based on text alignment
				if (btnInfo->textAlign == TEXT_LEFT) {
					// Left align: start from left margin
					iconX = 8;
					textX = iconX + iconSize + iconMargin;
				} else if (btnInfo->textAlign == TEXT_RIGHT) {
					// Right align: work backwards from right
					iconX = rect.right - totalContentWidth - 8;
					textX = iconX + iconSize + iconMargin;
				} else {
					// Center align: center the total content
					int startX = (rect.right - totalContentWidth) / 2;
					iconX = startX;
					textX = startX + iconSize + iconMargin;
				}

				// Draw icon
				int iconY = (rect.bottom - iconSize) / 2;
				DrawIconEx(memDC, iconX, iconY, btnInfo->hIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
			}

			// 特殊处理折叠按钮
			if (btnInfo && btnInfo->style == BTN_EXPAND) {
				// 绘制箭头
				DrawExpandArrow(memDC, rect, btnInfo->isExpanded, btnInfo->isHovered);

				// 绘制文本（留出箭头空间）
				if (!btnInfo->text.empty()) {
					RECT textRect = {28, rect.top, rect.right - 8, rect.bottom}; // 左侧留28px给箭头
					DrawTextW(memDC, btnInfo->text.c_str(), -1, &textRect,
							DT_LEFT | DT_VCENTER | DT_SINGLELINE);
				}
			}
			// 特殊处理折叠+开关组合按钮
			else if (btnInfo && btnInfo->style == BTN_EXPAND_SWITCH) {
				// 绘制箭头
				DrawExpandArrow(memDC, rect, btnInfo->isExpanded, btnInfo->isHovered);

				// 绘制文本（留出箭头空间和右侧开关空间）
				if (!btnInfo->text.empty()) {
					RECT textRect = {28, rect.top, rect.right - 56, rect.bottom}; // 左侧留28px给箭头,右侧留56px给开关
					DrawTextW(memDC, btnInfo->text.c_str(), -1, &textRect,
							DT_LEFT | DT_VCENTER | DT_SINGLELINE);
				}

				// 绘制开关
				RECT switchRect = GetSwitchRect(rect);
				DrawSwitchWin8UI(memDC, switchRect.left, switchRect.top,
								btnInfo->switchState, btnInfo->switchHovered);
			} else if (btnInfo && !btnInfo->text.empty()) {
				DWORD alignFlag = DT_CENTER; // Default center alignment
				RECT textRect = rect;

				// Determine alignment flag and text rectangle
				switch (btnInfo->textAlign) {
				case TEXT_LEFT: alignFlag = DT_LEFT;
					if (btnInfo->hIcon) {
						textRect = {textX, rect.top, rect.right - 8, rect.bottom};
					} else {
						textRect = {8, rect.top, rect.right - 8, rect.bottom}; // 8px left margin
					}
					break;
				case TEXT_RIGHT: alignFlag = DT_RIGHT;
					if (btnInfo->hIcon) {
						textRect = {textX, rect.top, rect.right - 8, rect.bottom}; // 8px right margin
					} else {
						textRect = {rect.left, rect.top, rect.right - 8, rect.bottom}; // 8px right margin
					}
					break;
				case TEXT_CENTER:
				default: alignFlag = DT_CENTER;
					if (btnInfo->hIcon) {
						textRect = {textX, rect.top, rect.right, rect.bottom};
					} else {
						textRect = rect; // Full button width for centering
					}
					break;
				}

				DrawTextW(memDC, btnInfo->text.c_str(), -1, &textRect,
						alignFlag | DT_VCENTER | DT_SINGLELINE);
			}

			// Copy to screen in one operation
			BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

			SelectObject(memDC, oldFont);
			SelectObject(memDC, oldBitmap);
			DeleteObject(memBitmap);
			DeleteDC(memDC);

			EndPaint(hWnd, &ps);
			return 0;
		}
	case WM_NCDESTROY:
		{
			ButtonInfo* pBtnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			if (pBtnInfo) {
				delete pBtnInfo; // 释放内存
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0); // 清掉
			}
			RemoveWindowSubclass(hWnd, EnhancedButtonSubclassProc, uIdSubclass);
			break;
		}
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Create enhanced button
static HWND CreateEnhancedButton(HWND hParent, size_t uid, const std::wstring& text, int x, int y,
								int width, int height, HMENU hMenu, ButtonStyle style = BTN_NORMAL, HICON hIcon = nullptr,
								TextAlignment textAlign = TEXT_CENTER) {
	HWND hButton = CreateWindowW(L"BUTTON", text.c_str(),
								WS_CHILD | WS_VISIBLE,
								x, y, width, height, hParent, hMenu, nullptr, nullptr);

	if (hButton) {
		ButtonInfo* pBtnInfo = new ButtonInfo; // 动态分配
		pBtnInfo->hwnd = hButton;
		pBtnInfo->style = style;
		pBtnInfo->isHovered = false;
		pBtnInfo->isPressed = false;
		pBtnInfo->isSelected = false;
		pBtnInfo->isExpanded = false;
		pBtnInfo->text = text;
		pBtnInfo->hIcon = hIcon;
		pBtnInfo->textAlign = textAlign;

		//		ButtonInfo btnInfo;
		//		btnInfo.hwnd = hButton;
		//		btnInfo.style = style;
		//		btnInfo.isHovered = false;
		//		btnInfo.isPressed = false;
		//		btnInfo.isSelected = false;
		//		btnInfo.text = text;
		//		buttons.push_back(*pBtnInfo);

		SetWindowSubclass(hButton, EnhancedButtonSubclassProc,
						uid, 0);
		SetWindowLongPtr(hButton, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pBtnInfo));
		SendMessage(hButton, WM_SETFONT, (WPARAM)hFontButton, TRUE);
	}

	return hButton;
}

// Update tab button selection state
static void UpdateTabButtonSelection(const std::vector<HWND>& hTabButtons, int selectedIndex) {
	for (size_t i = 0; i < hTabButtons.size(); ++i) {
		ButtonInfo* btnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hTabButtons[i], GWLP_USERDATA));

		if (btnInfo) {
			bool shouldBeSelected = (static_cast<int>(i) == selectedIndex);
			if (btnInfo->isSelected != shouldBeSelected) {
				btnInfo->isSelected = shouldBeSelected;
				InvalidateRect(hTabButtons[i], nullptr, TRUE);
			}
		}
	}
}

// 设置折叠按钮的展开状态
static void SetExpandButtonState(HWND hButton, bool isExpanded) {
	ButtonInfo* btnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hButton, GWLP_USERDATA));
	if (btnInfo && (btnInfo->style == BTN_EXPAND || btnInfo->style == BTN_EXPAND_SWITCH)) {
		btnInfo->isExpanded = isExpanded;
		InvalidateRect(hButton, nullptr, TRUE);
	}
}

// 获取组合控件的开关状态
static bool GetExpandSwitchState(HWND hButton) {
	ButtonInfo* btnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hButton, GWLP_USERDATA));
	if (btnInfo && btnInfo->style == BTN_EXPAND_SWITCH) {
		return btnInfo->switchState;
	}
	return false;
}

// 设置组合控件的开关状态
static void SetExpandSwitchState(HWND hButton, bool state) {
	ButtonInfo* btnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hButton, GWLP_USERDATA));
	if (btnInfo && btnInfo->style == BTN_EXPAND_SWITCH) {
		btnInfo->switchState = state;
		InvalidateRect(hButton, nullptr, TRUE);
	}
}


// Cleanup button resources
static void CleanupButtonResources() {
	// Cleanup fonts
	if (hFontDefault) {
		DeleteObject(hFontDefault);
		hFontDefault = nullptr;
	}
	if (hFontButton) {
		DeleteObject(hFontButton);
		hFontButton = nullptr;
	}

	// Cleanup brushes
	if (hBrushButtonNormal) {
		DeleteObject(hBrushButtonNormal);
		hBrushButtonNormal = nullptr;
	}
	if (hBrushButtonHover) {
		DeleteObject(hBrushButtonHover);
		hBrushButtonHover = nullptr;
	}
	if (hBrushButtonPressed) {
		DeleteObject(hBrushButtonPressed);
		hBrushButtonPressed = nullptr;
	}
	if (hBrushButtonSelected) {
		DeleteObject(hBrushButtonSelected);
		hBrushButtonSelected = nullptr;
	}
	if (hBrushButtonPrimary) {
		DeleteObject(hBrushButtonPrimary);
		hBrushButtonPrimary = nullptr;
	}
	if (hBrushButtonSuccess) {
		DeleteObject(hBrushButtonSuccess);
		hBrushButtonSuccess = nullptr;
	}
	if (hBrushButtonDanger) {
		DeleteObject(hBrushButtonDanger);
		hBrushButtonDanger = nullptr;
	}
	if (hBrushPrimaryHover) {
		DeleteObject(hBrushPrimaryHover);
		hBrushPrimaryHover = nullptr;
	}
	if (hBrushSuccessHover) {
		DeleteObject(hBrushSuccessHover);
		hBrushSuccessHover = nullptr;
	}
	if (hBrushDangerHover) {
		DeleteObject(hBrushDangerHover);
		hBrushDangerHover = nullptr;
	}
	if (hBrushButtonExpand) {
		DeleteObject(hBrushButtonExpand);
		hBrushButtonExpand = nullptr;
	}
	if (hBrushButtonExpandHover) {
		DeleteObject(hBrushButtonExpandHover);
		hBrushButtonExpandHover = nullptr;
	}

}
