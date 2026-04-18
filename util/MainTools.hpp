// 此类放置程序所有通用方法，本hpp不要引用其他抽象文件，只引用通用工具

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
#include <stdexcept>
#include <array>
#include <dwmapi.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <io.h>
#include <fcntl.h>
#include <shlwapi.h>
#include <tlhelp32.h>

#include "BaseTools.hpp"
#include "ShortcutUtil.hpp"
#include "../common/GlobalState.hpp"
#include "../plugins/BaseAction.hpp"
#include "../model/TraverseOptions.hpp"
#include <propvarutil.h>

#include "ShortCutDetectUtil.hpp"

struct MonitorData {
	HMONITOR hMonitor;
	MONITORINFOEX mi;
	bool isPrimary;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	std::vector<MonitorData>* monitors = reinterpret_cast<std::vector<MonitorData>*>(dwData);

	MONITORINFOEX mi{};
	mi.cbSize = sizeof(mi);

	if (GetMonitorInfo(hMonitor, &mi)) {
		bool isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY);
		monitors->push_back({hMonitor, mi, isPrimary});
	}

	return TRUE;
}

static void HideWindow() {
	// 不需要设置什么线程延时去关闭什么的，归根结底是alt键触发了控件的一些东西，屏蔽就好
	if (!pref_preserve_last_search_term) {
		SetWindowText(g_editHwnd, L"");
		// 强制更新 UI
		UpdateWindow(g_editHwnd);
		UpdateWindow(g_listViewHwnd);
	} else if (pref_last_search_term_selected) {
		PostMessageW(g_editHwnd, EM_SETSEL, 0, -1);
	}
	RECT saveRect;
	if (!GetWindowRect(g_mainHwnd, &saveRect)) {
		lastWindowCenterX = -1;
		lastWindowCenterY = -1;
	} else {
		lastWindowCenterX = saveRect.left;
		lastWindowCenterY = saveRect.top;
	}

	ShowWindow(g_mainHwnd, SW_HIDE);

	//ListView_DeleteAllItems(hListView);
	//UpdateWindow(hListView);
	// SendMessage(hEdit, EM_SETSEL, 0, -1); // Ctrl+A: 全选
	//SendMessage(hWnd, WM_WINDOWS_HIDE, 0, 0); // Ctrl+A: 全选
	// SetTimer(hWnd, 1001, 50, NULL);
}

static void showMainWindowInCursorScreen(HWND hWnd) {
	POINT pt;
	GetCursorPos(&pt);

	// 获取鼠标所在的显示器
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	// 获取该显示器的信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	// 计算位置
	const int centerX = static_cast<int>(mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x);
	const int centerY = static_cast<int>(mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y);
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY) {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	} else {
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static RECT getWindowRectMainWindowInCursorScreen() {
	POINT pt;
	GetCursorPos(&pt);

	// 获取鼠标所在的显示器
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	// 获取该显示器的信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	// 计算位置
	const int centerX = static_cast<int>(mi.rcWork.left +
		(float)(mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x);
	const int centerY = static_cast<int>(mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y);
	RECT result;
	result.left = centerX;
	result.top = centerY;
	result.right = centerX + 1;
	result.bottom = centerY + 1;
	return result;
}

static void showMainWindowInIndexScreen(HWND hWnd, int index) {
	// 枚举所有显示器
	std::vector<MonitorData> monitors;
	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

	MONITORINFOEX mi;
	if (monitors.empty()) {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	}

	if (monitors.size() > static_cast<size_t>(index)) {
		mi = monitors[index].mi;
	} else {
		mi = monitors[0].mi;
	}
	// 计算位置
	const int centerX = static_cast<int>(mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x);
	const int centerY = static_cast<int>(mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y);
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY) {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	} else {
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void showMainWindowInForegroundRect(HWND hWnd) {
	// todo: 对常见的系统ui class进行判断，排除
	HWND foregroundWindow = GetForegroundWindow();
	if (foregroundWindow == nullptr) {
		showMainWindowInCursorScreen(hWnd);
		return;
	}
	// 获取窗口的矩形（包括标题栏和边框）
	RECT rect;
	if (GetWindowRect(foregroundWindow, &rect)) {
		int x = rect.left;
		int y = rect.top;
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;

		std::wcout << L"foregroundWindow location: (" << x << ", " << y << ")" << std::endl;
		std::wcout << L"foregroundWindow size: " << width << " x " << height << std::endl;

		const int centerX = static_cast<int>(x + (width - MAIN_WINDOW_WIDTH) * window_position_offset_x);
		const int centerY = static_cast<int>(y + (height - MAIN_WINDOW_HEIGHT) * window_position_offset_y);
		if (centerX == lastWindowCenterX && centerY == lastWindowCenterY) {
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
						SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
		} else {
			SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
						SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
			lastWindowCenterX = centerX;
			lastWindowCenterY = centerY;
		}
	} else {
		std::wcout << L"Failed to get window rectangle." << std::endl;
		showMainWindowInCursorScreen(hWnd);
		return;
	}
}

static void showMainWindowInForegroundAppScreen2(HWND hWnd) {
	// 获取前台窗口
	HWND hForeground = GetForegroundWindow();
	if (!hForeground || hForeground == hWnd) {
		// 如果没有前台窗口，或就是当前窗口，降级为鼠标所在屏幕
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 获取前台窗口矩形
	RECT fgRect;
	if (!GetWindowRect(hForeground, &fgRect)) {
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 枚举所有显示器并找出覆盖面积最大的
	struct MonitorData {
		HMONITOR bestMonitor = nullptr;
		int maxArea = 0;
		RECT fgRect;
	} data = {nullptr, 0, fgRect};

	auto MonitorEnumProc = [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
		MonitorData* pData = reinterpret_cast<MonitorData*>(lParam);

		MONITORINFO mi = {};
		mi.cbSize = sizeof(mi);
		if (!GetMonitorInfo(hMonitor, &mi)) return TRUE;

		// 计算前台窗口与此显示器工作区的交集
		RECT intersection;
		if (IntersectRect(&intersection, &mi.rcWork, &pData->fgRect)) {
			int area = (intersection.right - intersection.left) * (intersection.bottom - intersection.top);
			if (area > pData->maxArea) {
				pData->maxArea = area;
				pData->bestMonitor = hMonitor;
			}
		}

		return TRUE; // 继续枚举
	};

	EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&data));

	HMONITOR targetMonitor = data.bestMonitor;
	if (!targetMonitor) {
		// fallback
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 获取目标显示器信息
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfo(targetMonitor, &mi)) {
		showMainWindowInCursorScreen(hWnd);
		return;
	}

	// 计算位置
	const int centerX = static_cast<int>(mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x);
	const int centerY = static_cast<int>(mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y);

	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY) {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	} else {
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void showMainWindowInForegroundAppScreen(HWND hWnd) {
	// 获取前台窗口
	HWND hForeground = GetForegroundWindow();
	if (!hForeground || hForeground == hWnd) {
		// 如果没有前台窗口，或就是当前窗口，降级为鼠标所在屏幕
		showMainWindowInCursorScreen(hWnd);
		return;
	}
	char className[256];
	GetClassNameA(hForeground, className, sizeof(className));
	if (std::string(className) == "Progman") {
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
	const int centerX = static_cast<int>(mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
		window_position_offset_x);
	const int centerY = static_cast<int>(mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - MAIN_WINDOW_HEIGHT) *
		window_position_offset_y);
	if (centerX == lastWindowCenterX && centerY == lastWindowCenterY) {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	} else {
		SetWindowPos(hWnd, HWND_TOPMOST, centerX, centerY, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER);
		lastWindowCenterX = centerX;
		lastWindowCenterY = centerY;
	}
}

static void MyMoveWindow(HWND hWnd) {
	std::string pref_window_popup_position = g_settings_map["pref_window_popup_position"].stringValue;

	if (pref_window_popup_position == "currect_cursor_on_screen") {
		showMainWindowInCursorScreen(hWnd);
	} else if (pref_window_popup_position == "last_time_opened_pos") {
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
					SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOMOVE);
	} else if (pref_window_popup_position == "currect_window_on_screen") {
		showMainWindowInForegroundAppScreen(hWnd);
	} else if (pref_window_popup_position == "within_focus_window") {
		showMainWindowInForegroundRect(hWnd);
	} else if (pref_window_popup_position == "screen_first") {
		showMainWindowInIndexScreen(hWnd, 0);
	} else if (pref_window_popup_position == "screen_second") {
		showMainWindowInIndexScreen(hWnd, 1);
	} else if (pref_window_popup_position == "screen_third") {
		showMainWindowInIndexScreen(hWnd, 2);
	}
}

// static const std::shared_ptr<RunCommandAction>& GetListViewSelectedAction(HWND hListView,std::vector<std::shared_ptr<RunCommandAction>>& filteredActions)
// {
// 	const int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
// 	return filteredActions[selected];
// }

static const std::shared_ptr<BaseAction>& GetListViewSelectedAction(
	HWND hListView, std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
	if (selected == -1 || selected < 0 || static_cast<size_t>(selected) >= filteredActions.size()) {
		// 这里你需要返回一个合法的引用，但当前没选中或者越界了
		// 可以考虑返回一个静态的空指针，避免崩溃
		static std::shared_ptr<BaseAction> emptyPtr = nullptr;
		return emptyPtr;
	}
	return filteredActions[selected];
}

static void ReleaseAltKey() {
	keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

static void ShowShellContextMenu(HWND hwnd, const std::wstring& filePath, const POINT& ptScreen) {
	ConsolePrintln(L"ShellMenu", L"enter path=" + filePath +
		L", x=" + std::to_wstring(ptScreen.x) +
		L", y=" + std::to_wstring(ptScreen.y));
	ComInitGuard guard;
	if (FAILED(guard.hr)) {
		ConsolePrintln(L"ShellMenu", L"CoInitialize failed hr=" + std::to_wstring(guard.hr));
		return;
	}

	PIDLIST_ABSOLUTE pidl = nullptr;
	SFGAOF sfgao;
	guard.hr = SHParseDisplayName(filePath.c_str(), nullptr, &pidl, 0, &sfgao);
	if (FAILED(guard.hr)) {
		ConsolePrintln(L"ShellMenu", L"SHParseDisplayName failed hr=" + std::to_wstring(guard.hr));
		return;
	}

	IShellFolder* desktopFolder = nullptr;
	guard.hr = SHGetDesktopFolder(&desktopFolder);
	if (FAILED(guard.hr)) {
		ConsolePrintln(L"ShellMenu", L"SHGetDesktopFolder failed hr=" + std::to_wstring(guard.hr));
		CoTaskMemFree(pidl);
		return;
	}

	PIDLIST_ABSOLUTE pidlParent = ILClone(pidl);
	if (!pidlParent) {
		ConsolePrintln(L"ShellMenu", L"ILClone failed");
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
	if (FAILED(guard.hr)) {
		ConsolePrintln(L"ShellMenu", L"SHBindToObject failed hr=" + std::to_wstring(guard.hr));
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	PCUITEMID_CHILD relpidl = ILFindLastID(pidl);

	IContextMenu* contextMenu = nullptr;
	guard.hr = parentFolder->GetUIObjectOf(hwnd, 1, &relpidl, IID_IContextMenu, nullptr, (void**)&contextMenu);
	if (FAILED(guard.hr)) {
		ConsolePrintln(L"ShellMenu", L"GetUIObjectOf failed hr=" + std::to_wstring(guard.hr));
	}
	if (SUCCEEDED(guard.hr)) {
		IContextMenu2* contextMenu2 = nullptr;
		if (SUCCEEDED(contextMenu->QueryInterface(IID_IContextMenu2, (void **) &contextMenu2))) {
			contextMenu2->Release(); // 这步是必要的
		}

		IContextMenu3* contextMenu3 = nullptr;
		if (SUCCEEDED(contextMenu->QueryInterface(IID_IContextMenu3, (void **) &contextMenu3))) {
			contextMenu3->Release(); // 这步是必要的
		}
	}

	if (FAILED(guard.hr)) {
		parentFolder->Release();
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	HMENU hMenu = CreatePopupMenu();
	if (hMenu) {
		UINT idCmdFirst = 1;
		UINT idCmdLast = 0x7FFF;

		const HRESULT queryHr = contextMenu->QueryContextMenu(hMenu, 0, idCmdFirst, idCmdLast, CMF_NORMAL);
		ConsolePrintln(L"ShellMenu", L"QueryContextMenu hr=" + std::to_wstring(queryHr) +
			L", count=" + std::to_wstring(GetMenuItemCount(hMenu)));

		SetForegroundWindow(hwnd);
		int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
								ptScreen.x, ptScreen.y, 0, hwnd, nullptr);
		PostMessageW(hwnd, WM_NULL, 0, 0);
		ConsolePrintln(L"ShellMenu", L"TrackPopupMenu cmd=" + std::to_wstring(cmd));

		if (cmd >= static_cast<int>(idCmdFirst) && cmd <= static_cast<int>(idCmdLast)) {
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
	} else {
		ConsolePrintln(L"ShellMenu", L"CreatePopupMenu failed err=" + std::to_wstring(GetLastError()));
	}

	contextMenu->Release();
	parentFolder->Release();
	desktopFolder->Release();
	CoTaskMemFree(pidl);
	CoTaskMemFree(pidlParent);
}

static void ShowShellContextMenu2(HWND hwnd, const std::wstring& filePath, const POINT& ptScreen) {
	ComInitGuard guard;
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) return;

	PIDLIST_ABSOLUTE pidl = nullptr;
	SFGAOF sfgao;
	hr = SHParseDisplayName(filePath.c_str(), nullptr, &pidl, 0, &sfgao);
	if (FAILED(hr)) return;

	IShellFolder* desktopFolder = nullptr;
	hr = SHGetDesktopFolder(&desktopFolder);
	if (FAILED(hr)) {
		CoTaskMemFree(pidl);
		return;
	}

	PIDLIST_ABSOLUTE pidlParent = ILClone(pidl);
	if (!pidlParent) {
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
	if (FAILED(hr)) {
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	PCUITEMID_CHILD relpidl = ILFindLastID(pidl);

	IContextMenu* contextMenu = nullptr;
	hr = parentFolder->GetUIObjectOf(hwnd, 1, &relpidl, IID_IContextMenu, nullptr, (void**)&contextMenu);
	if (FAILED(hr)) {
		parentFolder->Release();
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	HMENU hMenu = CreatePopupMenu();
	if (hMenu) {
		contextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);

		int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
								ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

		if (cmd >= 1 && cmd <= 0x7FFF) {
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


static int GetLabelHeight(HWND hwnd, std::wstring text, int maxWidth, HFONT hFontD) {
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

inline void SetControlFont(HWND hCtrl, HFONT hFont) {
	SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
}

static int GetWindowVScrollBarThumbWidth(HWND hwnd, bool bAutoShow) {
	SCROLLBARINFO sb = {0};
	sb.cbSize = sizeof(SCROLLBARINFO);
	GetScrollBarInfo(hwnd, OBJID_VSCROLL, &sb);

	if (!bAutoShow) return sb.dxyLineButton;

	if (sb.dxyLineButton) return sb.dxyLineButton;

	::ShowScrollBar(hwnd, SB_VERT, TRUE);
	sb.cbSize = sizeof(SCROLLBARINFO);
	GetScrollBarInfo(hwnd, OBJID_VSCROLL, &sb);
	::ShowScrollBar(hwnd, SB_VERT, FALSE);
	return sb.dxyLineButton;
}

inline bool IsPointOnAnyMonitor(int lastWindowPositionX, int lastWindowPositionY) {
	const POINT pt = {lastWindowPositionX, lastWindowPositionY};
	return MonitorFromPoint(pt, MONITOR_DEFAULTTONULL) != nullptr;
}

inline bool IsRectOnAnyMonitor(const RECT& rc) {
	return MonitorFromRect(&rc, MONITOR_DEFAULTTONULL) != nullptr;
}


[[deprecated(L"会误伤一些快捷方式")]]
static bool IsShortcutInvalid2(const std::wstring& shortcutPath) {
	ComInitGuard guard;
	if (FAILED(guard.hr)) return true;

	IShellLink* pShellLink = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink,
								(void**)&pShellLink);
	if (FAILED(hr)) return true;

	IPersistFile* pPersistFile = nullptr;
	hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
	if (FAILED(hr)) {
		pShellLink->Release();
		return true;
	}

	hr = pPersistFile->Load(shortcutPath.c_str(), STGM_READ);
	if (FAILED(hr)) {
		pPersistFile->Release();
		pShellLink->Release();
		return true;
	}
	hr = pShellLink->Resolve(nullptr, SLR_NO_UI | SLR_NOSEARCH | SLR_NOTRACK);
	
	wchar_t targetPath[MAX_PATH] = {0};
	hr = pShellLink->GetPath(targetPath, MAX_PATH, nullptr, 0);

	pPersistFile->Release();
	pShellLink->Release();

	if (FAILED(hr)) return true;

	// Check if target file exists
	DWORD attributes = GetFileAttributes(targetPath);
	return (attributes == INVALID_FILE_ATTRIBUTES);
}

static TraverseOptions getTraverseOptions(const nlohmann::basic_json<>& cmd) {
	TraverseOptions traverseOptions;
	if (!cmd.is_object()) {
		std::wcerr << L"配置项不是一个对象，返回。" << std::endl;
		return traverseOptions;
	}

	if (cmd.contains("folder") && cmd["folder"].is_string()) {
		traverseOptions.folder = Utf8ToWString(cmd["folder"].get<std::string>());cmd.value("folder", cmd["folder"].get<std::string>());
	}
	if (cmd.contains("name") && cmd["name"].is_string()) {
		traverseOptions.name = Utf8ToWString(cmd["name"].get<std::string>());
	}
	if (cmd.contains("type") && cmd["type"].is_string()) {
		traverseOptions.type = Utf8ToWString(cmd["type"].get<std::string>());
	}
	if (cmd.contains("is_contain_subfolder") && cmd["is_contain_subfolder"].is_boolean()) {
		traverseOptions.recursive = cmd["is_contain_subfolder"].get<bool>();
	} else {
		traverseOptions.recursive = true;
	}
	if (cmd.contains("index_files_only") && cmd["index_files_only"].is_boolean()) {
		traverseOptions.indexFilesOnly = cmd["index_files_only"].get<bool>();
	} else {
		traverseOptions.indexFilesOnly = true;
	}


	if (cmd.contains("exts") && cmd["exts"].is_array()) {
		for (const auto& name : cmd["exts"]) {
			if (name.is_string()) {
				std::string ext = MyTrim(name.get<std::string>());
				if (!ext.empty() && ext[0] != '.') {
					ext.insert(ext.begin(), '.');
				}
				traverseOptions.extensions.push_back(Utf8ToWString(ext));
			}
		}
	}

	if (cmd.contains("excludes") && cmd["excludes"].is_array()) {
		for (const auto& name : cmd["excludes"]) {
			if (name.is_string()) {
				traverseOptions.excludeNames.push_back(Utf8ToWString(name.get<std::string>()));
			}
		}
	}

	if (cmd.contains("exclude_words") && cmd["exclude_words"].is_array()) {
		for (const auto& word : cmd["exclude_words"]) {
			if (word.is_string()) {
				traverseOptions.excludeWords.push_back(Utf8ToWString(MyToLower(word.get<std::string>())));
			}
		}
	}

	if (cmd.contains("rename_sources") && cmd["rename_sources"].is_array() &&
		cmd.contains("rename_targets") && cmd["rename_targets"].is_array()) {
		const auto& sources = cmd["rename_sources"];
		const auto& targets = cmd["rename_targets"];
		size_t count = (((sources.size()) < (targets.size())) ? (sources.size()) : (targets.size()));

		for (size_t i = 0; i < count; ++i) {
			if (sources[i].is_string() && targets[i].is_string()) {
				std::wstring src = Utf8ToWString(sources[i].get<std::string>());
				std::wstring dst = Utf8ToWString(targets[i].get<std::string>());
				traverseOptions.renameMap[src] = dst;
				traverseOptions.renameSources.push_back(src);
				traverseOptions.renameTargets.push_back(dst);
			}
		}
	}
	return traverseOptions;
}

inline bool OpenConsoleHere(const std::wstring& targetPath) {
	// 如果是文件，切到其目录；如果是目录，就用目录本身
	wchar_t dir[MAX_PATH];
	wcsncpy_s(dir, targetPath.c_str(), _TRUNCATE);
	DWORD attr = GetFileAttributesW(targetPath.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) return false;

	if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		PathRemoveFileSpecW(dir);
	}

	SHELLEXECUTEINFOW sei{sizeof(sei)};
	sei.lpFile = L"cmd.exe";
	sei.lpParameters = L"/K title Console & prompt $G";
	sei.lpDirectory = dir;
	sei.nShow = SW_SHOWNORMAL;
	return ShellExecuteExW(&sei) != FALSE;
}

// 从类似 "Ctrl+Alt+A(3)(65)" 字符串中提取并注册全局热键
static bool RegisterHotkeyFromString(HWND hWnd, const std::string& hotkeyStr, int hotkeyId) {
	UINT modifiers = 0;
	UINT vk = 0;

	// 查找括号中的 VK 值
	size_t posStart = hotkeyStr.rfind('(');
	size_t posEnd = hotkeyStr.rfind(')');

	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1) {
		return false;
	}

	std::string vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try {
		vk = std::stoi(vkStr);
	} catch (...) {
		return false;
	}

	// 查找 modifiers 值
	posEnd = posStart - 1;
	posStart = hotkeyStr.rfind('(', posEnd);
	if (posStart == std::wstring::npos || posEnd <= posStart + 1) {
		return false;
	}

	std::string modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try {
		modifiers = std::stoi(modStr);
	} catch (...) {
		return false;
	}

	// 取消旧的热键（可选）
	UnregisterHotKey(hWnd, hotkeyId);

	// 注册新的热键
	return RegisterHotKey(hWnd, hotkeyId, modifiers, vk);
}

// 简单按可执行文件路径杀进程（需要管理员权限时自行提权）：
inline int KillProcessByImagePath(const std::wstring& imagePath) {
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) return 0;

	int killed = 0;
	PROCESSENTRY32W pe{sizeof(pe)};
	if (Process32FirstW(snap, &pe)) {
		do {
			HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
			if (hProc) {
				wchar_t buf[MAX_PATH];
				DWORD sz = _countof(buf);
				if (QueryFullProcessImageNameW(hProc, 0, buf, &sz)) {
					if (_wcsicmp(buf, imagePath.c_str()) == 0) {
						if (TerminateProcess(hProc, 1)) ++killed;
					}
				}
				CloseHandle(hProc);
			}
		} while (Process32NextW(snap, &pe));
	}
	CloseHandle(snap);
	return killed;
}

inline std::vector<std::wstring> GetWStringArray(const nlohmann::json& j, const std::string& key) {
	std::vector<std::wstring> result;
	if (j.contains(key) && j[key].is_array()) {
		for (const auto& s : j[key]) {
			result.push_back(utf8_to_wide(s.get<std::string>()));
		}
	}
	return result;
}

inline void ShowMainWindowSimple() {
	RestoreWindowIfMinimized(g_mainHwnd);
	SetFocus(g_editHwnd);
	MyMoveWindow(g_mainHwnd);
	if (!pref_ignore_popup_sound && g_skinJson != nullptr) {
		std::string soundRelPath = g_skinJson.value("popup_sound", "");
		if (!soundRelPath.empty()) {
			std::wstring soundPath = utf8_to_wide(soundRelPath);
			if (soundPath[0] != L'\\' && soundPath[0] != L'/' && soundPath.size() >= 2 && soundPath[1] != L':') {
				soundPath = utf8_to_wide(g_skinJson.value("skin_folder", "")) + L"\\" + soundPath;
			}
			PlaySoundW(soundPath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
		}
	}
	// if (g_BgImage != nullptr) {
	// 	RedrawWindow(g_mainHwnd, nullptr, nullptr,
	// 				RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
	// }
	SetForegroundWindow(g_mainHwnd);
	if (g_hklIme != nullptr) PostMessageW(g_editHwnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)g_hklIme);
}

/**
 * 打开一个控制台，用于显示调试信息
 */
inline void AttachConsoleForDebug2() {
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	std::wcout << "Console attached!" << std::endl;
	std::wcout << "Console attached!" << std::endl;
}

inline void AttachConsoleForDebug() {
	AllocConsole();

	// 设置控制台为 UTF-8
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);

	// 让 C++ 的宽字符流输出 UTF-8y以hb//，// 会崩溃_setmode(_fileno(stdout), _O_U8TEXT);
	// _setmode(_fileno(stderr), _O_U8TEX////);
	_setmode(_fileno(stdin), _O_U8TEXT);

	std::wcout << L"控制台已附加 (UTF-8 模式) 🎉" << std::endl;
	std::wcout << L"测试中文输出：你好，世界！" << std::endl;
}

// 读取窗口位置
static RECT LoadWindowRectFromRegistry() {
	RECT rect = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT + MAIN_WINDOW_WIDTH, CW_USEDEFAULT + MAIN_WINDOW_HEIGHT};
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\CandyTek\\CandyLauncher", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD size = sizeof(rect);
		RegQueryValueExW(hKey, L"WindowRect", nullptr, nullptr, (LPBYTE)&rect, &size);
		RegCloseKey(hKey);
	}
	return rect;
}

// 保存窗口位置
static void SaveWindowRectToRegistry(HWND hWnd) {
	RECT rect;
	if (GetWindowRect(hWnd, &rect)) {
		HKEY hKey;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\CandyTek\\CandyLauncher", 0, nullptr, 0, KEY_WRITE, nullptr,
							&hKey, nullptr) == ERROR_SUCCESS) {
			RegSetValueExW(hKey, L"WindowRect", 0, REG_BINARY, (const BYTE*)&rect, sizeof(rect));
			RegCloseKey(hKey);
		}
	}
}

// 已弃用 Win7以前有效
static BOOL IsDwmExclusiveFullscreen(HWND hwnd) {
	BOOL isCloaked = FALSE;
	HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked));
	return SUCCEEDED(hr) && isCloaked;
}

static bool IsRunAsAdmin() {
	BOOL isAdmin = FALSE;
	PSID adminGroup = NULL;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

	if (AllocateAndInitializeSid(&NtAuthority, 2,
								SECURITY_BUILTIN_DOMAIN_RID,
								DOMAIN_ALIAS_RID_ADMINS,
								0, 0, 0, 0, 0, 0,
								&adminGroup)) {
		CheckTokenMembership(NULL, adminGroup, &isAdmin);
		FreeSid(adminGroup);
	}

	return isAdmin == TRUE;
}

static bool RelaunchAsAdmin() {
	WCHAR szPath[MAX_PATH];
	if (!GetModuleFileNameW(NULL, szPath, MAX_PATH)) return false;

	SHELLEXECUTEINFOW sei = {sizeof(sei)};
	sei.lpVerb = L"runas"; // 关键点：请求以管理员身份运行
	sei.lpFile = szPath; // 当前程序路径
	sei.hwnd = NULL;
	sei.nShow = SW_NORMAL;

	if (!ShellExecuteExW(&sei)) {
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_CANCELLED) {
			MessageBoxW(NULL, L"用户取消了权限提升。", L"提示", MB_ICONINFORMATION | MB_TOPMOST);
		} else {
			MessageBoxW(NULL, L"无法以管理员身份重新启动程序。", L"错误", MB_ICONERROR | MB_TOPMOST);
		}
		return false;
	}

	return true;
}

static void ChangeEditTextArg(const std::wstring& arg2) {
	std::wstring editTextBuffer2;
	if (StartsWith(editTextBuffer, LR"({"arg":")")) {
		if (const size_t end = find_json_end(editTextBuffer); end != std::wstring::npos) {
			const std::wstring json_part = editTextBuffer.substr(0, end + 1);
			bool isSuccess = false;
			try {
				nlohmann::json j = nlohmann::json::parse(wide_to_utf8(json_part));
				const std::string arg = j["arg"];
				currectActionArg = utf8_to_wide(arg);
				isSuccess = true;
			} catch (const nlohmann::json::parse_error& e) {
				std::wcerr << L"JSON 解析错误：" << utf8_to_wide(e.what()) << std::endl;
			}
			if (isSuccess) {
				editTextBuffer2 = editTextBuffer.substr(end + 1);
			} else {
				editTextBuffer2 = (editTextBuffer);
				currectActionArg = L"";
			}
		} else {
			editTextBuffer2 = (editTextBuffer);
			currectActionArg = L"";
		}
	} else {
		editTextBuffer2 = (editTextBuffer);
		currectActionArg = L"";
	}
			
	nlohmann::json j;
	j["arg"] = wide_to_utf8(arg2);
	SetWindowTextW(g_editHwnd, (utf8_to_wide(j.dump()) + editTextBuffer2).c_str());
	const int textLength = GetWindowTextLengthW(g_editHwnd);
	SendMessageW(g_editHwnd, EM_SETSEL, (WPARAM)textLength, (LPARAM)textLength);
	SendMessageW(g_editHwnd, EM_SCROLLCARET, 0, 0);
}
