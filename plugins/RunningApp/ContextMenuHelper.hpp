#pragma once

#include <windows.h>
#include <string>
#include <ShlObj.h>
#pragma comment(lib, "winmm.lib")

#include "util/LogUtil.hpp"

enum : UINT {
	// 防止和弹出菜单的esc 0 值冲突
	IDM_OPEN_APP = 9999,
	IDM_OPEN_IN_CONSOLE,
	IDM_KILL_PROCESS,
	IDM_COPY_PATH,
};


static UINT ShowMyContextMenu(HWND hWnd, const std::wstring& path, POINT screenPt) {
	ConsolePrintln(L"MyMenu", L"enter path=" + path +
		L", x=" + std::to_wstring(screenPt.x) +
		L", y=" + std::to_wstring(screenPt.y));
	HMENU hMenu = CreatePopupMenu();
	if (!hMenu) {
		ConsolePrintln(L"MyMenu", L"CreatePopupMenu failed err=" + std::to_wstring(GetLastError()));
		return 0;
	}
	AppendMenuW(hMenu, MF_STRING, IDM_OPEN_APP, L"运行应用程序");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_OPEN_IN_CONSOLE, L"在控制台打开该项");
	AppendMenuW(hMenu, MF_STRING, IDM_KILL_PROCESS, L"杀死进程");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_COPY_PATH, L"复制路径");

	// DWORD attr = GetFileAttributesW(path.c_str());
	SetForegroundWindow(hWnd);
	UINT cmd = TrackPopupMenuEx(
		hMenu,
		TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NOANIMATION,
		screenPt.x, screenPt.y,
		hWnd,
		nullptr
	);
	PostMessageW(hWnd, WM_NULL, 0, 0);
	ConsolePrintln(L"MyMenu", L"TrackPopupMenuEx cmd=" + std::to_wstring(cmd) +
		L", count=" + std::to_wstring(GetMenuItemCount(hMenu)));
	DestroyMenu(hMenu);
	return cmd;
}
