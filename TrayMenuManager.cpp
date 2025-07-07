#include "TrayMenuManager.h"
#include <WinUser.h>
#include "Resource.h"
#include "MainTools.hpp"
#include <Shlwapi.h>

#include "SettingWindow.hpp"

TrayMenuManager::TrayMenuManager() = default;

HINSTANCE hInstance2;

void TrayMenuManager::Init(HWND parent, HINSTANCE hInstance)
{
	// 初始化托盘图标
	ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = parent;
	nid.uID = 1001;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
	hInstance2  = hInstance;
	wcscpy_s(nid.szTip, L"My App");
	Shell_NotifyIcon(NIM_ADD, &nid);

	// 创建托盘菜单
	hTrayMenu = CreatePopupMenu();
	AppendMenu(hTrayMenu, MF_STRING, 10001, L"打开主窗口");
	AppendMenu(hTrayMenu, MF_STRING, 10002, L"在资源管理器打开路径");
	AppendMenu(hTrayMenu, MF_STRING, 10003, L"编辑配置文件（记事本）");
	AppendMenu(hTrayMenu, MF_STRING, 10010, L"设置");
	AppendMenu(hTrayMenu, MF_SEPARATOR, 0, nullptr); // 插入分隔线
	AppendMenu(hTrayMenu, MF_STRING, 10004, L"打开 Github 主页");
	AppendMenu(hTrayMenu, MF_SEPARATOR, 0, nullptr); // 插入分隔线
	AppendMenu(hTrayMenu, MF_STRING, 10005, L"重启");
	AppendMenu(hTrayMenu, MF_STRING, 10006, L"退出");
}

void TrayMenuManager::TrayMenuClick(const int position, HWND hWnd, HWND hEdit)
{
	switch (position)
	{
	case 10001: // 打开主窗口
		{
			ShowMainWindowSimple(hWnd, hEdit);
		}
		break;
	case 10002: // 打开程序目录
		{
			wchar_t path[MAX_PATH];
			GetModuleFileName(nullptr, path, MAX_PATH);
			PathRemoveFileSpec(path);
			ShellExecute(nullptr, L"open", path, nullptr, nullptr, SW_SHOW);
		}
		break;
	case 10003: // 编辑 JSON 配置文件
		{
			wchar_t path[MAX_PATH];
			GetModuleFileName(nullptr, path, MAX_PATH);
			PathRemoveFileSpec(path);
			wcscat_s(path, L"\\runner.json");
			ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
		}
		break;
	case 10010: // 前往设置
		{
			ShowSettingsWindow(hInstance2, nullptr);
		}
		break;
	case 10004: // 打开 Github 页面
		ShellExecute(nullptr, L"open", L"https://github.com/YourRepoHere", nullptr, nullptr, SW_SHOW);
		break;
	case 10005: // 重启
		{
			wchar_t path[MAX_PATH];
			GetModuleFileName(nullptr, path, MAX_PATH);
			ShellExecute(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
			// 显式销毁窗口 → 会自动触发 WM_DESTROY，里面会退出程序
			DestroyWindow(hWnd);
		}
		break;
	case 10006: // 退出
		DestroyWindow(hWnd);
		break;
	default: ;
	}
}

void TrayMenuManager::TrayMenuShow(HWND hWnd) const
{
	POINT pt;
	GetCursorPos(&pt); // 获取鼠标位置
	SetForegroundWindow(hWnd); // 让菜单不会点击后立即消失
	TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
					pt.x, pt.y, 0, hWnd, nullptr);
}

void TrayMenuManager::TrayMenuDestroy()
{
	Shell_NotifyIcon(NIM_DELETE, &nid);
	if (hTrayMenu != nullptr)
	{
		DestroyMenu(hTrayMenu);
	}
}

void TrayMenuManager::ShowTrayIcon()
{
	// 尝试修改现有图标
	if (!Shell_NotifyIcon(NIM_MODIFY, &nid))
	{
		// 修改失败，说明图标不存在，执行添加
		Shell_NotifyIcon(NIM_ADD, &nid);
	}
}

void TrayMenuManager::HideTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &nid);
}
