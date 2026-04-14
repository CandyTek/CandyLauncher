#pragma once
#include <windows.h>
#include <WinUser.h>
#include <commctrl.h>
#include <memory>
// 不能少
#include <shellapi.h>

#include "util/BitmapUtil.hpp"
#include "util/MainTools.hpp"
#include "common/Resource.h"

#define WM_TRAYICON (WM_USER + 1)

// Tray icon constants
constexpr UINT TRAY_ICON_ID = 1001;

// Tray menu item IDs
constexpr int TRAY_MENU_ID_BASE = 10000;
constexpr int TRAY_MENU_ID_SHOW_WINDOW = 10001;
constexpr int TRAY_MENU_ID_OPEN_FOLDER = 10002;
constexpr int TRAY_MENU_ID_GITHUB = 10004;
constexpr int TRAY_MENU_ID_ABOUT = 10008;
constexpr int TRAY_MENU_ID_RESTART = 10005;
constexpr int TRAY_MENU_ID_EXIT = 10006;
constexpr int TRAY_MENU_ID_HELP = 10007;
constexpr int TRAY_MENU_ID_CHECK_UPDATE = 10009;
constexpr int TRAY_MENU_ID_SETTINGS = 10010;
constexpr int TRAY_MENU_ID_BASE_END = 10012;

static HMENU g_hTrayMenu = nullptr;
static HMENU g_hMoreSub = nullptr;
static NOTIFYICONDATAW g_nid = [] {
	NOTIFYICONDATAW nid{};
	nid.cbSize = sizeof(nid);
	nid.hWnd = nullptr; // 先留空，稍后覆盖
	nid.uID = TRAY_ICON_ID;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	lstrcpynW(nid.szTip, L"CandyLauncher", ARRAYSIZE(nid.szTip));
	return nid;
}();


// 位图（用于 SetMenuItemBitmaps）
static HBITMAP gBmpShow = nullptr;
static HBITMAP gBmpHelp = nullptr;
static HBITMAP gBmpSettings = nullptr;
static HBITMAP gBmpExit = nullptr;
static HBITMAP gBmpMore = nullptr, gBmpEdit = nullptr, gBmpFolder = nullptr, gBmpGithub = nullptr, gBmpRestart = nullptr;
static HBITMAP gBmpAbout = nullptr;
static HBITMAP gBmpCheckUpdate = nullptr;

// ---- 构建托盘菜单（用 SetMenuItemBitmaps 绑定位图）----
static void Init(HWND parent, HINSTANCE hInstance) {
	gBmpShow = LoadShell32IconProperAsBitmap(22);
	gBmpHelp = LoadShell32IconProperAsBitmap(154, 16, 16);
	gBmpSettings = LoadShell32IconProperAsBitmap(71, 16, 16);
	gBmpExit = LoadShell32IconProperAsBitmap(131, 16, 16);
	// gBmpMore = LoadShell32IconProperAsBitmap(293, 16, 16);
	gBmpEdit = LoadShell32IconProperAsBitmap(269, 16, 16);
	gBmpFolder = LoadShell32IconProperAsBitmap(4, 16, 16);
	gBmpGithub = LoadShell32IconProperAsBitmap(220, 16, 16);
	gBmpRestart = LoadShell32IconProperAsBitmap(238, 16, 16);
	gBmpAbout = LoadShell32IconProperAsBitmap(277, 16, 16);
	gBmpCheckUpdate = LoadShell32IconProperAsBitmap(239, 16, 16);

	// 主菜单
	g_hTrayMenu = CreatePopupMenu();

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_SHOW_WINDOW, L"打开主窗口(&C)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_SHOW_WINDOW, MF_BYCOMMAND, gBmpShow, gBmpShow);

	AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_HELP, L"帮助文档(&H)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_HELP, MF_BYCOMMAND, gBmpHelp, gBmpHelp);

	// 子菜单 “更多...”
	g_hMoreSub = CreatePopupMenu();

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_OPEN_FOLDER, L"在资源管理器打开路径(&I)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_OPEN_FOLDER, MF_BYCOMMAND, gBmpFolder, gBmpFolder);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_GITHUB, L"打开 Github 主页(&G)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_GITHUB, MF_BYCOMMAND, gBmpGithub, gBmpGithub);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_ABOUT, L"关于(&A)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_ABOUT, MF_BYCOMMAND, gBmpAbout, gBmpAbout);

	AppendMenuW(g_hMoreSub, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_CHECK_UPDATE, L"检查更新(&C)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_CHECK_UPDATE, MF_BYCOMMAND, gBmpCheckUpdate, gBmpCheckUpdate);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_RESTART, L"重启软件(&R)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_RESTART, MF_BYCOMMAND, gBmpRestart, gBmpRestart);

	// 把子菜单加到主菜单，并给“更多...”本身设位图（需要按位置设置）
	int posMore = GetMenuItemCount(g_hTrayMenu);
	AppendMenuW(g_hTrayMenu, MF_STRING | MF_POPUP, (UINT_PTR)g_hMoreSub, L"更多...(&M)");
	SetMenuItemBitmaps(g_hTrayMenu, posMore, MF_BYPOSITION, gBmpMore, gBmpMore);
	AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, nullptr);

	// 设置 & 退出
	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_SETTINGS, L"设置(&S)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_SETTINGS, MF_BYCOMMAND, gBmpSettings, gBmpSettings);

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_EXIT, L"退出(&X)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_EXIT, MF_BYCOMMAND, gBmpExit, gBmpExit);
}

static void TrayMenuShow(HWND hWnd) {
	POINT pt;
	GetCursorPos(&pt); // 获取鼠标位置
	SetForegroundWindow(hWnd); // 让菜单不会点击后立即消失
	TrackPopupMenu(g_hTrayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
					pt.x, pt.y, 0, hWnd, nullptr);
}

static void TrayMenuDestroy() {
	Shell_NotifyIcon(NIM_DELETE, &g_nid);

	// 位图需要销毁（icons 使用了 LR_SHARED 不需要 DestroyIcon）
	auto SAFE_DEL_BMP = [](HBITMAP& h) {
		if (h) {
			DeleteObject(h);
			h = nullptr;
		}
	};
	SAFE_DEL_BMP(gBmpShow);
	SAFE_DEL_BMP(gBmpHelp);
	SAFE_DEL_BMP(gBmpSettings);
	SAFE_DEL_BMP(gBmpExit);
	SAFE_DEL_BMP(gBmpMore);
	SAFE_DEL_BMP(gBmpEdit);
	SAFE_DEL_BMP(gBmpFolder);
	SAFE_DEL_BMP(gBmpGithub);
	SAFE_DEL_BMP(gBmpRestart);
	SAFE_DEL_BMP(gBmpCheckUpdate);

	if (g_hMoreSub) {
		DestroyMenu(g_hMoreSub);
		g_hMoreSub = nullptr;
	}
	if (g_hTrayMenu) {
		DestroyMenu(g_hTrayMenu);
		g_hTrayMenu = nullptr;
	}
}

static void ShowTrayIcon() {
	g_nid.hWnd = g_mainHwnd;
	const HICON hTrayIcon = (HICON)LoadImageW(
		g_hInst,
		MAKEINTRESOURCEW(IDI_CANDYLAUNCHER),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR
	);

	g_nid.hIcon = hTrayIcon;

	if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid)) {
		// 修改失败，说明图标不存在，执行添加
		Shell_NotifyIconW(NIM_ADD, &g_nid);
	}
}

static void HideTrayIcon() {
	Shell_NotifyIconW(NIM_DELETE, &g_nid);
}
