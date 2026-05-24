#pragma once

#include <windows.h>
#include <string>
#include <ShlObj.h>
#pragma comment(lib, "winmm.lib")

#include "util/LogUtil.hpp"
#include "util/ShortcutUtil.hpp"
#include <unordered_set>

class FileAction;
class BaseAction;

enum MenuCommand : UINT {
	// 防止和弹出菜单的esc 0 值冲突
	IDM_REMOVE_ITEM = 9999,
	IDM_CONTEXT_MENU_RENAME_ITEM,
	IDM_RUN_AS_ADMIN,
	IDM_OPEN_IN_CONSOLE,
	IDM_KILL_PROCESS,
	IDM_COPY_PATH,
	IDM_COPY_TARGET_PATH
};

const std::unordered_set<std::wstring> systemProcesses = {
	L"explorer.exe",
	L"svchost.exe",
	L"wininit.exe",
	L"csrss.exe",
	L"winlogon.exe",
	L"lsass.exe",
	L"services.exe",
	L"smss.exe",
	L"sihost.exe",
	L"wmiapsrv.exe",
	L"dwm.exe",
	L"dllhost.exe",
	L"runtimebroker.exe",
	L"applicationframehost.exe",
	L"system", // SYSTEM 内核进程
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
	AppendMenuW(hMenu, MF_STRING, IDM_REMOVE_ITEM, L"排除该索引(未完善)");
	AppendMenuW(hMenu, MF_STRING, IDM_CONTEXT_MENU_RENAME_ITEM, L"重映射命名该索引(未完善)");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_RUN_AS_ADMIN, L"以管理员身份运行");
	AppendMenuW(hMenu, MF_STRING, IDM_OPEN_IN_CONSOLE, L"在控制台打开该项");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_KILL_PROCESS, L"杀死进程");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_COPY_PATH, L"复制路径");
	AppendMenuW(hMenu, MF_STRING, IDM_COPY_TARGET_PATH, L"复制目标路径");

	DWORD attr = GetFileAttributesW(path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
		EnableMenuItem(hMenu, IDM_RUN_AS_ADMIN, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hMenu, IDM_OPEN_IN_CONSOLE, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hMenu, IDM_KILL_PROCESS, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hMenu, IDM_COPY_TARGET_PATH, MF_BYCOMMAND | MF_GRAYED);
	} else {
		if (EndsWithAny(path, {L".lnk"})) {
			std::wstring actualPath = GetShortcutTarget(path);
			if (!EndsWithAny(actualPath, {L".exe"})) {
				EnableMenuItem(hMenu, IDM_KILL_PROCESS, MF_BYCOMMAND | MF_GRAYED);
			}
		} else {
			EnableMenuItem(hMenu, IDM_COPY_TARGET_PATH, MF_BYCOMMAND | MF_GRAYED);
			if (!EndsWithAny(path, {L".exe"})) {
				EnableMenuItem(hMenu, IDM_KILL_PROCESS, MF_BYCOMMAND | MF_GRAYED);
			}
		}
	}

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

