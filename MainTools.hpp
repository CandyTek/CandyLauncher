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
#include <shobjidl.h>
#include <shellapi.h>
#include <comdef.h>
#include <fstream>
#include <iostream>
#include <set>

#include "Constants.hpp"
#include "SettingsHelper.hpp"

class RunCommandAction;

struct MonitorData
{
	HMONITOR hMonitor;
	MONITORINFOEX mi;
	bool isPrimary;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	std::vector<MonitorData>* monitors = reinterpret_cast<std::vector<MonitorData>*>(dwData);

	MONITORINFOEX mi{};
	mi.cbSize = sizeof(mi);

	if (GetMonitorInfo(hMonitor, &mi))
	{
		bool isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY);
		monitors->push_back({hMonitor, mi, isPrimary});
	}

	return TRUE;
}

static void HideWindow(HWND hWnd, HWND hEdit, HWND hListView)
{
	// 不需要设置什么线程延时去关闭什么的，归根结底是alt键触发了控件的一些东西，屏蔽就好
	if (!pref_preserve_last_search_term)
	{
		SetWindowText(hEdit, L"");
		// 强制更新 UI
		UpdateWindow(hEdit);
		UpdateWindow(hListView);
	}
	else if (pref_last_search_term_selected)
	{
		PostMessageW(hEdit, EM_SETSEL, 0, -1);
	}
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

static void showMainWindowInCursorScreen(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);

	// 获取鼠标所在的显示器
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	// 获取该显示器的信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	// 计算位置
	const int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x;
	const int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y;
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY)
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}
	else
	{
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void showMainWindowInIndexScreen(HWND hWnd, int index)
{
	// 枚举所有显示器
	std::vector<MonitorData> monitors;
	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

	MONITORINFOEX mi;
	if (monitors.size() == 0)
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}

	if (monitors.size() > index)
	{
		mi = monitors[index].mi;
	}
	else
	{
		mi = monitors[0].mi;
	}
	// 计算位置
	const int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x;
	const int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y;
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY)
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}
	else
	{
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void showMainWindowInForegroundRect(HWND hWnd)
{
	// todo: 对常见的系统ui class进行判断，排除
	HWND foregroundWindow = GetForegroundWindow();
	if (foregroundWindow == nullptr)
	{
		showMainWindowInCursorScreen(hWnd);
		return;
	}
	// 获取窗口的矩形（包括标题栏和边框）
	RECT rect;
	if (GetWindowRect(foregroundWindow, &rect))
	{
		int x = rect.left;
		int y = rect.top;
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;

		std::cout << "前台窗口位置: (" << x << ", " << y << ")" << std::endl;
		std::cout << "前台窗口大小: " << width << " x " << height << std::endl;

		const int centerX = x + (width - MAIN_WINDOW_WIDTH) * window_position_offset_x;
		const int centerY = y + (height - MAIN_WINDOW_HEIGHT) * window_position_offset_y;
		if (centerX == lastWindowCenterX && centerY == lastWindowCenterY)
		{
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
						SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
		}
		else
		{
			SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
						SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
			lastWindowCenterX = centerX;
			lastWindowCenterY = centerY;
		}
	}
	else
	{
		std::cout << "获取窗口矩形失败。" << std::endl;
		showMainWindowInCursorScreen(hWnd);
		return;
	}
}

static void showMainWindowInForegroundAppScreen2(HWND hWnd)
{
	// 获取前台窗口
	HWND hForeground = GetForegroundWindow();
	if (!hForeground || hForeground == hWnd)
	{
		// 如果没有前台窗口，或就是当前窗口，降级为鼠标所在屏幕
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 获取前台窗口矩形
	RECT fgRect;
	if (!GetWindowRect(hForeground, &fgRect))
	{
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 枚举所有显示器并找出覆盖面积最大的
	struct MonitorData
	{
		HMONITOR bestMonitor = nullptr;
		int maxArea = 0;
		RECT fgRect;
	} data = {nullptr, 0, fgRect};

	auto MonitorEnumProc = [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL
	{
		MonitorData* pData = reinterpret_cast<MonitorData*>(lParam);

		MONITORINFO mi = {};
		mi.cbSize = sizeof(mi);
		if (!GetMonitorInfo(hMonitor, &mi)) return TRUE;

		// 计算前台窗口与此显示器工作区的交集
		RECT intersection;
		if (IntersectRect(&intersection, &mi.rcWork, &pData->fgRect))
		{
			int area = (intersection.right - intersection.left) * (intersection.bottom - intersection.top);
			if (area > pData->maxArea)
			{
				pData->maxArea = area;
				pData->bestMonitor = hMonitor;
			}
		}

		return TRUE; // 继续枚举
	};

	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&data));

	HMONITOR targetMonitor = data.bestMonitor;
	if (!targetMonitor)
	{
		// fallback
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 获取目标显示器信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfo(targetMonitor, &mi))
	{
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 计算位置
	const int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x;
	const int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y;

	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY)
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}
	else
	{
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void showMainWindowInForegroundAppScreen(HWND hWnd)
{
	// 获取前台窗口
	HWND hForeground = GetForegroundWindow();
	if (!hForeground || hForeground == hWnd)
	{
		// 如果没有前台窗口，或就是当前窗口，降级为鼠标所在屏幕
		showMainWindowInCursorScreen(hWnd);
		return;
	}
	char className[256];
	GetClassNameA(hForeground, className, sizeof(className));
	if (std::string(className) == "Progman")
	{
		showMainWindowInCursorScreen(hWnd);
		return;
	}


	// 获取窗口所在的显示器
	HMONITOR hMonitor = MonitorFromWindow(hForeground, MONITOR_DEFAULTTONEAREST);

	// 获取该显示器的信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	// 计算位置
	const int centerX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x;
	const int centerY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y;
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY)
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}
	else
	{
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void MyMoveWindow(HWND hWnd)
{
	std::string pref_window_popup_position = settingsMap["pref_window_popup_position"].defValue.get<std::string>();

	if (pref_window_popup_position == "currect_cursor_on_screen")
	{
		showMainWindowInCursorScreen(hWnd);
	}
	else if (pref_window_popup_position == "last_time_opened_pos")
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}
	else if (pref_window_popup_position == "currect_window_on_screen")
	{
		showMainWindowInForegroundAppScreen(hWnd);
	}
	else if (pref_window_popup_position == "within_focus_window")
	{
		showMainWindowInForegroundRect(hWnd);
	}
	else if (pref_window_popup_position == "screen_first")
	{
		showMainWindowInIndexScreen(hWnd, 0);
	}
	else if (pref_window_popup_position == "screen_second")
	{
		showMainWindowInIndexScreen(hWnd, 1);
	}
	else if (pref_window_popup_position == "screen_third")
	{
		showMainWindowInIndexScreen(hWnd, 2);
	}
}

static void ShowMainWindowSimple(HWND hWnd, HWND hEdit)
{
	RestoreWindowIfMinimized(hWnd);
	SetFocus(hEdit);
	MyMoveWindow(hWnd);
	SetForegroundWindow(hWnd);
	if (g_hklIme != NULL)
		PostMessageW(hEdit, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)g_hklIme);
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

// 使listview监听esc键
static LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam,
											UINT_PTR, const DWORD_PTR dwRefData)
{
	switch (message)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			HWND hEdit = reinterpret_cast<HWND>(dwRefData);
			HideWindow(GetParent(hWnd), hEdit, hWnd);
			return 0;
		}
		break;
	case WM_MBUTTONUP:
		TimerIDSetFocusEdit = SetTimer(GetParent(hWnd), TIMER_SETFOCUS_EDIT, 10, nullptr); // 10 毫秒延迟
		break;

	default: break;
	}
	return DefSubclassProc(hWnd, message, wParam, lParam);
}

static void ReleaseAltKey()
{
	keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
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


static int GetLabelHeight(HWND hwnd, std::wstring text, int maxWidth, HFONT hFontD)
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
