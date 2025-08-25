#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <commctrl.h>

// Button styles enumeration
enum ButtonStyle {
	BTN_NORMAL = 0,
	BTN_PRIMARY = 1,
	BTN_SUCCESS = 2,
	BTN_DANGER = 3
};

// Button information structure
struct ButtonInfo {
	HWND hwnd{};
	ButtonStyle style;
	bool isHovered{};
	bool isPressed{};
	bool isSelected{};
	std::wstring text;
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

// Initialize button resources
static void InitializeButtonResources() {
	// Initialize fonts
	if (!hFontDefault) {
		hFontDefault = CreateFontW(
			20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");
	}
	
	if (!hFontButton) {
		hFontButton = CreateFontW(
			18, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");
	}
	
	// Initialize brushes
	if (!hBrushButtonNormal) hBrushButtonNormal = CreateSolidBrush(RGB(230,230,230));
	if (!hBrushButtonHover) hBrushButtonHover = CreateSolidBrush(RGB(207,207,207));
	if (!hBrushButtonPressed) hBrushButtonPressed = CreateSolidBrush(RGB(184,184,184));
	if (!hBrushButtonSelected) hBrushButtonSelected = CreateSolidBrush(RGB(220, 235, 255));
	if (!hBrushButtonPrimary) hBrushButtonPrimary = CreateSolidBrush(RGB(0, 120, 215));
	if (!hBrushButtonSuccess) hBrushButtonSuccess = CreateSolidBrush(RGB(40, 167, 69));
	if (!hBrushButtonDanger) hBrushButtonDanger = CreateSolidBrush(RGB(220, 53, 69));
	if (!hBrushPrimaryHover) hBrushPrimaryHover = CreateSolidBrush(RGB(56,165,250));
	if (!hBrushSuccessHover) hBrushSuccessHover = CreateSolidBrush(RGB(20, 147, 49));
	if (!hBrushDangerHover) hBrushDangerHover = CreateSolidBrush(RGB(200, 33, 49));
}

// Get appropriate brush for button state
static HBRUSH GetButtonBrush(ButtonInfo *btnInfo) {
	if (!btnInfo) return hBrushButtonNormal;

	if (btnInfo->isPressed) {
		return hBrushButtonPressed;
	}

	// If it's a selected normal button (tab button), use selected background
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

// Enhanced button subclass procedure
static LRESULT CALLBACK EnhancedButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
													UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	ButtonInfo* btnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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
				// Simulate button click
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

			// Use double buffering to reduce flicker
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			HBITMAP oldBitmap = (HBITMAP) SelectObject(memDC, memBitmap);

			HBRUSH hBrush = GetButtonBrush(btnInfo);
			FillRect(memDC, &rect, hBrush);

			// If it's a selected normal button (tab button), draw blue left border
			if (btnInfo && btnInfo->style == BTN_NORMAL && btnInfo->isSelected) {
				HBRUSH hBlueBrush = CreateSolidBrush(RGB(0, 120, 215));
				RECT blueRect = {0, 0, 3, rect.bottom}; // 3px wide blue border
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

			// Copy to screen in one operation
			BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

			SelectObject(memDC, oldFont);
			SelectObject(memDC, oldBitmap);
			DeleteObject(memBitmap);
			DeleteDC(memDC);

			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_NCDESTROY: {
			ButtonInfo* pBtnInfo = reinterpret_cast<ButtonInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			if (pBtnInfo) {
				delete pBtnInfo;  // 释放内存
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0); // 清掉
			}
			RemoveWindowSubclass(hWnd, EnhancedButtonSubclassProc, uIdSubclass);
			break;
		}
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Create enhanced button
static HWND CreateEnhancedButton(HWND hParent,size_t uid,const std::wstring &text, int x, int y,
								int width, int height, HMENU hMenu, ButtonStyle style = BTN_NORMAL) {
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
		pBtnInfo->text = text;

//		ButtonInfo btnInfo;
//		btnInfo.hwnd = hButton;
//		btnInfo.style = style;
//		btnInfo.isHovered = false;
//		btnInfo.isPressed = false;
//		btnInfo.isSelected = false;
//		btnInfo.text = text;
//		buttons.push_back(*pBtnInfo);

		SetWindowSubclass(hButton, EnhancedButtonSubclassProc,
						  uid , 0);
		SetWindowLongPtr(hButton, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pBtnInfo));
		SendMessage(hButton, WM_SETFONT, (WPARAM) hFontButton, TRUE);
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

// Cleanup button resources
static void CleanupButtonResources() {
	// Cleanup fonts
	if (hFontDefault) { DeleteObject(hFontDefault); hFontDefault = nullptr; }
	if (hFontButton) { DeleteObject(hFontButton); hFontButton = nullptr; }
	
	// Cleanup brushes
	if (hBrushButtonNormal) { DeleteObject(hBrushButtonNormal); hBrushButtonNormal = nullptr; }
	if (hBrushButtonHover) { DeleteObject(hBrushButtonHover); hBrushButtonHover = nullptr; }
	if (hBrushButtonPressed) { DeleteObject(hBrushButtonPressed); hBrushButtonPressed = nullptr; }
	if (hBrushButtonSelected) { DeleteObject(hBrushButtonSelected); hBrushButtonSelected = nullptr; }
	if (hBrushButtonPrimary) { DeleteObject(hBrushButtonPrimary); hBrushButtonPrimary = nullptr; }
	if (hBrushButtonSuccess) { DeleteObject(hBrushButtonSuccess); hBrushButtonSuccess = nullptr; }
	if (hBrushButtonDanger) { DeleteObject(hBrushButtonDanger); hBrushButtonDanger = nullptr; }
	if (hBrushPrimaryHover) { DeleteObject(hBrushPrimaryHover); hBrushPrimaryHover = nullptr; }
	if (hBrushSuccessHover) { DeleteObject(hBrushSuccessHover); hBrushSuccessHover = nullptr; }
	if (hBrushDangerHover) { DeleteObject(hBrushDangerHover); hBrushDangerHover = nullptr; }
}
