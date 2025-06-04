#pragma once
#include <windows.h>
#include <WinUser.h>
#include <commctrl.h>
#include <memory>
// 不能少
#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)

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
};

