#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include "framework.h"
#include "WindowsProject1.h"
#include "Resource.h"
#include <vector>
#include <string>
#include <shlguid.h>
#include <objbase.h>
#include <commctrl.h>
#include <shlobj.h>
#include <sstream>
#include <filesystem>
#include <fstream>
#include "MainTools.hpp"
#include "GlassHelper.hpp"
#include "DataKeeper.hpp"
#include <gdiplus.h>
#include <atomic>

#include "AppTools.hpp"
#include "DxgiUtils.h"
#include "EditHelper.hpp"
#include "SkinHelper.h"
#include <Richedit.h>
#include "IndexedManager.hpp"
#include "PluginRunningApps.hpp"
#include "Shell32IcoViewer.hpp"

//    #include "cli.h"


// 此代码模块中包含的函数的前向声明:
ATOM MainWindowRegisterClass(HINSTANCE hInstance);

void MainWindowInitInstance(HINSTANCE, int);

LRESULT CALLBACK MainWindowWndProc(HWND, UINT, WPARAM, LPARAM);

Gdiplus::GdiplusStartupInput gdiplusStartupInput;
inline ULONG_PTR gdiplusToken;

// 主进程函数
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					_In_opt_ HINSTANCE hPrevInstance,
					_In_ LPWSTR lpCmdLine,
					_In_ int nCmdShow)
{
	static ULONGLONG g_appStartTick = GetTickCount64();
	g_hInst = hInstance;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	if (needOpenDebugCmd) AttachConsoleForDebug();
	InitializeCustomButtonResources();

	MainWindowRegisterClass(hInstance);
	SettingWindowRegisterClass(hInstance);
	if (needOpenShell32IconViewer) ShowShell32IcoViewer(hInstance);
	MainWindowInitInstance(hInstance, nCmdShow);
	if (needOpenSettingWindow) ShowSettingsWindow(hInstance, nullptr);
	if (needOpenIndexedManager) ShowIndexedManagerWindow(g_settingsHwnd);
	g_skinFileWatcherThread = std::thread(watchSkinFile);
	g_pluginIndexedRunningAppsThread = std::thread(WorkerThreadFunction);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));
	refreshSkin(g_currectSkinFilePath, !g_settings_map["pref_hide_window_after_run"].boolValue);
	APP_STARTUP_TIME = GetTickCount64() - g_appStartTick; // 记录程序耗费时

	MSG msg;

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	// 程序退出前通知线程停止
	stopThreadPluginRunningApps();
	stopThreadSkinFileWatcher();
	CleanupButtonResources();
	return static_cast<int>(msg.wParam);
}

// 注册窗口类。
ATOM MainWindowRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);
	// 确保窗口大小发生变化时，整个客户区都会被重绘
	// wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWindowWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"CandyLauncherClass";
	wcex.hbrBackground = nullptr;

	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

static void CreateMainWindow(HINSTANCE hInstance, const int nCmdShow)
{
	unsigned long dw_style;
	bool isShow = !g_settings_map["pref_hide_window_after_run"].boolValue;
	dw_style = WS_POPUP;
	long dw_ex_style = WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
	if (g_settings_map["pref_window_always_on_top"].boolValue)
	{
		dw_ex_style |= WS_EX_TOPMOST;
	}
	if (g_settings_map["pref_window_mouse_penetration"].boolValue)
	{
		dw_ex_style |= WS_EX_TRANSPARENT;
	}
	// 从注册表读取上次窗口关闭的位置
	const RECT lastWindowPosition = LoadWindowRectFromRegistry();
	last_open_window_position_x = lastWindowPosition.left;
	last_open_window_position_y = lastWindowPosition.top;
	if (!IsPointOnAnyMonitor(last_open_window_position_x, last_open_window_position_y))
	{
		const RECT tempRc = getWindowRectMainWindowInCursorScreen();
		last_open_window_position_x = tempRc.left;
		last_open_window_position_y = tempRc.top;
	}

	g_mainHwnd = CreateWindowExW(
		// WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_TRANSPARENT | WS_EX_LAYERED, // 扩展样式
		// WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, // 扩展样式
		dw_ex_style,
		L"CandyLauncherClass",
		L"CandyLauncher",
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
	if (!g_mainHwnd)
	{
		ShowErrorMsgBox(L"创建窗口失败，hWnd为空");
		ExitProcess(1);
		return;
	}
	if (isShow)
	{
		UserShowMainWindowSimple();
	}
	else
	{
		ShowWindow(g_mainHwnd, SW_HIDE);
	}
}

static void InitMainWindowControls(HINSTANCE hInstance, HWND hWnd)
{
	// 编辑框（搜索框）
	g_editHwnd = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
								10, 10, 580, 35, hWnd, reinterpret_cast<HMENU>(1), hInstance, nullptr);
	SendMessageW(g_editHwnd, EM_SETCUEBANNER, TRUE,
				reinterpret_cast<LPARAM>(utf8_to_wide(
					g_settings_map["pref_search_box_placeholder"].stringValue).c_str()));

	listViewInitialize(hWnd, hInstance, 10, 45, 580, 380);
	appLaunchActionCallBacks = getAppLaunchActionCallBacks();
	refreshPluginRunner();
	EditHelper::Attach(g_editHwnd, 0);
	SetWindowSubclass(g_listViewHwnd, ListViewSubclassProc, 2, 0);
	// 不要使用透明，和主窗口的透明起冲突了
	// SetWindowLong(hEdit, GWL_EXSTYLE,GetWindowLong(hEdit, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
	//ListView_SetBkColor(g_listViewHwnd, COLOR_UI_BG);
	//ListView_SetTextBkColor(g_listViewHwnd, COLOR_UI_BG);

	// 不知道怎么回事，现在需要下面这两行才能正确使用ctrl+bs了，不知道是哪个地方干扰了
	SetFocus(g_editHwnd);
	EditHelper::EnableSmartEdit(g_editHwnd);
	//SetWindowText(g_editHwnd, L"ffffffffffffffffffffffffffffffff");
}


// 创建主窗口，初始化
void MainWindowInitInstance(HINSTANCE hInstance, const int nCmdShow)
{
	LoadSettingList();
	CreateMainWindow(hInstance, nCmdShow);
	InitMainWindowControls(hInstance, g_mainHwnd);
	TrayMenuManager::Init(g_mainHwnd, hInstance);
	userSettingsAfterTheAppStart();

	// 另一种指定透明效果，但是并不太行，有很严重的锯齿，而且没有透明度概念，很生硬
	// SetLayeredWindowAttributes(s_mainHwnd, RGB(80, 81, 82), 0, LWA_COLORKEY);

	DWM_BLURBEHIND db{};
	db.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
	db.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
	db.fEnable = TRUE;
	DwmEnableBlurBehindWindow(g_mainHwnd, &db);
	// DwmEnableBlurBehindWindow(ListViewManager::hListView, &db);
	// DwmEnableBlurBehindWindow(hEdit, &db);

	Println(L"Windows inited.");
}

// 处理主窗口的消息。
LRESULT CALLBACK MainWindowWndProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		{
			HWND hCtrl = (HWND)lParam;
			const int wmId = LOWORD(wParam);
			const int wmEvent = HIWORD(wParam);

			// 处理编辑框输入改变
			if (hCtrl == g_editHwnd && wmEvent == EN_CHANGE)
			{
				editTextInput();
			}
			else if (wmId > TRAY_MENU_ID_BASE && wmId < TRAY_MENU_ID_BASE_END)
			{
				TrayMenuManager::TrayMenuClick(wmId, hWnd, g_editHwnd);
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
			if (lpMeasureItem->CtlID == 2)
			{
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
			if (!pref_lock_window_popup_position)
			{
				LRESULT hit = DefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam);
				if (hit == HTCLIENT)
				{
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
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				if (g_settings_map["pref_close_on_dismiss_focus"].stringValue == "close_immediate")
					HideWindow();
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
			std::thread t([]()
			{
				doPrefChanged();
			});
			t.detach();
			return 0;
		}
	case WM_NOTIFY:
		{
			LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
			if (pnmh->hwndFrom == g_listViewHwnd)
			{
				switch (pnmh->code)
				{
				case NM_CUSTOMDRAW:
					{
						std::string bgColor;
						if (g_skinJson != nullptr)
						{
							bgColor = g_skinJson.value("listview_bg_color", "");
						}
						if (g_listViewBgImage != nullptr || !bgColor.empty())
						{
							// Forward to our manager and return the result
							LPNMLVCUSTOMDRAW lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
							SetWindowLongPtr(hWnd, DWLP_MSGRESULT, listViewOnCustomDraw(lplvcd));
							// return TRUE;
						}
					}
					break;
				case LVN_GETDISPINFO:
					{
						// 这就是 ListView 在向我们请求数据
						NMLVDISPINFO* pdi = reinterpret_cast<NMLVDISPINFOW*>(lParam);
						listViewCustomDataOnGetDispInfo(pdi); // 把请求转发给 ListViewManager 处理
						return 0; // 已处理
					}
				case NM_CLICK:
					{
						if (pref_single_click_to_open)
						{
							LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
							int index = pia->iItem;
							if (index != -1 && filteredActions.size() > static_cast<size_t>(index))
							{
								const std::shared_ptr<RunCommandAction>& it = filteredActions[index];
								if (pref_close_after_open_item)
									HideWindow();
								it->Invoke();
							}
							return TRUE;
						}
						PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
					}
					break;
				case NM_DBLCLK:
					{
						if (!pref_single_click_to_open) // 双击
						{
							LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
							int index = pia->iItem;
							if (index != -1 && filteredActions.size() > static_cast<size_t>(index))
							{
								const std::shared_ptr<RunCommandAction> it = filteredActions[index];
								if (pref_close_after_open_item)
									HideWindow();
								it->Invoke();
								// 你可以通过 index 找到 filteredActions[index]
								// 比如执行该 action
								// ExecuteAction(filteredActions[index]);
							}
							return TRUE;
						}
					}
					break;
				case NM_RCLICK:
					{
						POINT pt;
						// 获取鼠标坐标（相对 ListView 客户区）
						GetCursorPos(&pt);

						// 获取当前点击项
						LVHITTESTINFO lvhti = {};
						POINT ptClient = pt;
						ScreenToClient(g_listViewHwnd, &ptClient);
						lvhti.pt = ptClient;
						int index = ListView_HitTest(g_listViewHwnd, &lvhti);
						if (index != -1)
						{
							// 选中当前项（可选）
							// ListView_SetItemState(hListView, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
							std::shared_ptr<RunCommandAction>& it = filteredActions[index];
							const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
							if (shiftDown ^ pref_switch_list_right_click_with_shift_right_click)
							{
								UINT cmd = ShowMyContextMenu(hWnd, it->GetTargetPath(), pt);
								DoMyContextMenuAction(cmd, index, it);
							}
							else
							{
								// 非 Shift：走你原本的 Shell 右键菜单
								ShowShellContextMenu(hWnd, it->GetTargetPath(), pt);
							}
						}
						PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
						return TRUE;
					}
					break;
				default:
					break;
				}
			}
			break;
		}
	case WM_TIMER:
		if (wParam == TIMER_SETFOCUS_EDIT)
		{
			if (TimerIDSetFocusEdit != 0)
			{
				KillTimer(hWnd, TimerIDSetFocusEdit);
			}
			SetFocus(g_editHwnd);
		}
		else if (wParam == TIMER_SET_GLOBAL_HOTKEY)
		{
			KillTimer(hWnd, TIMER_SET_GLOBAL_HOTKEY);
			RegisterHotkeyFromString(g_mainHwnd, pref_hotkey_toggle_main_panel, HOTKEY_ID_TOGGLE_MAIN_PANEL);
		}
		else if (wParam == TIMER_SHOW_WINDOW)
		{
			KillTimer(hWnd, TIMER_SHOW_WINDOW);
			ShowMainWindowSimple();
		}
		break;
	case WM_FOCUS_EDIT:
		SetFocus(g_editHwnd);
		break;
	case WM_REFRESH_SKIN:
		{
			refreshSkin(g_currectSkinFilePath);
			return 0;
		}
	case WM_CLOSE:
		{
			HideWindow();
			return 0;
		}
	case WM_SYSCOMMAND:
		{
			if ((wParam & 0xFFF0) == SC_MINIMIZE)
			{
				// 阻止默认最小化行为
				HideWindow();
				return 0;
			}
		}
		break;
	case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED)
			{
				// 阻止默认最小化行为
				HideWindow();
				return 0;
			}
		}
		break;
	case WM_SYSCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		{
			// 阻止 ALT 等系统键造成的副作用
			return 0;
		}
	case WM_GETDLGCODE:
		return DLGC_WANTALLKEYS;
	case WM_DESTROY:
		{
			// 程序退出时释放 GDI+ 资源
			if (g_BgImage)
			{
				delete g_BgImage;
				g_BgImage = nullptr;
			}

			SaveWindowRectToRegistry(hWnd);
			ListView_DeleteAllItems(g_listViewHwnd);
			listViewCleanup();

			// 关闭插件系统
			if (g_pluginManager) {
				g_pluginManager->UnloadAllPlugins();
				g_pluginManager.reset();
			}

			TrayMenuManager::TrayMenuDestroy();
			PostQuitMessage(0);
			ExitProcess(0);
		}
		break;
	case WM_RBUTTONUP:
		{
			TrayMenuManager::TrayMenuShow(hWnd);
			return 0;
		}
	case WM_TRAYICON:
		{
			if (lParam == WM_RBUTTONUP)
			{
				TrayMenuManager::TrayMenuShow(hWnd);
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	// return 0;
	return DefWindowProc(hWnd, message, wParam, lParam);
}
