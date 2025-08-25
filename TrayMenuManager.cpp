#include "TrayMenuManager.h"
#include "Resource.h"
#include "MainTools.hpp"
#include <shellapi.h> // 需要包含 Shell API 头文件以使用 ExtractIcon

#include "SettingWindow.hpp"

HINSTANCE hInstance2;

HMENU           g_hTrayMenu = nullptr;
HMENU           g_hMoreSub  = nullptr;
NOTIFYICONDATAW g_nid = [] {
	NOTIFYICONDATAW nid{};
	nid.cbSize           = sizeof(nid);
	nid.hWnd             = nullptr; // 先留空，稍后覆盖
	nid.uID              = TRAY_ICON_ID;
	nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon            = LoadIcon(nullptr, IDI_APPLICATION);
	lstrcpynW(nid.szTip, L"CandyLauncher", ARRAYSIZE(nid.szTip));
	return nid;
}();


// 位图（用于 SetMenuItemBitmaps）
HBITMAP gBmpShow = nullptr, gBmpHelp = nullptr, gBmpSettings = nullptr, gBmpExit = nullptr;
HBITMAP gBmpMore = nullptr, gBmpEdit = nullptr, gBmpFolder = nullptr, gBmpGithub = nullptr, gBmpRestart = nullptr;
HBITMAP gBmpAbout = nullptr;

// ---- 构建托盘菜单（用 SetMenuItemBitmaps 绑定位图）----
void TrayMenuManager::Init(HWND parent, HINSTANCE hInstance)
{
	// 先准备所有位图（从 shell32.dll 取图标，再转位图）
	HICON iShow    = LoadShellIcon(281);
	HICON iHelp    = LoadShellIcon(24);
	HICON iSettings= LoadShellIcon(151);
	HICON iExit    = LoadShellIcon(240);
	HICON iMore    = nullptr;
	HICON iEdit    = LoadShellIcon(152);
	HICON iFolder  = LoadShellIcon(4);
	HICON iGithub  = LoadShellIcon(244);
	HICON iRestart = LoadShellIcon(47);
	HICON iAbout   = LoadShellIcon(161);

	gBmpShow     = IconToBitmap(iShow,    16,16);
	gBmpHelp     = IconToBitmap(iHelp,    16,16);
	gBmpSettings = IconToBitmap(iSettings,16,16);
	gBmpExit     = IconToBitmap(iExit,    16,16);
	gBmpMore     = IconToBitmap(iMore,    16,16);
	gBmpEdit     = IconToBitmap(iEdit,    16,16);
	gBmpFolder   = IconToBitmap(iFolder,  16,16);
	gBmpGithub   = IconToBitmap(iGithub,  16,16);
	gBmpRestart  = IconToBitmap(iRestart, 16,16);
	gBmpAbout  = IconToBitmap(iAbout, 16,16);

	// 主菜单
	g_hTrayMenu = CreatePopupMenu();

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_SHOW_WINDOW, L"打开主窗口(&C)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_SHOW_WINDOW, MF_BYCOMMAND, gBmpShow, gBmpShow);

	AppendMenuW(g_hTrayMenu, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_HELP, L"帮助文档(&H)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_HELP, MF_BYCOMMAND, gBmpHelp, gBmpHelp);

	// 子菜单 “更多...”
	g_hMoreSub = CreatePopupMenu();
	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_EDIT_CONFIG, L"编辑配置文件（记事本）(&O)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_EDIT_CONFIG, MF_BYCOMMAND, gBmpEdit, gBmpEdit);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_OPEN_FOLDER, L"在资源管理器打开路径(&I)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_OPEN_FOLDER, MF_BYCOMMAND, gBmpFolder, gBmpFolder);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_GITHUB, L"打开 Github 主页");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_GITHUB, MF_BYCOMMAND, gBmpGithub, gBmpGithub);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_ABOUT, L"关于");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_ABOUT, MF_BYCOMMAND, gBmpAbout, gBmpAbout);

	AppendMenuW(g_hMoreSub, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(g_hMoreSub, MF_STRING, TRAY_MENU_ID_RESTART, L"重启软件(&R)");
	SetMenuItemBitmaps(g_hMoreSub, TRAY_MENU_ID_RESTART, MF_BYCOMMAND, gBmpRestart, gBmpRestart);

	// 把子菜单加到主菜单，并给“更多...”本身设位图（需要按位置设置）
	int posMore = GetMenuItemCount(g_hTrayMenu);
	AppendMenuW(g_hTrayMenu, MF_STRING | MF_POPUP, (UINT_PTR)g_hMoreSub, L"更多...(&M)");
	SetMenuItemBitmaps(g_hTrayMenu, posMore, MF_BYPOSITION, gBmpMore, gBmpMore);

	// 设置 & 退出
	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_SETTINGS, L"设置(&S)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_SETTINGS, MF_BYCOMMAND, gBmpSettings, gBmpSettings);

	AppendMenuW(g_hTrayMenu, MF_STRING, TRAY_MENU_ID_EXIT, L"退出(&E)");
	SetMenuItemBitmaps(g_hTrayMenu, TRAY_MENU_ID_EXIT, MF_BYCOMMAND, gBmpExit, gBmpExit);
}


void TrayMenuManager::TrayMenuClick(const int position, HWND hWnd, HWND hEdit)
{
	switch (position)
	{
	case TRAY_MENU_ID_ABOUT: // 
		{
			ShowSettingsWindow(hInstance2, nullptr);
			RestoreWindowIfMinimized(g_settingsHwnd);
			SetForegroundWindow(g_settingsHwnd);
			SwitchToTab(L"about");
			
		}
		break;
	case TRAY_MENU_ID_SHOW_WINDOW: // 打开主窗口
		{
			ShowMainWindowSimple(hWnd, hEdit);
		}
		break;
	case TRAY_MENU_ID_OPEN_FOLDER: // 打开程序目录
		{
			wchar_t path[MAX_PATH];
			GetModuleFileName(nullptr, path, MAX_PATH);
			PathRemoveFileSpec(path);
			ShellExecute(nullptr, L"open", path, nullptr, nullptr, SW_SHOW);
		}
		break;
	case TRAY_MENU_ID_EDIT_CONFIG: // 编辑 JSON 配置文件
		{
			wchar_t path[MAX_PATH];
			GetModuleFileName(nullptr, path, MAX_PATH);
			PathRemoveFileSpec(path);
			wcscat_s(path, L"\\runner.json");
			ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
		}
		break;
	case TRAY_MENU_ID_SETTINGS: // 前往设置
		{
			ShowSettingsWindow(hInstance2, nullptr);
		}
		break;
	case TRAY_MENU_ID_HELP: // 打开 Github wiki 页面
		ShellExecute(nullptr, L"open", L"https://github.com/CandyTek/CandyLauncher/wiki", nullptr, nullptr, SW_SHOW);
		break;
	case TRAY_MENU_ID_GITHUB: // 打开 Github 页面
		ShellExecute(nullptr, L"open", L"https://github.com/CandyTek/CandyLauncher", nullptr, nullptr, SW_SHOW);
		break;
	case TRAY_MENU_ID_RESTART: // 重启
		{
			wchar_t path[MAX_PATH];
			GetModuleFileName(nullptr, path, MAX_PATH);
			ShellExecute(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
			// 显式销毁窗口 → 会自动触发 WM_DESTROY，里面会退出程序
			DestroyWindow(hWnd);
		}
		break;
	case TRAY_MENU_ID_EXIT: // 退出
		DestroyWindow(hWnd);
		break;
	default: ;
	}
}

void TrayMenuManager::TrayMenuShow(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt); // 获取鼠标位置
	SetForegroundWindow(hWnd); // 让菜单不会点击后立即消失
	TrackPopupMenu(g_hTrayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
					pt.x, pt.y, 0, hWnd, nullptr);
}

void TrayMenuManager::TrayMenuDestroy()
{
	Shell_NotifyIcon(NIM_DELETE, &g_nid);

	// 位图需要销毁（icons 使用了 LR_SHARED 不需要 DestroyIcon）
	auto SAFE_DEL_BMP = [](HBITMAP& h){ if(h){ DeleteObject(h); h=nullptr; } };
	SAFE_DEL_BMP(gBmpShow);
	SAFE_DEL_BMP(gBmpHelp);
	SAFE_DEL_BMP(gBmpSettings);
	SAFE_DEL_BMP(gBmpExit);
	SAFE_DEL_BMP(gBmpMore);
	SAFE_DEL_BMP(gBmpEdit);
	SAFE_DEL_BMP(gBmpFolder);
	SAFE_DEL_BMP(gBmpGithub);
	SAFE_DEL_BMP(gBmpRestart);

	if (g_hMoreSub) { DestroyMenu(g_hMoreSub); g_hMoreSub = nullptr; }
	if (g_hTrayMenu){ DestroyMenu(g_hTrayMenu); g_hTrayMenu = nullptr; }
	
}

void TrayMenuManager::ShowTrayIcon()
{
	
	g_nid.hWnd = g_mainHwnd;
	
	if (!Shell_NotifyIconW(NIM_MODIFY, &g_nid))
	{
		// 修改失败，说明图标不存在，执行添加
		Shell_NotifyIconW(NIM_ADD, &g_nid);
	}
}

void TrayMenuManager::HideTrayIcon()
{
	Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

