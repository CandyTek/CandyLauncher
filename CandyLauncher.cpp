#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include "common/framework.h"
#include "common/Resource.h"
#include <vector>
#include <string>
#include <shlguid.h>
#include <objbase.h>
#include <commctrl.h>
#include <shlobj.h>
#include <sstream>
#include <filesystem>
#include <fstream>
#include "util/MainTools.hpp"
#include "view/GlassHelper.hpp"
#include "common/GlobalState.hpp"
#include "common/I18n.hpp"
#include "common/I18nResource.h"
#include <gdiplus.h>
#include <atomic>

#include "common/AppController.hpp"
//#include "util/DxgiUtils.h"
#include "manager/EditManager.hpp"
#include "manager/SkinHelper.hpp"
#include <Richedit.h>

#include "util/MyToastUtil.hpp"
#include "util/EditDropTarget.hpp"
#include "manager/ProcessManager.hpp"
#include "util/UpdateManager.hpp"
#include "view/CustomComboBox.hpp"
#include "window/Shell32IconViewer.hpp"

// 此代码模块中包含的函数的前向声明:
ATOM MainWindowRegisterClass(HINSTANCE hInstance);

void MainWindowInitInstance(HINSTANCE, int);

LRESULT CALLBACK MainWindowWndProc(HWND, UINT, WPARAM, LPARAM);

// 鼠标钩子回调函数，用于检测拖放操作
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

Gdiplus::GdiplusStartupInput gdiplusStartupInput;
inline ULONG_PTR gdiplusToken;
static ULONGLONG lastDragAndDropTime;
static UINT g_WM_TASKBARCREATED = 0;
static bool g_isMainWindowPinned = false;

static bool UpdateSettingItem(std::vector<SettingItem>& items, const std::string& key,
	const nlohmann::json& value, size_t& controlId, SettingItem** found = nullptr) {
	for (auto& item : items) {
		++controlId;
		if (item.key == key) {
			item.setValue(value);
			if (found) *found = &item;
			return true;
		}
		if ((item.type == "expand" || item.type == "expandswitch") &&
			UpdateSettingItem(item.children, key, value, controlId, found)) {
			return true;
		}
	}
	return false;
}

// 从主窗口菜单修改设置项：保存到用户配置，并同步到已打开的设置界面
static void SetMainWindowSetting(const char* key, const nlohmann::json& value) {
	nlohmann::json newConfig;
	newConfig[key] = value;
	saveConfigToFile(USER_SETTINGS_PATH, newConfig);
	g_settings_map[key].setValue(value);

	size_t controlId = 2999;
	UpdateSettingItem(g_settings_ui_last_save, key, value, controlId);
	if (g_settingsHwnd && IsWindow(g_settingsHwnd)) {
		controlId = 2999;
		SettingItem* item = nullptr;
		if (UpdateSettingItem(g_settings_ui, key, value, controlId, &item) && item) {
			if (HWND tab = FindTabHwndByIndex(item->subPageIndex)) {
				if (HWND control = GetDlgItem(tab, static_cast<int>(controlId))) {
					if (item->type == "list") {
						const auto it = std::find(item->entryValues.begin(), item->entryValues.end(), item->stringValue);
						if (it != item->entryValues.end()) {
							SendMessageW(control, CB_SETCURSEL, std::distance(item->entryValues.begin(), it), 0);
						}
					} else {
						SetSwitchState(control, item->boolValue);
					}
				}
			}
		}
	}
}

static void ShowMainWindowSystemMenu(HWND hWnd) {
	HMENU hSystemMenu = GetSystemMenu(hWnd, FALSE);
	if (!hSystemMenu) return;

	RECT windowRect{};
	if (!GetWindowRect(hWnd, &windowRect)) return;
	SetForegroundWindow(hWnd);

	const int menuCmd = TrackPopupMenu(
		hSystemMenu,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
		windowRect.left,
		windowRect.top,
		0,
		hWnd,
		nullptr
	);
	if (menuCmd != 0) {
		PostMessageW(hWnd, WM_SYSCOMMAND, static_cast<WPARAM>(menuCmd), 0);
	}
}

// 主进程函数
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					_In_opt_ HINSTANCE hPrevInstance,
					_In_ LPWSTR lpCmdLine,
					_In_ int nCmdShow) {
	const std::wstring executablePath = GetOwnExecutablePath();
	if (executablePath.empty()) {
		MessageBoxW(nullptr, L"无法获取程序路径。", L"CandyLauncher", MB_OK | MB_ICONERROR);
		return -1;
	}
	g_WM_SHOW_EXISTING_INSTANCE = RegisterWindowMessageW(L"CandyLauncher.ShowExistingInstance");
	g_instanceMutex = CreateMutexW(nullptr, FALSE, GetInstanceMutexName(executablePath).c_str());
	if (!g_instanceMutex) {
		MessageBoxW(nullptr, L"无法创建单实例互斥量。", L"CandyLauncher", MB_OK | MB_ICONERROR);
		return -1;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		CloseHandle(g_instanceMutex);
		g_instanceMutex = nullptr;
		ShowExistingInstance(executablePath);
		return 0;
	}

	// 启动快捷方式可能没有指定“起始位置”，先固定工作目录再加载配置和插件。
	if (!SetCurrentDirectoryW(EXE_FOLDER_PATH.c_str())) {
		MessageBoxW(nullptr, L"无法将工作目录设置为程序所在目录。", L"CandyLauncher", MB_OK | MB_ICONERROR);
		return -1;
	}

	static ULONGLONG g_appStartTick = GetTickCount64();
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) return -1;
	HRESULT hrOle = OleInitialize(nullptr);
	if (FAILED(hrOle)) {
		return -1;
	}

	g_hInst = hInstance;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
#if !defined(__MINGW32__)
	isToastAvailable = InitializeWinToast();
#endif
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	if (needOpenDebugCmd) AttachConsoleForDebug();
	PinyinHelper::initPinyinLib(EXE_FOLDER_PATH);
	InitializeCustomButtonResources();
	MainWindowRegisterClass(hInstance);
	SettingWindowRegisterClass(hInstance);
	if (needOpenShell32IconViewer) ShowShell32IcoViewer(hInstance);
	MainWindowInitInstance(hInstance, nCmdShow);
	if (needOpenSettingWindow) ShowSettingsWindow(hInstance, nullptr, true);
	if (needMinimizeSettingWindow) ShowWindow(g_settingsHwnd, SW_MINIMIZE);
	g_skinFileWatcherThread = std::thread(watchSkinFile);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CANDYLAUNCHER));
	refreshSkin(g_currectSkinFilePath, !g_settings_map["pref_hide_window_after_run"].boolValue);
	APP_STARTUP_TIME = GetTickCount64() - g_appStartTick; // 记录程序耗费时
	g_WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
	Logi(L"WinMain", L"程序初始化完成");
	AppUpdate::ScheduleStartupUpdateCheck();
	// MyShowSimpleToast(L"CandyLauncher 已启动", L"程序初始化完成,按 Alt+K 呼出启动器");

	MSG msg;

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	// 程序退出前通知线程停止
	stopThreadSkinFileWatcher();
	CleanupButtonResources();
	CleanupComboBoxResources();
	CoUninitialize();
	return static_cast<int>(msg.wParam);
}

// 注册窗口类。
ATOM MainWindowRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);
	// 确保窗口大小发生变化时，整个客户区都会被重绘
	// wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWindowWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CANDYLAUNCHER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"CandyLauncherClass";
	wcex.hbrBackground = nullptr;

	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_CANDYLAUNCHER));

	return RegisterClassExW(&wcex);
}

static void CreateMainWindow(HINSTANCE hInstance, const int nCmdShow) {
	unsigned long dw_style;
	bool isShow = !g_settings_map["pref_hide_window_after_run"].boolValue;
	dw_style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX;
	long dw_ex_style = WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
	if (g_settings_map["pref_window_always_on_top"].boolValue) {
		dw_ex_style |= WS_EX_TOPMOST;
	}
	if (g_settings_map["pref_window_mouse_penetration"].boolValue) {
		dw_ex_style |= WS_EX_TRANSPARENT;
	}
	// 从注册表读取上次窗口关闭的位置
	const RECT lastWindowPosition = LoadWindowRectFromRegistry();
	last_open_window_position_x = lastWindowPosition.left;
	last_open_window_position_y = lastWindowPosition.top;
	if (!IsPointOnAnyMonitor(last_open_window_position_x, last_open_window_position_y)) {
		const RECT tempRc = getWindowRectMainWindowInCursorScreen();
		last_open_window_position_x = tempRc.left;
		last_open_window_position_y = tempRc.top;
	}

	g_mainHwnd = CreateWindowExW(
		// WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_TRANSPARENT | WS_EX_LAYERED, // 扩展样式
		// WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, // 扩展样式
		dw_ex_style,
		L"CandyLauncherClass",
		LoadI18nString(IDS_I18N_MAIN_WINDOW_TITLE, L"CandyLauncher").c_str(),
		// WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		dw_style,
		// WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_BORDER,
		// WS_POPUPWINDOW | WS_VISIBLE | WS_CAPTION | WS_BORDER,
		last_open_window_position_x, last_open_window_position_y, // 初始位置
		MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT,
		nullptr, // 父窗口
		nullptr, // 菜单
		hInstance, // 实例句柄
		nullptr // 附加参数
	);
	if (!g_mainHwnd) {
		ShowErrorMsgBox(L"创建窗口失败，hWnd为空");
		ExitProcess(1);
		return;
	}
	if (isShow) {
		ShowMainWindowSimple();
		g_pluginManager->OnMainWindowShowNotifi(true);
	} else {
		ShowWindow(g_mainHwnd, SW_HIDE);
	}
}

static void UninstallMouseHook();

// 拖放到搜索框后，取消窗口失焦时的延迟隐藏
static void OnEditDropReceived() {
	KillTimer(g_mainHwnd, TIMER_DELAY_HIDE_WINDOW);
	lastDragAndDropTime = GetTickCount64();
	UninstallMouseHook();
	SetForegroundWindow(g_mainHwnd);
}

static void InitMainWindowControls(HINSTANCE hInstance, HWND hWnd) {
	// 编辑框（搜索框）
	g_editHwnd = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
								10, 10, 580, 35, hWnd, reinterpret_cast<HMENU>(1), hInstance, nullptr);
	EditManager::EnableSmartEdit(g_editHwnd);
	// EDIT 只支持 WM_DROPFILES，这里注册 OLE 拖放目标以接受文本（文件保持原有的参数行为）
	RegisterEditDropTarget(g_editHwnd,
							[](const std::wstring& text, const POINT clientPt) {
								OnEditDropReceived();
								InsertTextIntoEditAtPoint(g_editHwnd, text, clientPt);
							},
							[](const std::vector<std::wstring>& files) {
								OnEditDropReceived();
								ChangeEditTextArg(files.front());
							});
	SendMessageW(g_editHwnd, EM_SETCUEBANNER, TRUE,
				reinterpret_cast<LPARAM>(utf8_to_wide(
					g_settings_map["pref_search_box_placeholder"].stringValue).c_str()));

	listViewInitialize(hWnd, hInstance, 10, 45, 580, 380);
	appLaunchActionCallBacks = getAppLaunchActionCallBacks();
	refreshPluginRunner();
	EditManager::Attach(g_editHwnd, 0);
	SetWindowSubclass(g_listViewHwnd, ListViewSubclassProc, 2, 0);
	// 不要使用透明，和主窗口的透明起冲突了
	// SetWindowLong(hEdit, GWL_EXSTYLE,GetWindowLong(hEdit, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
	//ListView_SetBkColor(g_listViewHwnd, COLOR_UI_BG);
	//ListView_SetTextBkColor(g_listViewHwnd, COLOR_UI_BG);

#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG) || defined(REL_WITH_DEB_INFO_DEBUG) 
	// 用于测试，程序启动键入词条
	SetTimer(hWnd, 1001, 1500, [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
		KillTimer(hwnd, id);
		if (g_editHwnd && IsWindow(g_editHwnd)) {
			// SetWindowText(g_editHwnd, L"a");
		}
	});
#endif

}


// 创建主窗口，初始化
void MainWindowInitInstance(HINSTANCE hInstance, const int nCmdShow) {
	// 初始化插件系统
	if (!g_pluginManager) {
		g_pluginManager = std::make_unique<PluginManager>();
		// 暂时禁用回调避免循环调用导致崩溃
		// g_pluginManager->SetActionsChangedCallback([]() {
		//     refreshPluginRunner();
		// });
		std::wstring pluginDir = (EXE_FOLDER_PATH + L"\\plugins");
		g_pluginManager->LoadAllPlugins(pluginDir);
	}

	LoadSettingList();

	CreateMainWindow(hInstance, nCmdShow);
	InitMainWindowControls(hInstance, g_mainHwnd);
	g_pluginManager->SetBeforePluginUnloadCallback([]() {
		clearVisibleActionReferences();
	});
	g_pluginManager->SetActionsChangedCallback([]() {
		refreshVisibleActionsFromCurrentInput();
	});
	Init(g_mainHwnd, hInstance);
	userSettingsAfterTheAppStart();
	g_pluginManager->NotifyUserSettingsLoadDone();
	g_pluginManager->RefreshAllActions();

	// 另一种指定透明效果，但是并不太行，有很严重的锯齿，而且没有透明度概念，很生硬
	// SetLayeredWindowAttributes(s_mainHwnd, RGB(80, 81, 82), 0, LWA_COLORKEY);

	DWM_BLURBEHIND db{};
	db.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
	db.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
	db.fEnable = TRUE;
	DwmEnableBlurBehindWindow(g_mainHwnd, &db);
	// DwmEnableBlurBehindWindow(ListViewManager::hListView, &db);
	// DwmEnableBlurBehindWindow(hEdit, &db);
	// Println(L"Windows inited.");
}

// 安装鼠标钩子，用于监听鼠标释放
static void InstallMouseHook() {
	if (g_mouseHook == nullptr) {
		g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, g_hInst, 0);
	}
}

// 卸载鼠标钩子
static void UninstallMouseHook() {
	if (g_mouseHook != nullptr) {
		UnhookWindowsHookEx(g_mouseHook);
		g_mouseHook = nullptr;
	}
}

// 鼠标钩子回调函数
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		if (wParam == WM_LBUTTONUP) {
			// 鼠标左键释放，启动延迟定时器检查是否收到拖放事件
			UninstallMouseHook();
			SetTimer(g_mainHwnd, TIMER_DELAY_HIDE_WINDOW, 100, nullptr);
		}
	}
	return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// 处理主窗口的消息。
LRESULT CALLBACK MainWindowWndProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam) {
	switch (message) {
	case WM_COMMAND:
		{
			HWND hCtrl = (HWND)lParam;
			const int wmId = LOWORD(wParam);
			const int wmEvent = HIWORD(wParam);

			// 处理编辑框输入改变
			if (hCtrl == g_editHwnd && wmEvent == EN_CHANGE) {
				editTextInput();
			} else if (wmId > TRAY_MENU_ID_BASE && wmId < TRAY_MENU_ID_BASE_END) {
				if (wmId == TRAY_MENU_ID_PIN_THIS_TIME) {
					g_isMainWindowPinned = !g_isMainWindowPinned;
					if (g_isMainWindowPinned) {
						KillTimer(hWnd, TIMER_DELAY_HIDE_WINDOW);
						KillTimer(hWnd, TIMER_DETERMINE_FILE_DRAG_AND_DROP);
						UninstallMouseHook();
					}
				} else if (wmId == TRAY_MENU_ID_ALWAYS_ON_TOP) {
					const bool value = !g_settings_map["pref_window_always_on_top"].boolValue;
					SetMainWindowSetting("pref_window_always_on_top", value);
					SetWindowPos(hWnd, value ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
						SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				} else if (wmId == TRAY_MENU_ID_LOCK_WINDOW_POSITION) {
					pref_lock_window_popup_position = !pref_lock_window_popup_position;
					SetMainWindowSetting("pref_lock_window_popup_position", pref_lock_window_popup_position);
				} else if (wmId == TRAY_MENU_ID_CLOSE_AFTER_OPEN_ITEM) {
					pref_close_after_open_item = !pref_close_after_open_item;
					SetMainWindowSetting("pref_close_after_open_item", pref_close_after_open_item);
				} else if (wmId >= TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE && wmId < TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_END) {
					const auto& entryValues = g_settings_map["pref_close_on_dismiss_focus"].entryValues;
					const size_t index = static_cast<size_t>(wmId - TRAY_MENU_ID_CLOSE_ON_DISMISS_FOCUS_BASE);
					if (index < entryValues.size()) {
						SetMainWindowSetting("pref_close_on_dismiss_focus", entryValues[index]);
					}
				} else {
					TrayMenuClick(wmId);
				}
			}
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			if (const int result = MainWindowCustomPaint(ps, hdc); result >= 0) return result;
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DRAWITEM:
		{
			const tagDRAWITEMSTRUCT* lpDrawItem = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);

			if (lpDrawItem->CtlID == 2) // 控件 ID，确保正确
			{
				listViewDrawItem(lpDrawItem);
				return TRUE;
			}
		}
		break;
	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT* lpMeasureItem = reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam);
			if (lpMeasureItem->CtlID == 2) {
				listViewMeasureItem(lpMeasureItem);
				return TRUE;
			}
		}
		break;

	case WM_HOTKEY:
		{
			if (const int result = mainWindowHotkey(wParam); result >= 0) return result;
		}
		break;
	case WM_KEYDOWN:
		{
			// 不实现按键事件，因为焦点总是会给到编辑框
			break;
		}
	case WM_ERASEBKGND:
		{
			return 1; // Return non-zero to indicate you have handled erasing the background
		}
	case WM_SETFOCUS:
		{
			TimerIDSetFocusEdit = SetTimer(hWnd, TIMER_SETFOCUS_EDIT, 10, nullptr); // 10 毫秒延迟
			break;
		}
	case WM_NCHITTEST:
		{
			if (!pref_lock_window_popup_position) {
				LRESULT hit = DefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam);
				if (hit == HTCLIENT) {
					return HTCAPTION;
				}
				return hit;
			}
		}
		break;
	// 常用区域分割线
	case WM_ACTIVATE:
		{
			// 如果是 WA_INACTIVE，说明窗口从激活变为非激活状态（失去焦点）
			if (LOWORD(wParam) == WA_INACTIVE) {
				if (hWnd == g_mainHwnd) {
					if (g_isMainWindowPinned) break;
					if (g_isOleFileDragDropInProgress) break;
					// 设置界面在皮肤tab时，暂停随焦点消失关闭功能（皮肤预览需要主窗口保持可见）
					bool isSettingsSkinTabActive = g_settingsHwnd != nullptr
						&& static_cast<size_t>(currentSubPageIndex) < subPageTabs.size()
						&& subPageTabs[currentSubPageIndex] == "skin";
					if (isSettingsSkinTabActive) break;

					std::string closeMode = g_settings_map["pref_close_on_dismiss_focus"].stringValue;
					if (closeMode == "close_immediate") {
						HideWindow();
					} else if (closeMode == "allow_drag_file") {
						SetTimer(g_mainHwnd, TIMER_DETERMINE_FILE_DRAG_AND_DROP, 50, nullptr);
					}
				}
			}
		}
		break;
	case WM_EDIT_CONTROL_HOTKEY:
		{
			editControlHotkey(wParam);
			return 0;
		}
	case WM_CONFIG_SAVED:
		{
			std::thread t([]() {
				doPrefChanged();
			});
			t.detach();
			return 0;
		}
	case WM_NOTIFY:
		{
			LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
			if (pnmh->hwndFrom == g_listViewHwnd) {
				switch (pnmh->code) {
				case NM_CUSTOMDRAW:
					{
						std::string bgColor;
						if (g_skinJson != nullptr) {
							bgColor = g_skinJson.value("listview_bg_color", "");
						}
						if (g_listViewBgImage != nullptr || !bgColor.empty()) {
							// Forward to our manager and return the result. WM_NOTIFY is
							// not a dialog message, so DWLP_MSGRESULT would be ignored.
							LPNMLVCUSTOMDRAW lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
							return listViewOnCustomDraw(lplvcd);
						}
					}
					break;
				case LVN_ITEMCHANGED:
					listViewOnItemChanged(reinterpret_cast<const NMLISTVIEW*>(lParam));
					break;
				case LVN_GETDISPINFO:
					{
						// 这就是 ListView 在向我们请求数据
						NMLVDISPINFO* pdi = reinterpret_cast<NMLVDISPINFOW*>(lParam);
						listViewCustomDataOnGetDispInfo(pdi); // 把请求转发给 ListViewManager 处理
						return 0; // 已处理
					}
				case NM_DBLCLK:
				case NM_CLICK:
					{
						if (pref_single_click_to_open && pnmh->code==NM_CLICK || !pref_single_click_to_open && pnmh->code==NM_DBLCLK) {
							LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
							int index = pia->iItem;
							if (index != -1 && filteredActions.size() > static_cast<size_t>(index)) {
								std::shared_ptr<BaseAction>& it = filteredActions[index];
								const bool needHide = PluginManager::DispatchActionExecute(it, currectActionArg);
								if (pref_close_after_open_item && needHide)
									HideWindow();
							}
							return TRUE;
						}
						PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
					}
					break;
				case NM_RCLICK:
					{
						LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
						POINT pt;
						// 获取鼠标坐标（相对 ListView 客户区）
						GetCursorPos(&pt);

						// 获取当前点击项
						int index = pia ? pia->iItem : -1;
						if (index == -1) {
							LVHITTESTINFO lvhti = {};
							POINT ptClient = pt;
							ScreenToClient(g_listViewHwnd, &ptClient);
							lvhti.pt = ptClient;
							index = ListView_HitTest(g_listViewHwnd, &lvhti);
						}
						if (index == -1) {
							index = ListView_GetNextItem(g_listViewHwnd, -1, LVNI_SELECTED);
						}
						Logi(L"RightClick",
							L"NM_RCLICK index=", index,
							L", actions=", filteredActions.size(),
							L", piaItem=", (pia ? pia->iItem : -1));
						if (index != -1 && filteredActions.size() > static_cast<size_t>(index)) {
							// 选中当前项（可选）
							// ListView_SetItemState(hListView, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
							std::shared_ptr<BaseAction>& it = filteredActions[index];
							const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
							bool handled = false;
							if (shiftDown ^ pref_switch_list_right_click_with_shift_right_click) {
								if (g_pluginManager) handled = g_pluginManager->DispatchItemShiftRightClick(it, hWnd, pt);
							} else {
								if (g_pluginManager) handled = g_pluginManager->DispatchItemRightClick(it, hWnd, pt);
							}
							Logi(L"RightClick",
								L"dispatch handled=", handled,
								L", shift=", shiftDown,
								L", switchPref=", pref_switch_list_right_click_with_shift_right_click,
								L", pluginId=", (it ? it->pluginId : 65535));
						} else {
							Logi(L"RightClick", L"skip dispatch: invalid index");
						}
						PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
						return TRUE;
					}
					break;

				case LVN_BEGINDRAG:
					{
						auto* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);

						const int itemIndex = nmlv->iItem;
						if (itemIndex < 0 || itemIndex >= static_cast<int>(filteredActions.size())) {
							return 0;
						}

						POINT screenPt = nmlv->ptAction;
						ClientToScreen(nmlv->hdr.hwndFrom, &screenPt);

						const auto& action = filteredActions[itemIndex];

						bool handled = false;
						if (g_pluginManager) {
							handled = g_pluginManager->DispatchItemBeginDrag(
								action,
								nmlv->hdr.hwndFrom,
								screenPt
							);
						}

						if (!handled) {
							// 插件/继承类未处理拖拽时，不实现功能
							return 0;
						}

						return 0;
					}
					default: break;
				}
			}
			break;
		}
	case WM_TIMER:
		{
			if (wParam == TIMER_SETFOCUS_EDIT) {
				if (TimerIDSetFocusEdit != 0) {
					KillTimer(hWnd, TimerIDSetFocusEdit);
				}
				SetFocus(g_editHwnd);
			} else if (wParam == TIMER_SET_GLOBAL_HOTKEY) {
				KillTimer(hWnd, TIMER_SET_GLOBAL_HOTKEY);
				ConfigureMainPanelToggleHotkey(
					g_mainHwnd,
					pref_hotkey_toggle_main_panel_mode,
					pref_hotkey_toggle_main_panel
				);
			} else if (wParam == TIMER_SHOW_WINDOW) {
				KillTimer(hWnd, TIMER_SHOW_WINDOW);
				ShowMainWindowSimple();
			} else if (wParam == TIMER_DELAY_HIDE_WINDOW) {
				KillTimer(hWnd, TIMER_DELAY_HIDE_WINDOW);
				// 延迟后如果没有收到拖放事件，则隐藏窗口
				if (!g_isMainWindowPinned && GetTickCount64() - lastDragAndDropTime >= 70) {
					HideWindow();
				}
			} else if (wParam == TIMER_DETERMINE_FILE_DRAG_AND_DROP) {
				KillTimer(hWnd, TIMER_DETERMINE_FILE_DRAG_AND_DROP);
				if (g_isMainWindowPinned) break;
				// 检查鼠标左键是否按下（正在拖动）
				if (GetKeyState(VK_LBUTTON) & 0x8000) {
					// 正在拖动，安装钩子等待释放
					InstallMouseHook();
				} else {
					// 没有拖动，直接隐藏
					HideWindow();
				}
			} else if (wParam == TIMER_RELOAD_SKIN_FILE) {
				KillTimer(hWnd, TIMER_RELOAD_SKIN_FILE);
				if (refreshSkin(g_currectSkinFilePath)) {
					g_skinFileReloadRetries = 0;
				} else if (++g_skinFileReloadRetries < SKIN_FILE_RELOAD_MAX_RETRIES) {
					// 文件可能仍在写入中（空内容或被占用），稍后重试
					SetTimer(hWnd, TIMER_RELOAD_SKIN_FILE, SKIN_FILE_RELOAD_DELAY_MS, nullptr);
				} else {
					g_skinFileReloadRetries = 0;
				}
			}
		}
		break;
	case WM_FOCUS_EDIT: SetFocus(g_editHwnd);
		break;
	case WM_SHOWWINDOW:
		if (wParam==FALSE) {
			g_isMainWindowPinned = false;
			g_pluginManager->OnMainWindowShowNotifi(false);
		}
		break;
	case WM_APP_UPDATE_AVAILABLE:
		return AppUpdate::HandleUpdateAvailableMessage(lParam);
	case WM_APP_UPDATE_ERROR:
		return AppUpdate::HandleUpdateErrorMessage(lParam);
	case WM_APP_UPDATE_DOWNLOAD_FINISHED:
		return AppUpdate::HandleUpdateDownloadFinishedMessage(lParam);
	case WM_DROPFILES:
		{
			// 收到拖放事件，取消延迟隐藏
			KillTimer(hWnd, TIMER_DELAY_HIDE_WINDOW);
			lastDragAndDropTime = GetTickCount64();
			UninstallMouseHook();
			const HDROP hDrop = (HDROP)wParam;
			if (DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0) > 0) {
				const UINT pathLen = DragQueryFile(hDrop, 0, NULL, 0) + 1;
				std::vector<wchar_t> filePath(pathLen);
				DragQueryFile(hDrop, 0, filePath.data(), pathLen);
				ChangeEditTextArg(filePath.data());
			}
			DragFinish(hDrop);
		}
		break;
	case WM_REFRESH_SKIN:
		{
			if (wParam == 1) {
				// 来自皮肤文件监听：重置计时器，等文件写入稳定后再加载
				g_skinFileReloadRetries = 0;
				SetTimer(hWnd, TIMER_RELOAD_SKIN_FILE, SKIN_FILE_RELOAD_DELAY_MS, nullptr);
			} else {
				refreshSkin(g_currectSkinFilePath);
			}
			return 0;
		}
	case WM_CLOSE:
		{
			HideWindow();
			return 0;
		}
	case WM_SYSCOMMAND:
		{
			if ((wParam & 0xFFF0) == SC_MINIMIZE) {
				// 阻止默认最小化行为
				HideWindow();
				return 0;
			}
		}
		break;
	case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED) {
				// 阻止默认最小化行为
				HideWindow();
				return 0;
			}
		}
		break;
	case WM_SYSCHAR:
	case WM_SYSKEYUP:
		{
			// 阻止 ALT 等系统键造成的副作用
			return 0;
		}
	case WM_SYSKEYDOWN:
		{
			if (wParam == VK_SPACE && (lParam & (1 << 29))) {
				ShowMainWindowSystemMenu(hWnd);
				return 0;
			}
			// 阻止 ALT 等系统键造成的副作用
			return 0;
		}
	case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
	case WM_DESTROY:
		{
			// 程序退出时释放 GDI+ 资源
			clearBackgroundCachedBitmaps();
			UnregisterMainPanelToggleHotkey(hWnd);
			RevokeDragDrop(g_editHwnd);
			UninstallMouseHook();
			SaveWindowRectToRegistry(hWnd);
			ListView_DeleteAllItems(g_listViewHwnd);
			listViewCleanup();

			// 关闭插件系统v
			g_pluginManager->UnloadAllPlugins();
			g_pluginManager.reset();

			TrayMenuDestroy();
			if (g_instanceMutex) {
				CloseHandle(g_instanceMutex);
				g_instanceMutex = nullptr;
			}
			if (g_restartRequested) {
				const std::wstring executablePath = GetOwnExecutablePath();
				if (!executablePath.empty()) {
					ShellExecuteW(nullptr, L"open", executablePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				}
			}
			PostQuitMessage(0);
			ExitProcess(0);
		}
		break;
	case WM_RBUTTONUP:
	case WM_NCRBUTTONUP:
		{
			const SettingItem& dismissFocus = g_settings_map["pref_close_on_dismiss_focus"];
			MainWindowMenuOptions options{};
			options.pinned = g_isMainWindowPinned;
			// 只有失去焦点时会隐藏窗口的模式下，“本次钉住”才有意义
			options.showPinOption = dismissFocus.stringValue == "close_immediate"
				|| dismissFocus.stringValue == "allow_drag_file";
			options.alwaysOnTop = g_settings_map["pref_window_always_on_top"].boolValue;
			options.lockWindowPosition = pref_lock_window_popup_position;
			options.closeAfterOpenItem = pref_close_after_open_item;
			options.closeOnDismissFocusTitle = utf8_to_wide(dismissFocus.title);
			options.closeOnDismissFocusIndex = -1;
			for (size_t i = 0; i < dismissFocus.entries.size() && i < dismissFocus.entryValues.size(); ++i) {
				options.closeOnDismissFocusEntries.push_back(utf8_to_wide(dismissFocus.entries[i]));
				if (dismissFocus.entryValues[i] == dismissFocus.stringValue) {
					options.closeOnDismissFocusIndex = static_cast<int>(i);
				}
			}
			TrayMenuShow(hWnd, &options);
			return 0;
		}
	case WM_TRAYICON:
		{
			if (lParam == WM_RBUTTONUP) {
				TrayMenuShow(hWnd);
			}
		}
		break;

	default:
		if (g_WM_SHOW_EXISTING_INSTANCE && message == g_WM_SHOW_EXISTING_INSTANCE) {
			ShowMainWindowSimple();
			return 0;
		}
		if (g_WM_TASKBARCREATED && message == g_WM_TASKBARCREATED) {
			ShowTrayIcon();
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	// return 0;
	return DefWindowProc(hWnd, message, wParam, lParam);
}
