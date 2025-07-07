#pragma once
#include <windows.h>
#include <WinUser.h>
#include <commctrl.h>
#include <memory>
// 不能少
#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)
#define TRAY_MENU_ID_BASE 10000
#define TRAY_MENU_ID_BASE_END 10011

class TrayMenuManager
{
public:
	HMENU hTrayMenu = nullptr;
	NOTIFYICONDATA nid ;

	TrayMenuManager();
	void Init(HWND parent, HINSTANCE hInstance);
	static void TrayMenuClick(int position, HWND hWnd,HWND hEdit);
	void TrayMenuShow(HWND hWnd) const;
	void TrayMenuDestroy();
	void ShowTrayIcon();
	void HideTrayIcon();
};

