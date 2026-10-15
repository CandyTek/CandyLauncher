#pragma once
#include <windows.h>
#include <WinUser.h>
#include <commctrl.h>
#include <memory>
#include <string>
#include <vector>
// 不能少
#include <shellapi.h>

#include "util/BitmapUtil.hpp"
#include "util/MainTools.hpp"
#include "common/Resource.h"
#include "common/Constants.hpp"
#include "common/I18n.hpp"

#define WM_TRAYICON (WM_USER + 1)

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
constexpr int TRAY_MENU_ID_GITHUB_ISSUES = 10013;
constexpr int TRAY_MENU_ID_PIN_THIS_TIME = 10014;
constexpr int TRAY_MENU_ID_ALWAYS_ON_TOP = 10015;
constexpr int TRAY_MENU_ID_LOCK_WINDOW_POSITION = 10016;
constexpr int TRAY_MENU_ID_CLOSE_AFTER_OPEN_ITEM = 10017;
// “失去焦点时”子菜单选项，按 entryValues 下标偏移
constexpr int TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE = 10020;
constexpr int TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_END = 10030;

constexpr int TRAY_MENU_ID_BASE_END = 10030;

struct MainWindowMenuOptions {
	bool pinned;
	bool showPinOption;
	bool alwaysOnTop;
	bool lockWindowPosition;
	bool closeAfterOpenItem;
	std::wstring closeOnDismissFocusTitle;
	std::vector<std::wstring> closeOnDismissFocusEntries;
	int closeOnDismissFocusIndex;
};

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

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_SHOW_WINDOW, LoadI18nString(IDS_I18N_TRAY_SHOW_WINDOW, L"&Show Main Window").c_str());
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_SHOW_WINDOW, MF_BYCOMMAND, gBmpShow, gBmpShow);

	AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_HELP, LoadI18nString(IDS_I18N_TRAY_HELP, L"&Help").c_str());
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_HELP, MF_BYCOMMAND, gBmpHelp, gBmpHelp);

	// 子菜单 “更多...”
	g_hMoreSub = CreatePopupMenu();

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_OPEN_FOLDER, LoadI18nString(IDS_I18N_TRAY_OPEN_FOLDER, L"Open &Install Folder").c_str());
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_OPEN_FOLDER, MF_BYCOMMAND, gBmpFolder, gBmpFolder);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_GITHUB, LoadI18nString(IDS_I18N_TRAY_GITHUB, L"Open &GitHub Page").c_str());
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_GITHUB, MF_BYCOMMAND, gBmpGithub, gBmpGithub);
	
	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_GITHUB_ISSUES, LoadI18nString(IDS_I18N_TRAY_FEEDBACK, L"&Feedback").c_str());

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_ABOUT, LoadI18nString(IDS_I18N_TRAY_ABOUT, L"&About").c_str());
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_ABOUT, MF_BYCOMMAND, gBmpAbout, gBmpAbout);

	AppendMenuW(g_hMoreSub, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_CHECK_UPDATE, LoadI18nString(IDS_I18N_TRAY_CHECK_UPDATE, L"&Check for Updates").c_str());
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_CHECK_UPDATE, MF_BYCOMMAND, gBmpCheckUpdate, gBmpCheckUpdate);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_RESTART, LoadI18nString(IDS_I18N_TRAY_RESTART, L"&Restart").c_str());
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_RESTART, MF_BYCOMMAND, gBmpRestart, gBmpRestart);

	// 把子菜单加到主菜单，并给“更多...”本身设位图（需要按位置设置）
	int posMore = GetMenuItemCount(g_hTrayMenu);
	AppendMenuW(g_hTrayMenu, MF_STRING | MF_POPUP, (UINT_PTR)g_hMoreSub, LoadI18nString(IDS_I18N_TRAY_MORE, L"&More...").c_str());
	SetMenuItemBitmaps(g_hTrayMenu, posMore, MF_BYPOSITION, gBmpMore, gBmpMore);
	AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, nullptr);

	// 设置 & 退出
	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_SETTINGS, LoadI18nString(IDS_I18N_TRAY_SETTINGS, L"&Settings").c_str());
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_SETTINGS, MF_BYCOMMAND, gBmpSettings, gBmpSettings);
#if defined(_DEBUG) || !defined(NDEBUG) || defined(REL_WITH_DEB_INFO_DEBUG)
	AppendMenuW(g_hTrayMenu, MF_STRING, 10999, L"当前为DEBUG版本");
#endif

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_EXIT, LoadI18nString(IDS_I18N_TRAY_EXIT, L"E&xit").c_str());
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_EXIT, MF_BYCOMMAND, gBmpExit, gBmpExit);
}

static void TrayMenuShow(HWND hWnd, const MainWindowMenuOptions* options = nullptr) {
	POINT pt;
	GetCursorPos(&pt); // 获取鼠标位置
	SetForegroundWindow(hWnd); // 让菜单不会点击后立即消失

	// 第一项根据主窗口当前可见状态切换文字
	std::wstring showText = (g_mainHwnd && IsWindowVisible(g_mainHwnd))
		? LoadI18nString(IDS_I18N_TRAY_HIDE_WINDOW, L"&Hide Main Window")
		: LoadI18nString(IDS_I18N_TRAY_SHOW_WINDOW, L"&Show Main Window");
	MENUITEMINFOW mii{};
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = showText.data();
	SetMenuItemInfoW(g_hTrayMenu, TRAY_MENU_ID_SHOW_WINDOW, FALSE, &mii);

	HMENU hDismissSub = nullptr;
	if (options) {
		UINT pos = 1;
		if (options->showPinOption) {
			InsertMenuW(g_hTrayMenu, pos++, MF_BYPOSITION | MF_STRING | (options->pinned ? MF_CHECKED : MF_UNCHECKED),
				TRAY_MENU_ID_PIN_THIS_TIME, LoadI18nString(IDS_I18N_TRAY_PIN_THIS_TIME, L"Pin This Time").c_str());
		}
		InsertMenuW(g_hTrayMenu, pos++, MF_BYPOSITION | MF_STRING | (options->alwaysOnTop ? MF_CHECKED : MF_UNCHECKED),
			TRAY_MENU_ID_ALWAYS_ON_TOP, LoadI18nString(IDS_I18N_TRAY_ALWAYS_ON_TOP, L"Keep Main Window on Top").c_str());
		InsertMenuW(g_hTrayMenu, pos++, MF_BYPOSITION | MF_STRING | (options->lockWindowPosition ? MF_CHECKED : MF_UNCHECKED),
			TRAY_MENU_ID_LOCK_WINDOW_POSITION, LoadI18nString(IDS_I18N_TRAY_LOCK_WINDOW_POSITION, L"Lock Window Position").c_str());
		InsertMenuW(g_hTrayMenu, pos++, MF_BYPOSITION | MF_STRING | (options->closeAfterOpenItem ? MF_CHECKED : MF_UNCHECKED),
			TRAY_MENU_ID_CLOSE_AFTER_OPEN_ITEM, LoadI18nString(IDS_I18N_TRAY_CLOSE_AFTER_OPEN_ITEM, L"Close After Opening an Item").c_str());

		const int entryCount = static_cast<int>(options->closeOnDismissFocusEntries.size());
		if (entryCount > 0) {
			hDismissSub = CreatePopupMenu();
			for (int i = 0; i < entryCount && TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE + i < TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_END; ++i) {
				AppendMenuW(hDismissSub, MF_STRING, TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE + i,
					options->closeOnDismissFocusEntries[i].c_str());
			}
			if (options->closeOnDismissFocusIndex >= 0) {
				CheckMenuRadioItem(hDismissSub,
					TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE,
					TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE + entryCount - 1,
					TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE + options->closeOnDismissFocusIndex,
					MF_BYCOMMAND);
			}
			InsertMenuW(g_hTrayMenu, pos++, MF_BYPOSITION | MF_STRING | MF_POPUP,
				reinterpret_cast<UINT_PTR>(hDismissSub), options->closeOnDismissFocusTitle.c_str());
		}
	}
	TrackPopupMenu(g_hTrayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
					pt.x, pt.y, 0, hWnd, nullptr);
	if (options) {
		DeleteMenu(g_hTrayMenu, TRAY_MENU_ID_PIN_THIS_TIME, MF_BYCOMMAND);
		DeleteMenu(g_hTrayMenu, TRAY_MENU_ID_ALWAYS_ON_TOP, MF_BYCOMMAND);
		DeleteMenu(g_hTrayMenu, TRAY_MENU_ID_LOCK_WINDOW_POSITION, MF_BYCOMMAND);
		DeleteMenu(g_hTrayMenu, TRAY_MENU_ID_CLOSE_AFTER_OPEN_ITEM, MF_BYCOMMAND);
		if (hDismissSub) {
			// 先移除再销毁，避免 g_hTrayMenu 中残留悬空的子菜单句柄
			const int count = GetMenuItemCount(g_hTrayMenu);
			for (int i = 0; i < count; ++i) {
				if (GetSubMenu(g_hTrayMenu, i) == hDismissSub) {
					RemoveMenu(g_hTrayMenu, i, MF_BYPOSITION);
					break;
				}
			}
			DestroyMenu(hDismissSub);
		}
	}
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
