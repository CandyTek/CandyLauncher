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

#include "Constants.hpp"
#include "DataKeeper.hpp"

class RunCommandAction;

struct MonitorData {
	HMONITOR hMonitor;
	MONITORINFOEX mi;
	bool isPrimary;
};

enum : UINT {
	// 防止和弹出菜单的esc 0 值冲突
	IDM_REMOVE_ITEM = 9999,
	IDM_RENAME_ITEM,
	IDM_RUN_AS_ADMIN,
	IDM_OPEN_IN_CONSOLE,
	IDM_KILL_PROCESS,
	IDM_COPY_PATH,
	IDM_COPY_TARGET_PATH
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	std::vector<MonitorData> *monitors = reinterpret_cast<std::vector<MonitorData> *>(dwData);

	MONITORINFOEX mi{};
	mi.cbSize = sizeof(mi);

	if (GetMonitorInfo(hMonitor, &mi)) {
		bool isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY);
		monitors->push_back({hMonitor, mi, isPrimary});
	}

	return TRUE;
}

static void HideWindow(HWND hWnd, HWND hEdit, HWND hListView) {
	// 不需要设置什么线程延时去关闭什么的，归根结底是alt键触发了控件的一些东西，屏蔽就好
	if (!pref_preserve_last_search_term) {
		SetWindowText(hEdit, L"");
		// 强制更新 UI
		UpdateWindow(hEdit);
		UpdateWindow(hListView);
	} else if (pref_last_search_term_selected) {
		PostMessageW(hEdit, EM_SETSEL, 0, -1);
	}
	RECT saveRect;
	if (!GetWindowRect(hWnd, &saveRect)) {
		lastWindowCenterX = -1;
		lastWindowCenterY = -1;
	} else {
		lastWindowCenterX = saveRect.left;
		lastWindowCenterY = saveRect.top;
	}

	ShowWindow(hWnd, SW_HIDE);

	//ListView_DeleteAllItems(hListView);
	//UpdateWindow(hListView);
	// SendMessage(hEdit, EM_SETSEL, 0, -1); // Ctrl+A: 全选
	//SendMessage(hWnd, WM_WINDOWS_HIDE, 0, 0); // Ctrl+A: 全选
	// SetTimer(hWnd, 1001, 50, NULL);
}

inline void RestoreWindowIfMinimized(HWND hWnd) {
	if (IsIconic(hWnd)) {
		ShowWindow(hWnd, SW_RESTORE);
	}
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
										 (float) (mi.rcWork.right - mi.rcWork.left - MAIN_WINDOW_WIDTH) *
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

	if (monitors.size() > index) {
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

		std::wcout << L"前台窗口位置: (" << x << ", " << y << ")" << std::endl;
		std::wcout << L"前台窗口大小: " << width << " x " << height << std::endl;

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
		std::wcout << L"获取窗口矩形失败。" << std::endl;
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
		MonitorData *pData = reinterpret_cast<MonitorData *>(lParam);

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

static void ShowMainWindowSimple(HWND hWnd, HWND hEdit) {
	RestoreWindowIfMinimized(hWnd);
	SetFocus(hEdit);
	MyMoveWindow(hWnd);
	SetForegroundWindow(hWnd);
	if (g_hklIme != nullptr)
		PostMessageW(hEdit, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM) g_hklIme);
}

// static const std::shared_ptr<RunCommandAction>& GetListViewSelectedAction(HWND hListView,std::vector<std::shared_ptr<RunCommandAction>>& filteredActions)
// {
// 	const int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
// 	return filteredActions[selected];
// }

static const std::shared_ptr<RunCommandAction> &GetListViewSelectedAction(
		HWND hListView, std::vector<std::shared_ptr<RunCommandAction>> &filteredActions) {
	const int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
	if (selected == -1 || selected < 0 || static_cast<size_t>(selected) >= filteredActions.size()) {
		// 这里你需要返回一个合法的引用，但当前没选中或者越界了
		// 可以考虑返回一个静态的空指针，避免崩溃
		static std::shared_ptr<RunCommandAction> emptyPtr = nullptr;
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


// };
struct ComInitGuard {
	ComInitGuard() { hr = CoInitialize(nullptr); }

	~ComInitGuard() { if (SUCCEEDED(hr)) CoUninitialize(); }

	HRESULT hr;
};

static void ShowShellContextMenu(HWND hwnd, const std::wstring &filePath, const POINT &ptScreen) {
	ComInitGuard guard;
	if (FAILED(guard.hr)) return;

	PIDLIST_ABSOLUTE pidl = nullptr;
	SFGAOF sfgao;
	guard.hr = SHParseDisplayName(filePath.c_str(), nullptr, &pidl, 0, &sfgao);
	if (FAILED(guard.hr)) return;

	IShellFolder *desktopFolder = nullptr;
	guard.hr = SHGetDesktopFolder(&desktopFolder);
	if (FAILED(guard.hr)) {
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

	IShellFolder *parentFolder = nullptr;
	guard.hr = SHBindToObject(
			nullptr,
			pidlParent,
			nullptr,
			IID_IShellFolder,
			(void **) &parentFolder
	);
	if (FAILED(guard.hr)) {
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	PCUITEMID_CHILD relpidl = ILFindLastID(pidl);

	IContextMenu *contextMenu = nullptr;
	guard.hr = parentFolder->GetUIObjectOf(hwnd, 1, &relpidl, IID_IContextMenu, nullptr, (void **) &contextMenu);
	if (SUCCEEDED(guard.hr)) {
		IContextMenu2 *contextMenu2 = nullptr;
		if (SUCCEEDED(contextMenu->QueryInterface(IID_IContextMenu2, (void **) &contextMenu2))) {
			contextMenu2->Release(); // 这步是必要的
		}

		IContextMenu3 *contextMenu3 = nullptr;
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

		contextMenu->QueryContextMenu(hMenu, 0, idCmdFirst, idCmdLast, CMF_NORMAL);

		int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
								 ptScreen.x, ptScreen.y, 0, hwnd, nullptr);

		if (cmd >= static_cast<int>(idCmdFirst) && cmd <= static_cast<int>(idCmdLast)) {
			CMINVOKECOMMANDINFOEX cmi = {0};
			cmi.cbSize = sizeof(cmi);
			cmi.fMask = CMIC_MASK_UNICODE;
			cmi.hwnd = hwnd;
			cmi.lpVerb = MAKEINTRESOURCEA(cmd - idCmdFirst);
			cmi.lpVerbW = MAKEINTRESOURCEW(cmd - idCmdFirst);

			cmi.nShow = SW_SHOWNORMAL;
			contextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO) &cmi);
		}

		DestroyMenu(hMenu);
	}

	contextMenu->Release();
	parentFolder->Release();
	desktopFolder->Release();
	CoTaskMemFree(pidl);
	CoTaskMemFree(pidlParent);
}

static void ShowShellContextMenu2(HWND hwnd, const std::wstring &filePath, const POINT &ptScreen) {
	ComInitGuard guard;
	HRESULT hr = CoInitialize(nullptr);
	if (FAILED(hr)) return;

	PIDLIST_ABSOLUTE pidl = nullptr;
	SFGAOF sfgao;
	hr = SHParseDisplayName(filePath.c_str(), nullptr, &pidl, 0, &sfgao);
	if (FAILED(hr)) return;

	IShellFolder *desktopFolder = nullptr;
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

	IShellFolder *parentFolder = nullptr;
	hr = SHBindToObject(
			nullptr,
			pidlParent,
			nullptr,
			IID_IShellFolder,
			(void **) &parentFolder
	);
	if (FAILED(hr)) {
		desktopFolder->Release();
		CoTaskMemFree(pidl);
		CoTaskMemFree(pidlParent);
		return;
	}

	PCUITEMID_CHILD relpidl = ILFindLastID(pidl);

	IContextMenu *contextMenu = nullptr;
	hr = parentFolder->GetUIObjectOf(hwnd, 1, &relpidl, IID_IContextMenu, nullptr, (void **) &contextMenu);
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
			contextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO) &cmi);
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
	HFONT hOldFont = (HFONT) SelectObject(hdc, hFontD);

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
	SendMessageW(hCtrl, WM_SETFONT, (WPARAM) hFont, TRUE);
}

static int GetWindowVScrollBarThumbWidth(HWND hwnd, bool bAutoShow) {
	SCROLLBARINFO sb = {0};
	sb.cbSize = sizeof(SCROLLBARINFO);
	GetScrollBarInfo(hwnd, OBJID_VSCROLL, &sb);

	if (!bAutoShow)
		return sb.dxyLineButton;

	if (sb.dxyLineButton)
		return sb.dxyLineButton;

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

inline bool IsRectOnAnyMonitor(const RECT &rc) {
	return MonitorFromRect(&rc, MONITOR_DEFAULTTONULL) != nullptr;
}

static bool IsShortcutInvalid(const std::wstring &shortcutPath) {
	ComInitGuard guard;
	if (FAILED(guard.hr)) return true;

	IShellLink *pShellLink = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink,
								  (void **) &pShellLink);
	if (FAILED(hr)) return true;

	IPersistFile *pPersistFile = nullptr;
	hr = pShellLink->QueryInterface(IID_IPersistFile, (void **) &pPersistFile);
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

	wchar_t targetPath[MAX_PATH] = {0};
	hr = pShellLink->GetPath(targetPath, MAX_PATH, nullptr, 0);

	pPersistFile->Release();
	pShellLink->Release();

	if (FAILED(hr)) return true;

	// Check if target file exists
	DWORD attributes = GetFileAttributes(targetPath);
	return (attributes == INVALID_FILE_ATTRIBUTES);
}

// 从 shell32.dll 加载指定索引的图标（16x16）
static HICON LoadShellIcon(int index, int cx = 16, int cy = 16) {
	HMODULE hMod = GetModuleHandleW(L"shell32.dll");
	return (HICON) LoadImageW(hMod, MAKEINTRESOURCEW(index), IMAGE_ICON, cx, cy, LR_SHARED);
}

// ---- 工具：将 HICON 转换为 32bpp DIB 的 HBITMAP（保留透明度）----
static HBITMAP IconToBitmap(HICON hIcon, int cx, int cy) {
	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = cx;
	bi.bmiHeader.biHeight = -cy;            // 负数 => top-down DIB，绘制更直接
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void *pBits = nullptr;
	HDC hdc = GetDC(nullptr);
	HBITMAP hBmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	HDC hMem = CreateCompatibleDC(hdc);
	HGDIOBJ hOld = SelectObject(hMem, hBmp);

	// 清空为全透明
	if (pBits) std::memset(pBits, 0, cx * cy * 4);

	DrawIconEx(hMem, 0, 0, hIcon, cx, cy, 0, nullptr, DI_NORMAL);

	SelectObject(hMem, hOld);
	DeleteDC(hMem);
	ReleaseDC(nullptr, hdc);
	return hBmp;
}

static TraverseOptions getTraverseOptions(const nlohmann::basic_json<> &cmd,bool isForceReturn = false) {
	std::wstring folderPath = Utf8ToWString(cmd["folder"].get<std::string>());
	folderPath = ExpandEnvironmentVariables(folderPath, EXE_FOLDER_PATH);
	TraverseOptions traverseOptions;
	if (!FolderExists(folderPath) && !isForceReturn) {
		std::wcerr << L"指定的文件夹不存在：" << folderPath << std::endl;
		return traverseOptions;
	}
	traverseOptions.command = Utf8ToWString(cmd["command"].get<std::string>());
	traverseOptions.folder = Utf8ToWString(cmd["folder"].get<std::string>());
	traverseOptions.extensions = {L".exe", L".lnk"};
	traverseOptions.recursive = true;

	if (cmd.contains("excludes") && cmd["excludes"].is_array()) {
		for (const auto &name: cmd["excludes"]) {
			if (name.is_string()) {
				traverseOptions.excludeNames.push_back(Utf8ToWString(name.get<std::string>()));
			}
		}
	}

	if (cmd.contains("exclude_words") && cmd["exclude_words"].is_array()) {
		for (const auto &word: cmd["exclude_words"]) {
			if (word.is_string()) {
				traverseOptions.excludeWords.push_back(Utf8ToWString(word.get<std::string>()));
			}
		}
	}

	if (cmd.contains("rename_sources") && cmd["rename_sources"].is_array() &&
		cmd.contains("rename_targets") && cmd["rename_targets"].is_array()) {
		const auto &sources = cmd["rename_sources"];
		const auto &targets = cmd["rename_targets"];
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

static auto shouldExclude = [](const TraverseOptions &options, const std::wstring &name) -> bool {
	std::wstring nameLower = name;
	std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
	if (std::find(options.excludeNames.begin(),
				  options.excludeNames.end(),
				  nameLower) != options.excludeNames.end()) {
		return true;
	}

	for (const auto &word: options.excludeWords) {
		if (nameLower.find(word) != std::wstring::npos) return true;
	}
	return false;
};

inline bool OpenConsoleHere(const std::wstring &targetPath) {
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
static bool RegisterHotkeyFromString(HWND hWnd, const std::string &hotkeyStr, int hotkeyId) {
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
	}
	catch (...) {
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
	}
	catch (...) {
		return false;
	}

	// 取消旧的热键（可选）
	UnregisterHotKey(hWnd, hotkeyId);

	// 注册新的热键
	return RegisterHotKey(hWnd, hotkeyId, modifiers, vk);
}

// 简单按可执行文件路径杀进程（需要管理员权限时自行提权）：
inline int KillProcessByImagePath(const std::wstring &imagePath) {
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

inline std::vector<std::wstring> GetWStringArray(const nlohmann::json &j, const std::string &key) {
	std::vector<std::wstring> result;
	if (j.contains(key) && j[key].is_array()) {
		for (const auto &s: j[key]) {
			result.push_back(utf8_to_wide(s.get<std::string>()));
		}
	}
	return result;
}

// 保存配置到文件，当前先测试，不执行保存
static void SaveConfigToFile(std::vector<TraverseOptions>& runnerConfigs) {
	if(true){
		return;
	}
	try {
		nlohmann::json j;
		for (const auto &config: runnerConfigs) {
			nlohmann::json item;
			item["command"] = config.command;
			item["folder"] = config.folder;
			item["exclude_words"] = config.excludeWords;
			item["excludes"] = config.excludeNames;
			item["rename_sources"] = config.renameSources;
			item["rename_targets"] = config.renameTargets;
			j.push_back(item);
		}

		std::ofstream file("runner.json");
		file << j.dump(1, '\t');
		file.close();
	}
	catch (const std::exception &e) {
		MessageBoxA(nullptr, ("Failed to save runner.json: " + std::string(e.what())).c_str(),
					"Error", MB_OK | MB_ICONERROR);
	}
}

// 解析 runner.json 文件
static std::vector<TraverseOptions> ParseRunnerConfig() {
	std::vector<TraverseOptions> configs;
	std::string filename = "runner.json";
	std::string jsonText = ReadUtf8File(filename);
	nlohmann::json runnerConfig;

	try {
		runnerConfig = nlohmann::json::parse(jsonText);
	}
	catch (const nlohmann::json::parse_error &e) {
		std::string error_msg = "Config file load error in '" + filename +
								"'. Using default settings.\n\nError: " + e.what();
	}

	try {
		for (const nlohmann::basic_json<> &item: runnerConfig) {
			TraverseOptions config = getTraverseOptions(item,true);
			configs.push_back(config);
		}
	}
	catch (const std::exception &e) {
		MessageBoxA(nullptr, ("Failed to parse runner.json: " + std::string(e.what())).c_str(),
					"Error", MB_OK | MB_ICONERROR);
	}

	return configs;
}

