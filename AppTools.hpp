#pragma once

#include "SettingsHelper.hpp"
#include "TrayMenuManager.h"
#include "dwmapi.h"
#include "SettingWindow.hpp"
#include "ShortCutHelper.hpp"

// --- (在此处粘贴上面定义的 MAKE_HOTKEY_KEY, HotkeyMap) ---
#define MAKE_HOTKEY_KEY(modifiers, vk) (static_cast<UINT64>(modifiers) << 32 | static_cast<UINT64>(vk))
using HotkeyMap = std::unordered_map<UINT64, UINT64>;


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
	if (!GetModuleFileNameW(NULL, szPath, MAX_PATH))
		return false;

	SHELLEXECUTEINFOW sei = {sizeof(sei)};
	sei.lpVerb = L"runas"; // 关键点：请求以管理员身份运行
	sei.lpFile = szPath; // 当前程序路径
	sei.hwnd = NULL;
	sei.nShow = SW_NORMAL;

	if (!ShellExecuteExW(&sei)) {
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_CANCELLED) {
			MessageBoxW(NULL, L"用户取消了权限提升。", L"提示", MB_ICONINFORMATION);
		} else {
			MessageBoxW(NULL, L"无法以管理员身份重新启动程序。", L"错误", MB_ICONERROR);
		}
		return false;
	}

	return true;
}

// 解析字符串并存储到 Map 中，用于主程序里的窗口快捷键
static bool AddHotkey(const std::wstring &hotkeyStr, UINT64 action) {
	UINT modifiers = 0;
	UINT vk = 0;

	// 你的解析代码...
	size_t posStart = hotkeyStr.rfind(L"(");
	size_t posEnd = hotkeyStr.rfind(L")");

	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1) {
		return false;
	}

	std::wstring vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try {
		vk = std::stoi(vkStr);
	}
	catch (...) {
		return false;
	}

	posEnd = posStart - 1;
	posStart = hotkeyStr.rfind(L"(", posEnd);
	if (posStart == std::wstring::npos || posEnd <= posStart + 1) {
		// 兼容没有修饰符的情况, 例如 "xx(89)"
		modifiers = 0;
	} else {
		std::wstring modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
		try {
			modifiers = std::stoi(modStr);
		}
		catch (...) {
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
static BOOL IsDwmExclusiveFullscreen(HWND hwnd) {
	BOOL isCloaked = FALSE;
	HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked));
	return SUCCEEDED(hr) && isCloaked;
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
	if (!GetWindowRect(hwnd, &windowRect))
		return true;

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
	if (!GetWindowRect(hwnd, &windowRect))
		return true;

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


static void userSettingsAfterTheAppStart(TrayMenuManager trayMenuManager) {
	initGlobalVariable();
	bool pref_run_app_as_admin = (g_settings_map["pref_run_app_as_admin"].boolValue);
	if (pref_run_app_as_admin) {
		if (!IsRunAsAdmin()) {
			if (RelaunchAsAdmin()) {
				ExitProcess(0);
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
		trayMenuManager.ShowTrayIcon();
//		TrayMenuManager::AddTrayIcon(g_mainHwnd);
	} else {
		trayMenuManager.HideTrayIcon();
	}

	// UnregisterHotKey(s_mainHwnd, HOTKEY_ID_TOGGLE_MAIN_PANEL);
	// 这里重新注册快捷键的话要设置一下延时，不然不生效
	SetTimer(g_mainHwnd, TIMER_SET_GLOBAL_HOTKEY, 100, nullptr);
	g_hotkeyMap.clear();
	if (!pref_hotkey_open_file_location.empty())
		AddHotkey(pref_hotkey_open_file_location, HOTKEY_ID_OPEN_FILE_LOCATION);
	if (!pref_hotkey_open_target_location.empty())
		AddHotkey(pref_hotkey_open_target_location, HOTKEY_ID_OPEN_TARGET_LOCATION);
	if (!pref_hotkey_open_with_clipboard_params.empty())
		AddHotkey(pref_hotkey_open_with_clipboard_params, HOTKEY_ID_OPEN_WITH_CLIPBOARD_PARAMS);
	if (!pref_hotkey_run_item_as_admin.empty())
		AddHotkey(pref_hotkey_run_item_as_admin, HOTKEY_ID_RUN_ITEM_AS_ADMIN);


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
		}
		catch (...) {
			g_hklIme = NULL;
		}
	}
}

/**
 * 当用户配置更改时，执行一些功能变更操作
 */
static void doPrefChanged(TrayMenuManager trayMenuManager) {
	userSettingsAfterTheAppStart(trayMenuManager);
	// 初始化一些常见变量
	wchar_t myAppExePath[MAX_PATH];
	GetModuleFileNameW(nullptr, myAppExePath, MAX_PATH);
	const std::wstring startUpShortcutName = L"CandyLauncherBest";

	bool pref_auto_start = (g_settings_map["pref_auto_start"].boolValue);
	bool pref_indexed_apps_show_sendto_shortcut = (g_settings_map["pref_indexed_apps_show_sendto_shortcut"].boolValue);


	bool startupShortcutExists = IsStartupShortcutExists(startUpShortcutName);

	if (pref_auto_start) {
		if (!startupShortcutExists) {
			// 创建快捷方式
			CreateStartupShortcut(myAppExePath, startUpShortcutName);
		}
	} else {
		if (startupShortcutExists) {
			bool result = DeleteStartupShortcut(startUpShortcutName);
			ShowErrorMsgBox(L"删除快捷方式" + result);
		}
	}

	PWSTR sendto = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
		// 需要编译CreateShortcut 文件夹里的win32 程序，然后把exe放在CandyLauncher.exe 同一个目录下
		std::wstring createShortcutExePath;
		createShortcutExePath.append(GetExecutableFolder()).append(L"\\CreateShortcut.exe");
		std::wstring sendToShortcutName;
		sendToShortcutName.append(sendto).append(L"\\").append(kLinkName);

		bool sendtoShortcutExists = PathFileExistsW(sendToShortcutName.c_str());

		if (pref_indexed_apps_show_sendto_shortcut) {
			if (!sendtoShortcutExists) {
				// 创建快捷方式
				InstallSendToEntry(createShortcutExePath);
			}
		} else {
			if (sendtoShortcutExists) {
				bool result = DeleteSendToEntry(kLinkName);
				// ShowErrorMsgBox(L"删除快捷方式" + result);
			}
		}
	}
}


// 读取窗口位置
static RECT LoadWindowRectFromRegistry() {
	RECT rect = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT + MAIN_WINDOW_WIDTH, CW_USEDEFAULT + MAIN_WINDOW_HEIGHT};
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\CandyTek\\CandyLauncher", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD size = sizeof(rect);
		RegQueryValueExW(hKey, L"WindowRect", nullptr, nullptr, (LPBYTE) &rect, &size);
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
			RegSetValueExW(hKey, L"WindowRect", 0, REG_BINARY, (const BYTE *) &rect, sizeof(rect));
			RegCloseKey(hKey);
		}
	}
}

// 监听皮肤文件
static void watchSkinFile() {
	std::wstring directory = LR"(C:\Users\Administrator\source\repos\WindowsProject1)";
	std::wstring fileName = L"skin_test.json";

	HANDLE hDir = CreateFileW(
			directory.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS,
			nullptr
	);


	if (hDir == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		std::wcerr << L"无法打开目录句柄。错误代码：" << err << std::endl;
		return;
	}

	// 1. (推荐) 使用 BYTE 数组作为缓冲区
	char buffer[(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH) * 2]{};
	DWORD bytesReturned;

	while (true) // 线程将在此循环
	{
		BOOL success = ReadDirectoryChangesW(
				hDir,
				buffer,
				sizeof(buffer),
				FALSE, // 不递归
				FILE_NOTIFY_CHANGE_LAST_WRITE,
				&bytesReturned,
				nullptr,
				nullptr
		);

		if (!success) {
			DWORD err = GetLastError();
			std::wcerr << L"ReadDirectoryChangesW 失败，错误代码：" << err << std::endl;
			break; // 发生错误，退出循环
		}

		// 2. (关键修复) 只有当确实返回了数据时才处理
		if (bytesReturned > 0) {
			FILE_NOTIFY_INFORMATION *pNotify;
			size_t offset = 0;

			do {
				pNotify = (FILE_NOTIFY_INFORMATION *) ((BYTE *) buffer + offset);

				// FileNameLength 是以字节为单位的，需要转换为 WCHAR 的数量
				std::wstring changedFile(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));

				if (changedFile == fileName) {
					std::wcout << L"检测到文件修改：" << changedFile << std::endl;
					PostMessage(g_mainHwnd, WM_HOTKEY, HOTKEY_ID_REFRESH_SKIN, 0);
				}

				// 移动到下一个通知记录
				offset += pNotify->NextEntryOffset;
			} while (pNotify->NextEntryOffset != 0);
		}
	}

	CloseHandle(hDir);
}

/**
 * 打开一个控制台，用于显示调试信息
 */
inline void AttachConsoleForDebug() {
	AllocConsole();
	FILE *fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	std::wcout << "Console attached!" << std::endl;
}


inline void DoMyContextMenuAction(UINT cmd, int index, std::shared_ptr<RunCommandAction> &action) {
	switch (cmd) {
		case IDM_REMOVE_ITEM: {
			// 从 ListView 以及你的数据结构移除
//			ListView_DeleteItem(ListViewManager::hListView, index);
//			ListViewManager::filteredActions.erase(ListViewManager::filteredActions.begin() + index);
		}
			break;
		case IDM_RENAME_ITEM: {
			// A. 只改 ListView 的显示名：启动就地编辑
//			ListView_EditLabel(ListViewManager::hListView, index);
			// B. 如果你想真的重命名文件：
			//    建议在 LVN_ENDLABELEDIT 里拿到新名字，调用 MoveFileExW(old, new, 0)
		}
			break;
		case IDM_RUN_AS_ADMIN:
			action->InvokeWithTarget(nullptr, true);
			break;
		case IDM_OPEN_IN_CONSOLE:
			OpenConsoleHere(SaveGetShortcutTargetAndReturn(action->GetTargetPath()));
			break;
		case IDM_KILL_PROCESS: {
			std::wstring actualPath = GetShortcutTarget(action->GetTargetPath());
			if (systemProcesses.find(MyToLower(actualPath)) != systemProcesses.end()) {
				MessageBoxW(nullptr, (actualPath + L" 是系统关键进程，不建议终止。").c_str(),
							L"警告", MB_OK | MB_ICONWARNING);
			} else {
				int n = KillProcessByImagePath(actualPath);
				using namespace std::string_literals;
				Println(L"终止"s + std::to_wstring(n));
			}
		}
			break;
		case IDM_COPY_PATH:
			CopyTextToClipboard(g_mainHwnd, action->GetTargetPath());
			break;
		case IDM_COPY_TARGET_PATH: {
			std::wstring actualPath = GetShortcutTarget(action->GetTargetPath());
			CopyTextToClipboard(g_mainHwnd, actualPath);
		}
			break;
		default:
			break;
	}
}


static UINT ShowMyContextMenu(HWND hWnd, const std::wstring &path, POINT screenPt) {
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



#define ICON_COUNT 330
#define ICON_SIZE  16
#define ICON_SPACING 30   // 图标间距
#define ICONS_PER_ROW 20  // 每行多少个

static LRESULT CALLBACK Shell32IcoViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			SetBkMode(hdc, TRANSPARENT);   // 透明背景
			SetTextColor(hdc, RGB(0, 0, 0)); // 黑色文字

			int x = 0, y = 0;
			for (int i = 0; i < ICON_COUNT; i++)
			{
				HICON hIcon = LoadShellIcon(i, ICON_SIZE, ICON_SIZE);
				if (hIcon)
				{
					// 画图标
					DrawIconEx(hdc, x, y, hIcon, ICON_SIZE, ICON_SIZE, 0, NULL, DI_NORMAL);

					// 在图标下方显示序号
					wchar_t buf[16];
					swprintf(buf, 16, L"%d", i);
					TextOutW(hdc, x, y + ICON_SIZE + 2, buf, static_cast<int>(wcslen(buf)));
				}

				x += ICON_SPACING;
				if ((i + 1) % ICONS_PER_ROW == 0)
				{
					x = 0;
					y += ICON_SPACING + 12; // 多留一点高度放数字
				}
			}

			EndPaint(hwnd, &ps);
		}
			break;
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

/**
 * SHELL32 图标查看器
 * @param hInstance 
 */
static void ShowShell32IcoViewer(HINSTANCE hInstance)
{
	const wchar_t CLASS_NAME[] = L"ShellIconWindow";

	WNDCLASS wc = {};
	wc.lpfnWndProc = Shell32IcoViewerWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
			0,
			CLASS_NAME,
			L"Shell32.dll 330 icons",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 700, 800,
			NULL,
			NULL,
			hInstance,
			NULL
	);

	if (!hwnd) return ;

	ShowWindow(hwnd, SW_SHOW);
}
