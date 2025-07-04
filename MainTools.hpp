#pragma once
#include <windows.h>
#include <string>
#include <debugapi.h>
#include <sstream>
#include <ShlObj.h>
#include <thread>
#include <commctrl.h>
#include <vector>
#include <memory>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <comdef.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>

#include "Constants.hpp"

// class MainTools
// {
// public:
// 全局或静态变量
// MainTools()= delete;
// ~MainTools() = delete;

class RunCommandAction;


static void HideWindow(HWND hWnd, HWND hEdit, HWND hListView)
{
	// 不需要设置什么线程延时去关闭什么的，归根结底是alt键触发了控件的一些东西，屏蔽就好
	SetWindowText(hEdit, L"");
	// 强制更新 UI
	UpdateWindow(hEdit);
	UpdateWindow(hListView);
	ShowWindow(hWnd, SW_HIDE);

	//ListView_DeleteAllItems(hListView);
	//UpdateWindow(hListView);
	// SendMessage(hEdit, EM_SETSEL, 0, -1); // Ctrl+A: 全选
	//SendMessage(hWnd, WM_WINDOWS_HIDE, 0, 0); // Ctrl+A: 全选
	// SetTimer(hWnd, 1001, 50, NULL);
}

static void RestoreWindowIfMinimized(HWND hWnd)
{
	if (IsIconic(hWnd))
	{
		ShowWindow(hWnd, SW_RESTORE);
	}
}

static int lastWindowCenterX = -1;
static int lastWindowCenterY = -1;

static void MyMoveWindow(HWND hWnd)
{
	// 获取鼠标位置
	POINT pt;
	GetCursorPos(&pt);

	// 获取鼠标所在的显示器
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	// 获取该显示器的信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	// 计算中心位置
	const int windowWidth = 620;
	const int windowHeight = 480;

	const int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - windowWidth) / 2;
	const int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - windowHeight) / 2;
	// Move the window to the center and make it topmost.
	// SWP_NOSIZE preserves the window size and SWP_SHOWWINDOW ensures it is visible.
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY)
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
		// ShowWindow(hWnd, SW_SHOW);
	}
	else
	{
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void ShowMainWindowSimple(HWND hWnd, HWND hEdit)
{
	SetFocus(hEdit);
	RestoreWindowIfMinimized(hWnd);
	SetForegroundWindow(hWnd);
	MyMoveWindow(hWnd);
}

// static const std::shared_ptr<RunCommandAction>& GetListViewSelectedAction(HWND hListView,std::vector<std::shared_ptr<RunCommandAction>>& filteredActions)
// {
// 	const int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
// 	return filteredActions[selected];
// }

static const std::shared_ptr<RunCommandAction>& GetListViewSelectedAction(
	HWND hListView, std::vector<std::shared_ptr<RunCommandAction>>& filteredActions)
{
	const int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
	if (selected == -1 || selected < 0 || static_cast<size_t>(selected) >= filteredActions.size())
	{
		// 这里你需要返回一个合法的引用，但当前没选中或者越界了
		// 可以考虑返回一个静态的空指针，避免崩溃
		static std::shared_ptr<RunCommandAction> emptyPtr = nullptr;
		return emptyPtr;
	}
	return filteredActions[selected];
}


static void CustomRegisterHotKey(HWND hWnd)
{
	if (!RegisterHotKey(hWnd, 1, MOD_ALT, 0x4B))
	{
		const DWORD error = GetLastError();
		if (error == ERROR_HOTKEY_ALREADY_REGISTERED)
		{
			MessageBox(nullptr, L"热键 Alt+K 已被占用", L"错误", MB_OK | MB_ICONERROR);
		}
		else
		{
			std::wstringstream ss;
			ss << L"热键注册失败，错误代码：" << error;
			MessageBox(nullptr, ss.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
		}
	}
}

// 使listview监听esc键
static LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam,
											UINT_PTR, const DWORD_PTR dwRefData)
{
	if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
	{
		HWND hEdit = reinterpret_cast<HWND>(dwRefData);
		HideWindow(GetParent(hWnd), hEdit, hWnd);
		return 0;
	}
	return DefSubclassProc(hWnd, message, wParam, lParam);
}

static void ReleaseAltKey()
{
	keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
}

static std::wstring GetExecutableFolder()
{
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH)
	{
		// 错误处理（可选）
		return L"";
	}

	// 找到最后一个反斜杠（目录分隔符）
	const std::wstring fullPath(path);
	const size_t pos = fullPath.find_last_of(L"\\/");
	if (pos != std::wstring::npos)
	{
		return fullPath.substr(0, pos);
	}

	return L"";
}

// static std::wstring MyToLower2(const std::wstring& input)
// 	{
// 		std::wstring output;
// 		output.reserve(input.size());
// 		for (const wchar_t ch : input)
// 		{
// 			output.push_back(towlower(ch));
// 		}
// 		return output;
// 	}

static bool IsChineseChar2(const std::wstring& str)
{
	return str[0] >= 0x4E00 && str[0] <= 0x9FFF;
}

static bool IsChineseChar(const wchar_t ch)
{
	return ch >= 0x4E00 && ch <= 0x9FFF;
}


// };
struct ComInitGuard
{
	ComInitGuard() { hr = CoInitialize(nullptr); }
	~ComInitGuard() { if (SUCCEEDED(hr)) CoUninitialize(); }
	HRESULT hr;
};

static void ShowShellContextMenu(HWND hwnd, const std::wstring& filePath, const POINT& ptScreen)
{
	ComInitGuard guard;
	if (FAILED(guard.hr)) return;

	PIDLIST_ABSOLUTE pidl = nullptr;
	SFGAOF sfgao;
	guard.hr = SHParseDisplayName(filePath.c_str(), nullptr, &pidl, 0, &sfgao);
	if (FAILED(guard.hr)) return;

	IShellFolder* desktopFolder = nullptr;
	guard.hr = SHGetDesktopFolder(&desktopFolder);
	if (FAILED(guard.hr))
	{
		CoTaskMemFree(pidl);
		return;
	}

	PIDLIST_ABSOLUTE pidlParent = ILClone(pidl);
	if (!pidlParent)
	{
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		return;
	}

	ILRemoveLastID(pidlParent);

	IShellFolder* parentFolder = nullptr;
	guard.hr = SHBindToObject(
		nullptr,
		pidlParent,
		nullptr,
		IID_IShellFolder,
		(void**)&parentFolder
	);
	if (FAILED(guard.hr))
	{
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	PCUITEMID_CHILD relpidl = ILFindLastID(pidl);

	IContextMenu* contextMenu = nullptr;
	guard.hr = parentFolder->GetUIObjectOf(hwnd, 1, &relpidl, IID_IContextMenu, nullptr, (void**)&contextMenu);
	if (SUCCEEDED(guard.hr))
	{
		IContextMenu2* contextMenu2 = nullptr;
		if (SUCCEEDED(contextMenu->QueryInterface(IID_IContextMenu2, (void**)&contextMenu2)))
		{
			contextMenu2->Release(); // 这步是必要的
		}

		IContextMenu3* contextMenu3 = nullptr;
		if (SUCCEEDED(contextMenu->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
		{
			contextMenu3->Release(); // 这步是必要的
		}
	}

	if (FAILED(guard.hr))
	{
		parentFolder->Release();
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	HMENU hMenu = CreatePopupMenu();
	if (hMenu)
	{
		UINT idCmdFirst = 1;
		UINT idCmdLast = 0x7FFF;

		contextMenu->QueryContextMenu(hMenu, 0, idCmdFirst, idCmdLast, CMF_NORMAL);

		int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
								ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

		if (cmd >= idCmdFirst && cmd <= idCmdLast)
		{
			CMINVOKECOMMANDINFOEX cmi = {0};
			cmi.cbSize = sizeof(cmi);
			cmi.fMask = CMIC_MASK_UNICODE;
			cmi.hwnd = hwnd;
			cmi.lpVerb = MAKEINTRESOURCEA(cmd - idCmdFirst);
			cmi.lpVerbW = MAKEINTRESOURCEW(cmd - idCmdFirst);

			cmi.nShow = SW_SHOWNORMAL;
			contextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
		}

		DestroyMenu(hMenu);
	}

	contextMenu->Release();
	parentFolder->Release();
	desktopFolder->Release();
	CoTaskMemFree(pidl);
	CoTaskMemFree(pidlParent);
}

static void ShowShellContextMenu2(HWND hwnd, const std::wstring& filePath, const POINT& ptScreen)
{
	ComInitGuard guard;
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) return;

	PIDLIST_ABSOLUTE pidl = nullptr;
	SFGAOF sfgao;
	hr = SHParseDisplayName(filePath.c_str(), nullptr, &pidl, 0, &sfgao);
	if (FAILED(hr)) return;

	IShellFolder* desktopFolder = nullptr;
	hr = SHGetDesktopFolder(&desktopFolder);
	if (FAILED(hr))
	{
		CoTaskMemFree(pidl);
		return;
	}

	PIDLIST_ABSOLUTE pidlParent = ILClone(pidl);
	if (!pidlParent)
	{
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		return;
	}

	ILRemoveLastID(pidlParent);

	IShellFolder* parentFolder = nullptr;
	hr = SHBindToObject(
		nullptr,
		pidlParent,
		nullptr,
		IID_IShellFolder,
		(void**)&parentFolder
	);
	if (FAILED(hr))
	{
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	PCUITEMID_CHILD relpidl = ILFindLastID(pidl);

	IContextMenu* contextMenu = nullptr;
	hr = parentFolder->GetUIObjectOf(hwnd, 1, &relpidl, IID_IContextMenu, nullptr, (void**)&contextMenu);
	if (FAILED(hr))
	{
		parentFolder->Release();
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	HMENU hMenu = CreatePopupMenu();
	if (hMenu)
	{
		contextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);

		int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
								ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

		if (cmd >= 1 && cmd <= 0x7FFF)
		{
			CMINVOKECOMMANDINFOEX cmi = {0};
			cmi.cbSize = sizeof(cmi);
			cmi.fMask = CMIC_MASK_UNICODE;
			cmi.hwnd = hwnd;
			cmi.lpVerb = MAKEINTRESOURCEA(cmd - 1);
			cmi.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
			cmi.nShow = SW_SHOWNORMAL;
			contextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
		}

		DestroyMenu(hMenu);
	}

	contextMenu->Release();
	parentFolder->Release();
	desktopFolder->Release();
	CoTaskMemFree(pidl);
	CoTaskMemFree(pidlParent);

	// 根据你程序结构决定是否CoUninitialize
	// CoUninitialize();
}


static int GetLabelHeight(HWND hwnd, std::wstring text, int maxWidth,HFONT hFontD)
{
	// 1. 创建 RECT 结构体，设置最大宽度，高度初始为0
	HDC hdc = GetDC(hwnd);
	RECT rc = {0, 0, maxWidth, 0};

	// 2. 选择字体到 DC
	HFONT hOldFont = (HFONT)SelectObject(hdc, hFontD);

	// 3. 使用 DrawText 测量文本所需高度
	DrawText(hdc, text.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK);

	// 4. 恢复旧字体
	SelectObject(hdc, hOldFont);
	int height = rc.bottom - rc.top;
	ReleaseDC(hwnd, hdc);

	// 5. 返回计算后的高度
	return height;
}

static void SetControlFont(HWND hCtrl, HFONT hFont)
{
	SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
}


