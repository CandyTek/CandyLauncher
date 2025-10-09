#pragma once

#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <commctrl.h>


// ComboBox brushes
inline HBRUSH hBrushComboNormal = nullptr;
inline HBRUSH hBrushComboHover = nullptr;
inline HBRUSH hBrushComboFocus = nullptr;
inline HBRUSH hBrushComboDropped = nullptr;

// ComboBox information structure
struct ComboBoxInfo {
	HWND hwnd{};
	bool isHovered{};
	bool isDropped{};
	std::wstring placeholder;
};


// Initialize ComboBox resources
static void InitializeCustomComboBoxResources() {
	if (!hBrushComboNormal) hBrushComboNormal = CreateSolidBrush(RGB(255, 255, 255));
	if (!hBrushComboHover) hBrushComboHover = CreateSolidBrush(RGB(240, 240, 240));
	if (!hBrushComboFocus) hBrushComboFocus = CreateSolidBrush(RGB(255, 255, 255));
	if (!hBrushComboDropped) hBrushComboDropped = CreateSolidBrush(RGB(230, 243, 255));
}

// Get appropriate brush for combobox state
static HBRUSH GetComboBoxBrush(ComboBoxInfo* comboInfo) {
	if (!comboInfo) return hBrushComboNormal;

	if (comboInfo->isDropped) {
		return hBrushComboDropped;
	}
	if (comboInfo->isHovered) {
		return hBrushComboHover;
	}
	return hBrushComboNormal;
}

// Draw dropdown arrow
static void DrawDropdownArrow(HDC hdc, RECT rect, bool isHovered, bool isDropped) {
	int centerX = rect.right - 20;
	int centerY = rect.top + (rect.bottom - rect.top) / 2;

	COLORREF arrowColor = isHovered || isDropped ? RGB(0, 120, 215) : RGB(100, 100, 100);
	HPEN hPen = CreatePen(PS_SOLID, 2, arrowColor);
	HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

	// Draw down arrow
	MoveToEx(hdc, centerX - 4, centerY - 2, NULL);
	LineTo(hdc, centerX, centerY + 2);
	LineTo(hdc, centerX + 4, centerY - 2);

	SelectObject(hdc, hOldPen);
	DeleteObject(hPen);
}

// Enhanced ComboBox subclass procedure
static LRESULT CALLBACK EnhancedComboBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
													UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	ComboBoxInfo* comboInfo = reinterpret_cast<ComboBoxInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (uMsg) {
	case WM_MOUSEMOVE:
		{
			if (comboInfo && !comboInfo->isHovered) {
				comboInfo->isHovered = true;
				InvalidateRect(hWnd, nullptr, TRUE);

				TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT)};
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				TrackMouseEvent(&tme);
			}
			break;
		}
	case WM_MOUSELEAVE:
		{
			if (comboInfo) {
				comboInfo->isHovered = false;
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			break;
		}
	case WM_COMMAND:
		{
			if (HIWORD(wParam) == CBN_DROPDOWN) {
				if (comboInfo) {
					comboInfo->isDropped = true;
					InvalidateRect(hWnd, nullptr, TRUE);
				}
			} else if (HIWORD(wParam) == CBN_CLOSEUP) {
				if (comboInfo) {
					comboInfo->isDropped = false;
					InvalidateRect(hWnd, nullptr, TRUE);
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

			// Use double buffering
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

			// Fill background
			HBRUSH hBrush = GetComboBoxBrush(comboInfo);
			FillRect(memDC, &rect, hBrush);

			// Draw border
			COLORREF borderColor = RGB(180, 180, 180);
			if (comboInfo) {
				if (comboInfo->isDropped) {
					borderColor = RGB(0, 120, 215);
				} else if (comboInfo->isHovered) {
					borderColor = RGB(120, 120, 120);
				}
			}

			HPEN hPen = CreatePen(PS_SOLID, 1, borderColor);
			HPEN hOldPen = (HPEN)SelectObject(memDC, hPen);
			HBRUSH hOldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));

			Rectangle(memDC, rect.left, rect.top, rect.right, rect.bottom);

			SelectObject(memDC, hOldBrush);
			SelectObject(memDC, hOldPen);
			DeleteObject(hPen);

			// Draw dropdown arrow
			if (comboInfo) {
				DrawDropdownArrow(memDC, rect, comboInfo->isHovered, comboInfo->isDropped);
			}

			// Get current selection text
			int curSel = (int)SendMessage(hWnd, CB_GETCURSEL, 0, 0);
			if (curSel != CB_ERR) {
				int textLen = (int)SendMessage(hWnd, CB_GETLBTEXTLEN, curSel, 0);
				if (textLen > 0) {
					std::wstring text(textLen + 1, L'\0');
					SendMessage(hWnd, CB_GETLBTEXT, curSel, (LPARAM)text.data());
					text.resize(textLen);

					SetBkMode(memDC, TRANSPARENT);
					SetTextColor(memDC, RGB(0, 0, 0));
					HFONT oldFont = (HFONT)SelectObject(memDC, hFontDefault);

					RECT textRect = {rect.left + 8, rect.top, rect.right - 30, rect.bottom};
					DrawTextW(memDC, text.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

					SelectObject(memDC, oldFont);
				}
			}

			// Copy to screen
			BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

			SelectObject(memDC, oldBitmap);
			DeleteObject(memBitmap);
			DeleteDC(memDC);

			EndPaint(hWnd, &ps);
			return 0;
		}
	case WM_NCDESTROY:
		{
			ComboBoxInfo* pComboInfo = reinterpret_cast<ComboBoxInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			if (pComboInfo) {
				delete pComboInfo;
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
			}
			RemoveWindowSubclass(hWnd, EnhancedComboBoxSubclassProc, uIdSubclass);
			break;
		}
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Create enhanced ComboBox
static HWND CreateEnhancedComboBox(HWND hParent, size_t uid, int x, int y, int width, int height,
									HMENU hMenu, const std::wstring& placeholder = L"") {
	// For CBS_DROPDOWNLIST, height parameter includes dropdown height
	// Use a reasonable dropdown height (e.g., 200px for dropdown area)
	int totalHeight = height > 100 ? height : 200;
	HWND hCombo = CreateWindowW(L"COMBOBOX", L"",
								WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
								x, y, width, totalHeight, hParent, hMenu, nullptr, nullptr);

	if (hCombo) {
		ComboBoxInfo* pComboInfo = new ComboBoxInfo;
		pComboInfo->hwnd = hCombo;
		pComboInfo->isHovered = false;
		pComboInfo->isDropped = false;
		pComboInfo->placeholder = placeholder;

		SetWindowSubclass(hCombo, EnhancedComboBoxSubclassProc, uid, 0);
		SetWindowLongPtr(hCombo, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pComboInfo));
		SendMessage(hCombo, WM_SETFONT, (WPARAM)hFontDefault, TRUE);
	}

	return hCombo;
}

// Cleanup button resources
static void CleanupComboBoxResources() {

	// Cleanup ComboBox brushes
	if (hBrushComboNormal) {
		DeleteObject(hBrushComboNormal);
		hBrushComboNormal = nullptr;
	}
	if (hBrushComboHover) {
		DeleteObject(hBrushComboHover);
		hBrushComboHover = nullptr;
	}
	if (hBrushComboFocus) {
		DeleteObject(hBrushComboFocus);
		hBrushComboFocus = nullptr;
	}
	if (hBrushComboDropped) {
		DeleteObject(hBrushComboDropped);
		hBrushComboDropped = nullptr;
	}
}
