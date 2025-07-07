#pragma once

#ifndef APPTOOLS_H
#define APPTOOLS_H
#include "SettingsHelper.hpp"
#include "TrayMenuManager.h"
#include "dwmapi.h"
#include "SettingWindow.hpp"
#include "ShortCutHelper.hpp"

#endif //APPTOOLS_H

// --- (在此处粘贴上面定义的 MAKE_HOTKEY_KEY, HotkeyMap) ---
#define MAKE_HOTKEY_KEY(modifiers, vk) (static_cast<UINT64>(modifiers) << 32 | static_cast<UINT64>(vk))
using HotkeyMap = std::unordered_map<UINT64, UINT64>;


static bool IsRunAsAdmin()
{
	BOOL isAdmin = FALSE;
	PSID adminGroup = NULL;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

	if (AllocateAndInitializeSid(&NtAuthority, 2,
								SECURITY_BUILTIN_DOMAIN_RID,
								DOMAIN_ALIAS_RID_ADMINS,
								0, 0, 0, 0, 0, 0,
								&adminGroup))
	{
		CheckTokenMembership(NULL, adminGroup, &isAdmin);
		FreeSid(adminGroup);
	}

	return isAdmin == TRUE;
}


static bool RelaunchAsAdmin()
{
	WCHAR szPath[MAX_PATH];
	if (!GetModuleFileNameW(NULL, szPath, MAX_PATH))
		return false;

	SHELLEXECUTEINFOW sei = {sizeof(sei)};
	sei.lpVerb = L"runas"; // 关键点：请求以管理员身份运行
	sei.lpFile = szPath; // 当前程序路径
	sei.hwnd = NULL;
	sei.nShow = SW_NORMAL;

	if (!ShellExecuteExW(&sei))
	{
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_CANCELLED)
		{
			MessageBoxW(NULL, L"用户取消了权限提升。", L"提示", MB_ICONINFORMATION);
		}
		else
		{
			MessageBoxW(NULL, L"无法以管理员身份重新启动程序。", L"错误", MB_ICONERROR);
		}
		return false;
	}

	return true;
}

// 从类似 "Ctrl+Alt+A(3)(65)" 字符串中提取并注册全局热键
static bool RegisterHotkeyFromString(HWND hWnd, const std::wstring& hotkeyStr, int hotkeyId)
{
	UINT modifiers = 0;
	UINT vk = 0;

	// 查找括号中的 VK 值
	size_t posStart = hotkeyStr.rfind(L"(");
	size_t posEnd = hotkeyStr.rfind(L")");

	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1)
	{
		return false;
	}

	std::wstring vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try
	{
		vk = std::stoi(vkStr);
	}
	catch (...)
	{
		return false;
	}

	// 查找 modifiers 值
	posEnd = posStart - 1;
	posStart = hotkeyStr.rfind(L"(", posEnd);
	if (posStart == std::wstring::npos || posEnd <= posStart + 1)
	{
		return false;
	}

	std::wstring modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try
	{
		modifiers = std::stoi(modStr);
	}
	catch (...)
	{
		return false;
	}

	// 取消旧的热键（可选）
	UnregisterHotKey(hWnd, hotkeyId);

	// 注册新的热键
	return RegisterHotKey(hWnd, hotkeyId, modifiers, vk);
}

// 解析字符串并存储到 Map 中，用于主程序里的窗口快捷键
static bool AddHotkey(const std::wstring& hotkeyStr, UINT64 action)
{
	UINT modifiers = 0;
	UINT vk = 0;

	// 你的解析代码...
	size_t posStart = hotkeyStr.rfind(L"(");
	size_t posEnd = hotkeyStr.rfind(L")");

	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1)
	{
		return false;
	}

	std::wstring vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try
	{
		vk = std::stoi(vkStr);
	}
	catch (...)
	{
		return false;
	}

	posEnd = posStart - 1;
	posStart = hotkeyStr.rfind(L"(", posEnd);
	if (posStart == std::wstring::npos || posEnd <= posStart + 1)
	{
		// 兼容没有修饰符的情况, 例如 "xx(89)"
		modifiers = 0;
	}
	else
	{
		std::wstring modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
		try
		{
			modifiers = std::stoi(modStr);
		}
		catch (...)
		{
			return false;
		}
	}

	// 生成唯一的键
	UINT64 key = MAKE_HOTKEY_KEY(modifiers, vk);

	// 存储到哈希表中
	g_hotkeyMap[key] = action;

	return true;
}

// 已弃用 Win7以前有效
static BOOL IsDwmExclusiveFullscreen(HWND hwnd)
{
	BOOL isCloaked = FALSE;
	HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked));
	return SUCCEEDED(hr) && isCloaked;
}

static bool shouldShowInCurrentWindowMode(HWND hwnd)
{
	if (hwnd == nullptr)
	{
		return true;
	}
	// 获取窗口样式
	const LONG style = GetWindowLong(hwnd, GWL_STYLE);
	const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (style & WS_CAPTION || style & WS_THICKFRAME || exStyle & WS_EX_WINDOWEDGE)
	{
		// 如果有这些样式，基本可以确定不是全屏
		return true;
	}

	// 获取窗口所在屏幕
	HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);
	RECT screenRect = monitorInfo.rcMonitor;

	// 获取窗口位置
	RECT windowRect;
	// 获取失败，默认窗口模式
	if (!GetWindowRect(hwnd, &windowRect))
		return true;

	// 检查是否为无边框全屏
	if ((style & WS_BORDER) == 0 &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom)
	{
		// 检查是否是桌面（类名为 "Progman"）
		char className[256];
		GetClassNameA(hwnd, className, sizeof(className));
		if (std::string(className) == "Progman")
		{
			return true;
		}
		return false;
	}

	// 检查是否全屏（隐藏任务栏等）
	if ((style & WS_CAPTION) == 0 &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom)
	{
		return false;
	}

	return true;
}

static bool shouldShowInCurrentWindowTopmostMode(HWND hwnd)
{
	if (hwnd == nullptr)
	{
		return true;
	}
	// 获取窗口样式
	const LONG style = GetWindowLong(hwnd, GWL_STYLE);
	const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (style & WS_CAPTION || style & WS_THICKFRAME || exStyle & WS_EX_WINDOWEDGE)
	{
		// 如果有这些样式，基本可以确定不是全屏
		return true;
	}

	// 获取窗口所在屏幕
	HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);
	RECT screenRect = monitorInfo.rcMonitor;

	// 获取窗口位置
	RECT windowRect;
	// 获取失败，默认窗口模式
	if (!GetWindowRect(hwnd, &windowRect))
		return true;

	// 检查是否为无边框全屏
	if (exStyle & WS_EX_TOPMOST &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom)
	{
		return false;
	}

	return true;
}


static void userSettingsAfterTheAppStart(TrayMenuManager trayMenuManager)
{
	initGlobalVariable();
	bool pref_run_app_as_admin = (settingsMap["pref_run_app_as_admin"].defValue.get<int>() == 1);
	if (pref_run_app_as_admin)
	{
		if (!IsRunAsAdmin())
		{
			if (RelaunchAsAdmin())
			{
				ExitProcess(0);
			}
		}
	}

	bool pref_show_tray_icon = (settingsMap["pref_show_tray_icon"].defValue.get<int>() == 1);
	std::wstring pref_hotkey_open_file_location = utf8_to_wide(
		settingsMap["pref_hotkey_open_file_location"].defValue.get<std::string>());
	std::wstring pref_hotkey_open_target_location = utf8_to_wide(
		settingsMap["pref_hotkey_open_target_location"].defValue.get<std::string>());
	std::wstring pref_hotkey_open_with_clipboard_params = utf8_to_wide(
		settingsMap["pref_hotkey_open_with_clipboard_params"].defValue.get<std::string>());
	std::wstring pref_hotkey_run_item_as_admin = utf8_to_wide(
		settingsMap["pref_hotkey_run_item_as_admin"].defValue.get<std::string>());

	if (pref_show_tray_icon)
	{
		trayMenuManager.ShowTrayIcon();
	}
	else
	{
		trayMenuManager.HideTrayIcon();
	}


	RegisterHotkeyFromString(s_mainHwnd, L"xxx(1)(75)",HOTKEY_ID_TOGGLE_MAIN_PANEL);


	g_hotkeyMap.clear();
	if (!pref_hotkey_open_file_location.empty())
		AddHotkey(pref_hotkey_open_file_location, HOTKEY_ID_OPEN_FILE_LOCATION);
	if (!pref_hotkey_open_target_location.empty())
		AddHotkey(pref_hotkey_open_target_location, HOTKEY_ID_OPEN_TARGET_LOCATION);
	if (!pref_hotkey_open_with_clipboard_params.empty())
		AddHotkey(pref_hotkey_open_with_clipboard_params, HOTKEY_ID_OPEN_WITH_CLIPBOARD_PARAMS);
	if (!pref_hotkey_run_item_as_admin.empty())
		AddHotkey(pref_hotkey_run_item_as_admin, HOTKEY_ID_RUN_ITEM_AS_ADMIN);


	if (pref_force_ime_mode == "null")
	{
		g_hklIme = NULL;
	}
	else if (pref_force_ime_mode == "en")
	{
		g_hklIme = LoadKeyboardLayoutW(L"00000409", KLF_ACTIVATE);
	}
	else if (pref_force_ime_mode == "ch")
	{
		g_hklIme = LoadKeyboardLayoutW(L"00000804", KLF_ACTIVATE);
	}
	else if (pref_force_ime_mode == "custom")
	{
		try
		{
			g_hklIme = LoadKeyboardLayoutW(
				utf8_to_wide(settingsMap["pref_custom_ime_id"].defValue.get<std::string>()).c_str(), KLF_ACTIVATE);
		}
		catch (...)
		{
			g_hklIme = NULL;
		}
	}
}


/**
 * 当用户配置更改时，执行一些功能变更操作
 */
static void doPrefChanged(TrayMenuManager trayMenuManager)
{
	userSettingsAfterTheAppStart(trayMenuManager);
	// 初始化一些常见变量
	wchar_t myAppExePath[MAX_PATH];
	GetModuleFileNameW(nullptr, myAppExePath, MAX_PATH);
	const std::wstring startUpShortcutName = L"CandyLauncherBest";

	bool pref_auto_start = (settingsMap["pref_auto_start"].defValue.get<int>() == 1);
	bool pref_indexed_apps_show_sendto_shortcut = (settingsMap["pref_indexed_apps_show_sendto_shortcut"].defValue.get<int>() == 1);

	bool startupShortcutExists = IsStartupShortcutExists(startUpShortcutName);

	if (pref_auto_start)
	{
		if (!startupShortcutExists)
		{
			// 创建快捷方式
			CreateStartupShortcut(myAppExePath, startUpShortcutName);
		}
	}
	else
	{
		if (startupShortcutExists)
		{
			bool result = DeleteStartupShortcut(startUpShortcutName);
			ShowErrorMsgBox(L"删除快捷方式" + result);
		}
	}

	if (pref_indexed_apps_show_sendto_shortcut)
	{
		
	}
	
}


// 读取窗口位置
static RECT LoadWindowRectFromRegistry()
{
	RECT rect = { CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT + MAIN_WINDOW_WIDTH, CW_USEDEFAULT + MAIN_WINDOW_HEIGHT };
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\CandyTek\\CandyLauncher", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD size = sizeof(rect);
		RegQueryValueExW(hKey, L"WindowRect", nullptr, nullptr, (LPBYTE)&rect, &size);
		RegCloseKey(hKey);
	}
	return rect;
}

// 保存窗口位置
static void SaveWindowRectToRegistry(HWND hWnd)
{
	RECT rect;
	if (GetWindowRect(hWnd, &rect)) {
		HKEY hKey;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\CandyTek\\CandyLauncher", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
			RegSetValueExW(hKey, L"WindowRect", 0, REG_BINARY, (const BYTE*)&rect, sizeof(rect));
			RegCloseKey(hKey);
		}
	}
}
