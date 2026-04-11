// 此类放置与 CandyLauncher 强相关的方法，不要被其他文件引用，只能被唯一主程序引用

#pragma once

#include "../window/SettingsManager.hpp"
#include "../manager/TrayMenuManager.hpp"
#include "dwmapi.h"
#include "../window/SettingsWindow.hpp"
#include "../util/ShortcutUtil.hpp"
// #include "ListedRunnerPlugin.h"
#include "../manager/ListViewManager.hpp"
#include <atomic>

constexpr bool needOpenDebugCmd = false;
constexpr bool needOpenShell32IconViewer = false;
constexpr bool needOpenIndexedManager = false;
constexpr bool needOpenSettingWindow = false;
constexpr bool needMinimizeSettingWindow = false;


// --- (在此处粘贴上面定义的 MAKE_HOTKEY_KEY, HotkeyMap) ---
#define MAKE_HOTKEY_KEY(modifiers, vk) (static_cast<UINT64>(modifiers) << 32 | static_cast<UINT64>(vk))
using HotkeyMap = std::unordered_map<UINT64, UINT64>;

// 解析字符串并存储到 Map 中，用于主程序里的窗口快捷键
static bool AddHotkey(const std::wstring& hotkeyStr, UINT64 action) {
	UINT modifiers = 0;
	UINT vk = 0;

	// 你的解析代码...
	size_t posStart = hotkeyStr.rfind(L'(');
	size_t posEnd = hotkeyStr.rfind(L')');

	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1) {
		return false;
	}

	std::wstring vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try {
		vk = std::stoi(vkStr);
	} catch (...) {
		return false;
	}

	posEnd = posStart - 1;
	posStart = hotkeyStr.rfind(L'(', posEnd);
	if (posStart == std::wstring::npos || posEnd <= posStart + 1) {
		// 兼容没有修饰符的情况, 例如 "xx(89)"
		modifiers = 0;
	} else {
		std::wstring modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
		try {
			modifiers = std::stoi(modStr);
		} catch (...) {
			return false;
		}
	}

	// 生成唯一的键
	UINT64 key = MAKE_HOTKEY_KEY(modifiers, vk);

	// 存储到哈希表中
	g_hotkeyMap[key] = action;

	return true;
}

static bool AddHotkey(const std::string& hotkeyStr, UINT64 action) {
	return AddHotkey(utf8_to_wide(hotkeyStr), action);
}

static bool shouldShowInCurrentWindowMode(HWND hwnd) {
	if (hwnd == nullptr) {
		return true;
	}
	// 获取窗口样式
	const LONG style = GetWindowLong(hwnd, GWL_STYLE);
	const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (style & WS_CAPTION || style & WS_THICKFRAME || exStyle & WS_EX_WINDOWEDGE) {
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
	if (!GetWindowRect(hwnd, &windowRect)) return true;

	// 检查是否为无边框全屏
	if ((style & WS_BORDER) == 0 &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom) {
		// 检查是否是桌面（类名为 "Progman"）
		char className[256];
		GetClassNameA(hwnd, className, sizeof(className));
		if (std::string(className) == "Progman") {
			return true;
		}
		return false;
	}

	// 检查是否全屏（隐藏任务栏等）
	if ((style & WS_CAPTION) == 0 &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom) {
		return false;
	}

	return true;
}

static bool shouldShowInCurrentWindowTopmostMode(HWND hwnd) {
	if (hwnd == nullptr) {
		return true;
	}
	// 获取窗口样式
	const LONG style = GetWindowLong(hwnd, GWL_STYLE);
	const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (style & WS_CAPTION || style & WS_THICKFRAME || exStyle & WS_EX_WINDOWEDGE) {
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
	if (!GetWindowRect(hwnd, &windowRect)) return true;

	// 检查是否为无边框全屏
	if (exStyle & WS_EX_TOPMOST &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom) {
		return false;
	}

	return true;
}

static void userSettingsAfterTheAppStart() {
	initGlobalVariable();
	bool pref_run_app_as_admin = (g_settings_map["pref_run_app_as_admin"].boolValue);
	if (pref_run_app_as_admin) {
		if (!IsRunAsAdmin()) {
			if (RelaunchAsAdmin()) {
				ExitProcess(0);
				return;
			}
		}
	}

	bool pref_show_tray_icon = (g_settings_map["pref_show_tray_icon"].boolValue);
	std::wstring pref_hotkey_open_file_location = utf8_to_wide(
		g_settings_map["pref_hotkey_open_file_location"].stringValue);
	std::wstring pref_hotkey_open_target_location = utf8_to_wide(
		g_settings_map["pref_hotkey_open_target_location"].stringValue);
	std::wstring pref_hotkey_open_with_clipboard_params = utf8_to_wide(
		g_settings_map["pref_hotkey_open_with_clipboard_params"].stringValue);
	std::wstring pref_hotkey_run_item_as_admin = utf8_to_wide(
		g_settings_map["pref_hotkey_run_item_as_admin"].stringValue);

	if (pref_show_tray_icon) {
		ShowTrayIcon();
		//		AddTrayIcon(g_mainHwnd);
	} else {
		HideTrayIcon();
	}

	// UnregisterHotKey(s_mainHwnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
	// 这里重新注册快捷键的话要设置一下延时，不然不生效
	SetTimer(g_mainHwnd, TIMER_SET_GLOBAL_HOTKEY, 100, nullptr);
	g_hotkeyMap.clear();
	if (!pref_hotkey_open_file_location.empty()) AddHotkey(pref_hotkey_open_file_location, HOTKEY_ID_OPEN_FILE_LOCATION);
	if (!pref_hotkey_open_target_location.empty()) AddHotkey(pref_hotkey_open_target_location, HOTKEY_ID_OPEN_TARGET_LOCATION);
	if (!pref_hotkey_open_with_clipboard_params.empty())
		AddHotkey(pref_hotkey_open_with_clipboard_params,
				HOTKEY_ID_OPEN_WITH_CLIPBOARD_PARAMS);
	if (!pref_hotkey_run_item_as_admin.empty()) AddHotkey(pref_hotkey_run_item_as_admin, HOTKEY_ID_RUN_ITEM_AS_ADMIN);
	if (!(g_settings_map["pref_hotkey_show_setting"].stringValue).empty())
		AddHotkey(
			g_settings_map["pref_hotkey_show_setting"].stringValue, HOTKEY_ID_SHOW_SETTING_WINDOW);


	if (pref_force_ime_mode == "null") {
		g_hklIme = NULL;
	} else if (pref_force_ime_mode == "en") {
		g_hklIme = LoadKeyboardLayoutW(L"00000409", KLF_ACTIVATE);
	} else if (pref_force_ime_mode == "ch") {
		g_hklIme = LoadKeyboardLayoutW(L"00000804", KLF_ACTIVATE);
	} else if (pref_force_ime_mode == "custom") {
		try {
			g_hklIme = LoadKeyboardLayoutW(
				utf8_to_wide(g_settings_map["pref_custom_ime_id"].stringValue).c_str(), KLF_ACTIVATE);
		} catch (...) {
			g_hklIme = NULL;
		}
	}
}

/**
 * 当用户配置更改时，执行一些功能变更操作
 */
static void doPrefChanged() {
	userSettingsAfterTheAppStart();
	// 初始化一些常见变量
	wchar_t myAppExePath[MAX_PATH];
	GetModuleFileNameW(nullptr, myAppExePath, MAX_PATH);
	const std::wstring startUpShortcutName = L"CandyLauncherBest";

	bool pref_auto_start = (g_settings_map["pref_auto_start"].boolValue);

	bool startupShortcutExists = IsStartupShortcutExists(startUpShortcutName);

	if (pref_auto_start) {
		if (!startupShortcutExists) {
			// 创建快捷方式
			CreateStartupShortcut(myAppExePath, startUpShortcutName);
		}
	} else {
		if (startupShortcutExists) {
			bool result = DeleteStartupShortcut(startUpShortcutName);
			// ShowErrorMsgBox(L"删除快捷方式" + result);
		}
	}
}

inline void DoMyContextMenuAction(UINT cmd, int index, std::shared_ptr<BaseAction>& action) {
	switch (cmd) {
	case IDM_REMOVE_ITEM:
		{
			// 从 ListView 以及你的数据结构移除
			//			ListView_DeleteItem(ListViewManager::hListView, index);
			//			ListViewManager::filteredActions.erase(ListViewManager::filteredActions.begin() + index);
		}
		break;
	case IDM_RENAME_ITEM:
		{
			// A. 只改 ListView 的显示名：启动就地编辑
			//			ListView_EditLabel(ListViewManager::hListView, index);
			// B. 如果你想真的重命名文件：
			//    建议在 LVN_ENDLABELEDIT 里拿到新名字，调用 MoveFileExW(old, new, 0)
		}
		break;
	case IDM_RUN_AS_ADMIN:
		// action->InvokeWithTarget(nullptr, true);
		// action->Invoke();
		break;
	case IDM_OPEN_IN_CONSOLE:
		// OpenConsoleHere(SaveGetShortcutTargetAndReturn(action->GetTargetPath()));
		break;
	case IDM_KILL_PROCESS:
		{
			// std::wstring actualPath = GetShortcutTarget(action->GetTargetPath());
			// if (systemProcesses.find(MyToLower(actualPath)) != systemProcesses.end()) {
			// 	MessageBoxW(nullptr, (actualPath + L" 是系统关键进程，不建议终止。").c_str(),
			// 				L"警告", MB_OK | MB_ICONWARNING | MB_TOPMOST);
			// } else {
			// 	int n = KillProcessByImagePath(actualPath);
			// 	using namespace std::string_literals;
			// 	Println(L"终止"s + std::to_wstring(n));
			// }
		}
		break;
	case IDM_COPY_PATH:
		// CopyTextToClipboard(g_mainHwnd, action->GetTargetPath());
		break;
	case IDM_COPY_TARGET_PATH:
		{
			// std::wstring actualPath = GetShortcutTarget(action->GetTargetPath());
			// CopyTextToClipboard(g_mainHwnd, actualPath);
		}
		break;
	default: break;
	}
}

static UINT ShowMyContextMenu(HWND hWnd, const std::wstring& path, POINT screenPt) {
	HMENU hMenu = CreatePopupMenu();
	// todo: 写完索引管理器时，来完善这个菜单项
	AppendMenuW(hMenu, MF_STRING, IDM_REMOVE_ITEM, L"排除该索引(未完善)");
	AppendMenuW(hMenu, MF_STRING, IDM_RENAME_ITEM, L"重映射命名该索引(未完善)");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_RUN_AS_ADMIN, L"以管理员身份运行");
	AppendMenuW(hMenu, MF_STRING, IDM_OPEN_IN_CONSOLE, L"在控制台打开该项");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(hMenu, MF_STRING, IDM_KILL_PROCESS, L"杀死进程");

	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, IDM_COPY_PATH, L"复制路径");

	AppendMenuW(hMenu, MF_STRING, IDM_COPY_TARGET_PATH, L"复制目标路径");

	// 根据上下文可禁用一些项（举例：路径不存在则灰掉）
	DWORD attr = GetFileAttributesW(path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
		EnableMenuItem(hMenu, IDM_RUN_AS_ADMIN, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hMenu, IDM_OPEN_IN_CONSOLE, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hMenu, IDM_KILL_PROCESS, MF_BYCOMMAND | MF_GRAYED);
		EnableMenuItem(hMenu, IDM_COPY_TARGET_PATH, MF_BYCOMMAND | MF_GRAYED);
	} else {
		if (MyEndsWith(path, {L".lnk"})) {
			std::wstring actualPath = GetShortcutTarget(path);
			if (!MyEndsWith(actualPath, {L".exe"})) {
				EnableMenuItem(hMenu, IDM_KILL_PROCESS, MF_BYCOMMAND | MF_GRAYED);
			}
		} else {
			EnableMenuItem(hMenu, IDM_COPY_TARGET_PATH, MF_BYCOMMAND | MF_GRAYED);
			if (!MyEndsWith(path, {L".exe"})) {
				EnableMenuItem(hMenu, IDM_KILL_PROCESS, MF_BYCOMMAND | MF_GRAYED);
			}
		}
	}


	SetForegroundWindow(hWnd); // 避免菜单失焦
	UINT cmd = TrackPopupMenuEx(
		hMenu,
		TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NOANIMATION,
		screenPt.x, screenPt.y,
		hWnd,
		nullptr
	);
	DestroyMenu(hMenu);
	return cmd; // 0 表示没点任何命令（点了空白/ESC）
}

static void TrayMenuClick(const int position) {
	switch (position) {
	case TRAY_MENU_ID_ABOUT: // 
		{
			ShowSettingsWindow(g_hInst, nullptr);
			RestoreWindowIfMinimized(g_settingsHwnd);
			SetForegroundWindow(g_settingsHwnd);
			SwitchToTab("about");
		}
		break;
	case TRAY_MENU_ID_SHOW_WINDOW: // 打开主窗口
		{
			PostMessageW(g_mainHwnd,WM_HOTKEY,HOTKEY_ID_TOGGLE_MAIN_PANEL, 0);
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
	case TRAY_MENU_ID_SETTINGS: // 前往设置
		{
			ShowSettingsWindow(g_hInst, nullptr);
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
			DestroyWindow(g_mainHwnd);
		}
		break;
	case TRAY_MENU_ID_EXIT: // 退出
		DestroyWindow(g_mainHwnd);
		break;
	default: ;
	}
}

inline std::unordered_map<std::string, std::function<void()>> getAppLaunchActionCallBacks() {
	std::unordered_map<std::string, std::function<void()>> callbacks;
	HWND hWnd = g_mainHwnd;

	callbacks["refreshList"] = []() {
		// refreshPluginRunner();
		g_pluginManager->RefreshAllActions();
	};
	callbacks["refreshPlugins"] = []() {
		// g_pluginManager->RefreshAllActions();
	};
	callbacks["quit"] = [hWnd]() {
		DestroyWindow(hWnd);
	};
	callbacks["restart"] = [hWnd]() {
		TrayMenuClick(10005);
	};
	callbacks["settings"] = [hWnd]() {
		TrayMenuClick(10010);
	};
	callbacks["editUserConfig"] = []() {
		// 打开用户配置文件
		ShellExecute(nullptr, L"open", USER_SETTINGS_PATH.c_str(), nullptr, nullptr, SW_SHOW);
	};

	callbacks["viewAllSettings"] = []() {
		// 提取并打开 settings.json (从资源文件)
		HMODULE hModule = GetModuleHandle(nullptr);
		HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_SETTINGS_JSON), RT_RCDATA);
		if (hRes) {
			HGLOBAL hData = LoadResource(hModule, hRes);
			if (hData) {
				DWORD dataSize = SizeofResource(hModule, hRes);
				void* pData = LockResource(hData);
				if (pData) {
					// 创建临时文件并写入内容
					std::wstring tempPath = std::filesystem::temp_directory_path().wstring() + L"\\settings_view.json";
					std::ofstream file(tempPath, std::ios::binary);
					file.write(static_cast<const char*>(pData), dataSize);
					file.close();
					// 打开临时文件
					ShellExecute(nullptr, L"open", tempPath.c_str(), nullptr, nullptr, SW_SHOW);
				}
			}
		}
	};

	callbacks["openHelp"] = []() {
		// 打开项目主页的帮助文档
		ShellExecute(nullptr, L"open", L"https://github.com/CandyTek/CandyLauncher/wiki", nullptr, nullptr, SW_SHOW);
	};

	callbacks["checkUpdate"] = []() {
		// 打开项目主页检查更新
		ShellExecute(nullptr, L"open", L"https://github.com/CandyTek/CandyLauncher/releases", nullptr, nullptr,
					SW_SHOW);
	};

	callbacks["feedback"] = []() {
		// 打开项目主页反馈建议
		ShellExecute(nullptr, L"open", L"https://github.com/CandyTek/CandyLauncher/issues", nullptr, nullptr, SW_SHOW);
	};

	callbacks["openGithub"] = []() {
		// 打开项目Github主页
		ShellExecute(nullptr, L"open", L"https://github.com/CandyTek/CandyLauncher", nullptr, nullptr, SW_SHOW);
	};
	return callbacks;
}

inline int MainWindowCustomPaint(PAINTSTRUCT ps, HDC hdc) {
	// 使用 GDI+ 绘制背景图
	// 如果没有背景图，则填充纯色背景
	if (g_skinJson != nullptr) {
		if (g_BgImage) {
			Gdiplus::Graphics graphics(hdc);
			graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
			Gdiplus::Rect rect(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
			graphics.DrawImage(g_BgImage, rect);
		} else {
			std::string bgColorStr = g_skinJson.value("window_bg_color", "#FFFFFF");
			if (bgColorStr.empty()) {
				EndPaint(g_mainHwnd, &ps);
				return 1;
			} else {
				// 绘制透明色
				Gdiplus::SolidBrush bgBrush(HexToGdiplusColor(bgColorStr));
				Gdiplus::Graphics graphics(hdc);
				graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
				Gdiplus::Rect rect(ps.rcPaint.left,
									ps.rcPaint.top,
									ps.rcPaint.right - ps.rcPaint.left,
									ps.rcPaint.bottom - ps.rcPaint.top);
				graphics.FillRectangle(&bgBrush, rect);
			}
		}
	} else {
		HBRUSH hBrush = CreateSolidBrush(COLOR_UI_BG);
		FillRect(hdc, &ps.rcPaint, hBrush);
		DeleteObject(hBrush);
	}
	return -1;
}

inline int mainWindowHotkey(WPARAM wParam) {
	switch (wParam) {
	case HOTKEY_ID_TOGGLE_MAIN_PANEL:
		{
			Println(L"Hotkey Alt + K");
			if (IsWindowVisible(g_mainHwnd)) {
				HideWindow();
			} else {
				// 判断全屏应用模式
				bool shouldShow = true;
				if (pref_hide_in_fullscreen) shouldShow = shouldShowInCurrentWindowMode(GetForegroundWindow());
				else if (pref_hide_in_topmost_fullscreen) shouldShow = shouldShowInCurrentWindowTopmostMode(GetForegroundWindow());

				if (shouldShow) {
					if (pref_show_window_and_release_modifier_key) ReleaseAltKey();
					ShowMainWindowSimple();
					g_pluginManager->OnMainWindowShowNotifi(true);
				}
			}
		}
		return 0;
	default: break;
	}
	return -1;
}

inline void editControlHotkey(WPARAM wParam) {
	std::shared_ptr<BaseAction> it = GetListViewSelectedAction(
		g_listViewHwnd, filteredActions);
	if (!it) {
		switch (wParam) {
		case HOTKEY_ID_SHOW_SETTING_WINDOW: ShowSettingsWindow(g_hInst, nullptr);
			if (pref_close_after_open_item) HideWindow();
			break;
		default: ;
		}
	} else {
		bool needHide = true;
		switch (wParam) {
		case HOTKEY_ID_RUN_ITEM:
			// it->Invoke();
			needHide = PluginManager::DispatchActionExecute(it, currectActionArg);
			break;
		case HOTKEY_ID_OPEN_FILE_LOCATION:
			// it->InvokeOpenFolder();
			break;
		case HOTKEY_ID_OPEN_TARGET_LOCATION:
			// it->InvokeOpenGoalFolder();
			break;
		case HOTKEY_ID_RUN_ITEM_AS_ADMIN:
			// it->InvokeWithTarget(nullptr, TRUE);
			break;
		case HOTKEY_ID_OPEN_WITH_CLIPBOARD_PARAMS:
			// it->InvokeWithTargetClipBoard();
			break;
		case HOTKEY_ID_SHOW_SETTING_WINDOW: ShowSettingsWindow(g_hInst, nullptr);
			break;
		default: break;
		}
		if (pref_close_after_open_item && needHide) HideWindow();
	}
}
