#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>
#include <CommCtrl.h>


static LRESULT CALLBACK CustomBgcolorControlClassProc(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
		UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uMsg) {
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			// 从 dwRefData 取颜色
			COLORREF color = (COLORREF)dwRefData;
			HBRUSH hBrush = CreateSolidBrush(color);
			FillRect(hdc, &ps.rcPaint, hBrush);
			DeleteObject(hBrush);

			EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_ERASEBKGND:
			// 返回 1 表示我们自己画背景，避免闪烁
			return 1;

		case WM_NCDESTROY:
			RemoveWindowSubclass(hWnd, CustomBgcolorControlClassProc, uIdSubclass);
			break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
