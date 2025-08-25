#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls
#include <gdiplus.h> // For anti-aliased drawing

#include "BaseTools.hpp"
#include "Constants.hpp"
#include "SwitchViewStyle.h"

#pragma comment(lib, "gdiplus.lib")


//static ULONG_PTR g_gdiplusToken = 0;
//static bool g_gdiplusInitialized = false;
//
//struct SwitchInfo {
//	HWND hwnd;
//	bool isOn;
//	bool isHovered;
//	bool isPressed;
//	bool isAnimating;
//	float animationProgress; // 0.0f to 1.0f
//	DWORD animationStartTime;
//};
//
//static void UpdateSwitchAnimation(SwitchInfo *switchInfo) {
//	if (!switchInfo->isAnimating) return;
//
//	DWORD currentTime = GetTickCount();
//	DWORD elapsed = currentTime - switchInfo->animationStartTime;
//	const DWORD ANIMATION_DURATION = 150; // 150ms animation
//
//	if (elapsed >= ANIMATION_DURATION) {
//		switchInfo->isAnimating = false;
//		switchInfo->animationProgress = switchInfo->isOn ? 1.0f : 0.0f;
//	} else {
//		float progress = static_cast<float>(elapsed) / ANIMATION_DURATION;
//		// Smooth easing function
//		progress = progress * progress * (3.0f - 2.0f * progress);
//		switchInfo->animationProgress = switchInfo->isOn ? progress : (1.0f - progress);
//	}
//}

static bool GetSwitchState(HWND hSwitch) {
	SwitchInfo *switchInfo = reinterpret_cast<SwitchInfo*>(GetWindowLongPtr(hSwitch, GWLP_USERDATA));
	return switchInfo ? switchInfo->isOn : false;
}

static void SetSwitchState(HWND hSwitch, bool state) {
	SwitchInfo *switchInfo = reinterpret_cast<SwitchInfo*>(GetWindowLongPtr(hSwitch, GWLP_USERDATA));
	if (switchInfo && switchInfo->isOn != state) {
		switchInfo->isOn = state;
		switchInfo->isAnimating = true;
		switchInfo->animationStartTime = GetTickCount();
		SetTimer(hSwitch, 1001, 16, nullptr); // Start animation
		InvalidateRect(hSwitch, nullptr, FALSE);
	}
}

static LRESULT CALLBACK SwitchControlSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
												  UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	SwitchInfo *switchInfo = reinterpret_cast<SwitchInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (uMsg) {
		case WM_TIMER:
			if (wParam == 1001) { // Animation timer
				UpdateSwitchAnimation(switchInfo);
				InvalidateRect(hWnd, nullptr, FALSE);
				if (switchInfo && !switchInfo->isAnimating) {
					KillTimer(hWnd, 1001);
				}
				return 0;
			}
			break;

		case WM_MOUSEMOVE: {
			if (switchInfo && !switchInfo->isHovered) {
				switchInfo->isHovered = true;
				InvalidateRect(hWnd, nullptr, FALSE);

				TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT)};
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				TrackMouseEvent(&tme);
			}
			break;
		}
		case WM_MOUSELEAVE: {
			if (switchInfo) {
				switchInfo->isHovered = false;
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			break;
		}
		case WM_LBUTTONDOWN: {
			if (switchInfo) {
				switchInfo->isPressed = true;
				InvalidateRect(hWnd, nullptr, FALSE);
				SetCapture(hWnd);
			}
			return 0; // Ensure we handle the message
		}
		case WM_LBUTTONUP: {
			if (switchInfo) {
				switchInfo->isPressed = false;
				ReleaseCapture();

				// Check if click is within the switch control bounds
				POINT pt;
				GetCursorPos(&pt);
				ScreenToClient(hWnd, &pt);

				RECT rect;
				GetClientRect(hWnd, &rect);

				if (PtInRect(&rect, pt)) {
					// Toggle switch state
					switchInfo->isOn = !switchInfo->isOn;
					switchInfo->isAnimating = true;
					switchInfo->animationStartTime = GetTickCount();
					SetTimer(hWnd, 1001, 16, nullptr); // 60 FPS animation

					// Notify parent of state change
					HWND hParent = GetParent(hWnd);
					if (hParent) {
						SendMessage(hParent, WM_COMMAND,
									MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED),
									(LPARAM)hWnd);
					}
				}
				InvalidateRect(hWnd, nullptr, FALSE);
			}
			return 0; // Ensure we handle the message
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			SwitchPaintWin8(hWnd,hdc,switchInfo);
			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_NCDESTROY: {
			SwitchInfo* pSwitchInfo = reinterpret_cast<SwitchInfo*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			if (pSwitchInfo) {
				delete pSwitchInfo;  // 释放内存
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0); // 清掉
			}
			KillTimer(hWnd, 1001);
			RemoveWindowSubclass(hWnd, SwitchControlSubclassProc, uIdSubclass);
			break;
		}
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}





static HWND CreateSwitchControl(HWND hParent,size_t subId, int x, int y, int width, int height,
								HMENU hMenu, bool initialState = false) {
	// Initialize GDI+ if not already done
	
	HWND hSwitch = CreateWindowW(L"STATIC", L"",
								 WS_CHILD | WS_VISIBLE | SS_NOTIFY,
								 x, y, width, height, hParent, hMenu, nullptr, nullptr);

	if (hSwitch) {
		SwitchInfo* switchInfo = new SwitchInfo();
		switchInfo->hwnd = hSwitch;
		switchInfo->isOn = initialState;
		switchInfo->isHovered = false;
		switchInfo->isPressed = false;
		switchInfo->isAnimating = false;
		switchInfo->animationProgress = initialState ? 1.0f : 0.0f;
		switchInfo->animationStartTime = 0;
		SetWindowLongPtr(hSwitch, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(switchInfo));

		SetWindowSubclass(hSwitch, SwitchControlSubclassProc,
						  subId, 0);
	}

	return hSwitch;
}
