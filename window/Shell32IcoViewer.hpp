#pragma once

#include "dwmapi.h"

#define ICON_COUNT 330
#define ICON_SIZE  16
#define ICON_SPACING 30   // 图标间距
#define ICONS_PER_ROW 20  // 每行多少个

static LRESULT CALLBACK Shell32IcoViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			SetBkMode(hdc, TRANSPARENT);   // 透明背景
			SetTextColor(hdc, RGB(0, 0, 0)); // 黑色文字

			int x = 0, y = 0;
			for (int i = 0; i < ICON_COUNT; i++) {
				HICON hIcon = LoadShellIcon(i, ICON_SIZE, ICON_SIZE);
				if (hIcon) {
					// 画图标
					DrawIconEx(hdc, x, y, hIcon, ICON_SIZE, ICON_SIZE, 0, NULL, DI_NORMAL);

					// 在图标下方显示序号
					wchar_t buf[16];
					swprintf(buf, 16, L"%d", i);
					TextOutW(hdc, x, y + ICON_SIZE + 2, buf, static_cast<int>(wcslen(buf)));
				}

				x += ICON_SPACING;
				if ((i + 1) % ICONS_PER_ROW == 0) {
					x = 0;
					y += ICON_SPACING + 12; // 多留一点高度放数字
				}
			}

			EndPaint(hwnd, &ps);
	}
		break;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

/**
 * SHELL32 图标查看器
 * @param hInstance 
 */
static void ShowShell32IcoViewer(HINSTANCE hInstance) {
	const wchar_t CLASS_NAME[] = L"ShellIconWindow";

	WNDCLASS wc = {};
	wc.lpfnWndProc = Shell32IcoViewerWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
			0,
			CLASS_NAME,
			L"Shell32.dll 330 icons",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 700, 800,
			NULL,
			NULL,
			hInstance,
			NULL
	);

	if (!hwnd) return;

	ShowWindow(hwnd, SW_SHOW);
}
