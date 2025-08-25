#pragma once
#include <windows.h>
#include <WinUser.h>
#include <commctrl.h>
#include <memory>
// 不能少
#include <shellapi.h>

#define WM_TRAYICON (WM_USER + 1)

// Tray icon constants
constexpr UINT TRAY_ICON_ID = 1001;

// Tray menu item IDs
constexpr int TRAY_MENU_ID_BASE = 10000;
constexpr int TRAY_MENU_ID_SHOW_WINDOW = 10001;
constexpr int TRAY_MENU_ID_OPEN_FOLDER = 10002;
constexpr int TRAY_MENU_ID_EDIT_CONFIG = 10003;
constexpr int TRAY_MENU_ID_GITHUB = 10004;
constexpr int TRAY_MENU_ID_ABOUT = 10008;
constexpr int TRAY_MENU_ID_RESTART = 10005;
constexpr int TRAY_MENU_ID_EXIT = 10006;
constexpr int TRAY_MENU_ID_HELP = 10007;
constexpr int TRAY_MENU_ID_SETTINGS = 10010;
constexpr int TRAY_MENU_ID_BASE_END = 10011;

class TrayMenuManager
{
public:
	static void Init(HWND parent, HINSTANCE hInstance);
	static void TrayMenuClick(int position, HWND hWnd, HWND hEdit);
	static void TrayMenuShow(HWND hWnd);
	static void TrayMenuDestroy();
	static void ShowTrayIcon();
	static void HideTrayIcon();

};
