#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include "framework.h"
#include "WindowsProject1.h"
#include "ListedRunnerPlugin.h"
#include <vector>
#include <string>
#include <shlguid.h>
#include <objbase.h>
#include <commctrl.h>
#include <shlobj.h>
#include <sstream>
#include "ListViewManager.h"
#include "MainTools.hpp"
#include "GlassHelper.hpp"
#include "DataKeeper.hpp"
#include <gdiplus.h>

#include "AppTools.hpp"
#include "DxgiUtils.h"
#include "EditHelper.hpp"
#include "SettingWindow.hpp"
#include "TrayMenuManager.h"

// 全局变量:
HWND hEdit;
// 当前主窗口
HWND hWnd;
// 当前实例
HINSTANCE hInst;
ListViewManager listViewManager;
TrayMenuManager trayMenuManager;
ListedRunnerPlugin plugin;
// 此代码模块中包含的函数的前向声明:
ATOM MyRegisterClass(HINSTANCE hInstance);
ATOM MyRegisterClass2(HINSTANCE hInstance);
void InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
Gdiplus::GdiplusStartupInput gdiplusStartupInput;
static ULONG_PTR gdiplusToken;


// 主进程函数
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					_In_opt_ HINSTANCE hPrevInstance,
					_In_ LPWSTR lpCmdLine,
					_In_ int nCmdShow)
{
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MyRegisterClass(hInstance);
	MyRegisterClass2(hInstance);

	InitInstance(hInstance, nCmdShow);

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT1));

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

	return static_cast<int>(msg.wParam);
}

// 重新加载快捷方式列表
void refreshList()
{
	plugin.LoadConfiguration();
	listViewManager.LoadActions(plugin.GetActions());
}

// 注册窗口类。
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);
	// 确保窗口大小发生变化时，整个客户区都会被重绘
	// wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"MyWindowClass";
	wcex.hbrBackground = nullptr;

	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

// 注册设置窗口类。
ATOM MyRegisterClass2(HINSTANCE hInstance)
{
	WNDCLASSEXW wc{};

	wc.cbSize = sizeof(WNDCLASSEX);
	//wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = SettingsWndProc;
	wc.hInstance = hInstance;
	//wc.cbClsExtra = 0;
	//wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	wc.lpszClassName = L"SettingsWndClass";
	return RegisterClassExW(&wc);
}

void CreateMainWindow(HINSTANCE hInstance, const int nCmdShow)
{
	unsigned long dw_style;
	bool pref_hide_window_after_run = settingsMap["pref_hide_window_after_run"].defValue.get<int>() == 1;
	if (pref_hide_window_after_run)
	{
		// dw_style = WS_POPUP|WS_OVERLAPPEDWINDOW;
		dw_style = WS_POPUP;
	}
	else
	{
		dw_style = WS_POPUP | WS_VISIBLE;
	}
	long dw_ex_style = WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
	if (settingsMap["pref_window_always_on_top"].defValue.get<int>() == 1)
	{
		dw_ex_style |= WS_EX_TOPMOST;
	}
	if (settingsMap["pref_window_mouse_penetration"].defValue.get<int>() == 1)
	{
		dw_ex_style |= WS_EX_TRANSPARENT;
	}
	// 从注册表读取上次窗口关闭的位置
	RECT lastWindowPosition=LoadWindowRectFromRegistry();
	last_open_window_position_x =lastWindowPosition.left;
	last_open_window_position_y =lastWindowPosition.top;
	
	hWnd = CreateWindowExW(
		// WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_TRANSPARENT | WS_EX_LAYERED, // 扩展样式
		// WS_EX_ACCEPTFILES | WS_EX_COMPOSITED | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, // 扩展样式
		dw_ex_style,
		L"MyWindowClass",
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
	if (!hWnd)
	{
		ShowErrorMsgBox(L"创建窗口失败，hWnd为空");
		ExitProcess(1);
	}
	s_mainHwnd = hWnd;

	EnableBlur(hWnd);
	if (!pref_hide_window_after_run)
	{
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
		MyMoveWindow(hWnd);
	}
}

void InitControls(HINSTANCE hInstance, HWND hWnd)
{
	// 编辑框（搜索框）
	hEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
						10, 10, 580, 35, hWnd, reinterpret_cast<HMENU>(1), hInstance, nullptr);

	SendMessageW(hEdit, EM_SETCUEBANNER, TRUE,
				reinterpret_cast<LPARAM>(utf8_to_wide(
					settingsMap["pref_search_box_placeholder"].defValue.get<std::string>()).c_str()));

	listViewManager.Initialize(hWnd, hInstance, 10, 45, 580, 380);
	std::unordered_map<std::string, std::function<void()>> callbacks;
	callbacks["refreshList"] = refreshList;
	callbacks["quit"] = [hWnd]()
	{
		DestroyWindow(hWnd);
	};
	callbacks["restart"] = [hWnd]()
	{
		TrayMenuManager::TrayMenuClick(10005, hWnd, nullptr);
	};
	callbacks["settings"] = [hWnd]()
	{
		TrayMenuManager::TrayMenuClick(10010, hWnd, nullptr);
	};
	plugin = ListedRunnerPlugin(callbacks);

	listViewManager.LoadActions(plugin.GetActions());
	EditHelper::Attach(hEdit, reinterpret_cast<DWORD_PTR>(listViewManager.hListView));
	SetWindowSubclass(listViewManager.hListView, ListViewSubclassProc, 1,
					reinterpret_cast<DWORD_PTR>(hEdit));
	SetWindowLong(hEdit, GWL_EXSTYLE,
				GetWindowLong(hEdit, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
	ListView_SetBkColor(listViewManager.hListView, COLOR_UI_BG);
	ListView_SetTextBkColor(listViewManager.hListView, COLOR_UI_BG);
	SetFocus(hEdit);
}


// 创建主窗口，初始化
void InitInstance(HINSTANCE hInstance, const int nCmdShow)
{
	LoadSettingList();
	ShowSettingsWindow(hInstance, nullptr);
	// return;
	CreateMainWindow(hInstance, nCmdShow);
	InitControls(hInstance, hWnd);
	trayMenuManager.Init(hWnd, hInstance);
	userSettingsAfterTheAppStart(trayMenuManager);
	ShowWindow(g_settingsHwnd, SW_MINIMIZE);
	Println(L"Windows inited.");
}

static std::wstring buffer;

// 处理主窗口的消息。
LRESULT CALLBACK WndProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		{
			const int wmId = LOWORD(wParam);
			const int wmEvent = HIWORD(wParam);

			// 处理编辑框输入改变
			if (wmId == 1 && wmEvent == EN_CHANGE)
			{
				buffer.resize(256, L'\0');
				buffer.resize(GetWindowTextW(hEdit, &buffer[0], 256));
				SendMessage(listViewManager.hListView, WM_SETREDRAW, FALSE, 0);
				listViewManager.Filter(buffer);
				SendMessage(listViewManager.hListView, WM_SETREDRAW, TRUE, 0);
			}
			else if (wmId > TRAY_MENU_ID_BASE && wmId < TRAY_MENU_ID_BASE_END)
			{
				TrayMenuManager::TrayMenuClick(wmId, hWnd, hEdit);
			}
		}
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			// 使用 hdc 的任何绘图代码
			HDC hdc = BeginPaint(hWnd, &ps);
			// 创建带透明度的刷子，比如黑色透明背景
			HBRUSH hBrush = CreateSolidBrush(COLOR_UI_BG);
			FillRect(hdc, &ps.rcPaint, hBrush);
			DeleteObject(hBrush);

			EndPaint(hWnd, &ps);
		}
		break;

	case WM_DRAWITEM:
		{
			const tagDRAWITEMSTRUCT* lpDrawItem = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);

			if (lpDrawItem->CtlID == 2) // 控件 ID，确保正确
			{
				listViewManager.DrawItem(lpDrawItem);
				return TRUE;
			}
		}
		break;
	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT* lpMeasureItem = reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam);
			if (lpMeasureItem->CtlID == 2)
			{
				ListViewManager::MeasureItem(lpMeasureItem);
				return TRUE;
			}
		}
		break;

	case WM_HOTKEY:
		{
			switch (wParam)
			{
			case HOTKEY_ID_TOGGLE_MAIN_PANEL:
				{
					Println(L"Hotkey Alt + K");
					if (IsWindowVisible(hWnd))
					{
						HideWindow(hWnd, hEdit, listViewManager.hListView);
					}
					else
					{
						// 判断全屏应用模式
						bool shouldShow = true;
						if (pref_hide_in_fullscreen)
							shouldShow = shouldShowInCurrentWindowMode(GetForegroundWindow());
						else if (pref_hide_in_topmost_fullscreen)
							shouldShow = shouldShowInCurrentWindowTopmostMode(GetForegroundWindow());

						if (shouldShow)
						{
							if (pref_show_window_and_release_modifier_key)
								ReleaseAltKey();
							ShowMainWindowSimple(hWnd, hEdit);
						}
					}
				}
				return 0;
			}
		}
		break;
	case WM_KEYDOWN:
		{
			// 1. 获取当前按下的虚拟键码
			UINT vk = wParam;
			// 2. 获取当前的修饰符状态
			UINT currentModifiers = 0;

			if (GetKeyState(VK_MENU) < 0) currentModifiers |= MOD_ALT_KEY;
			if (GetKeyState(VK_CONTROL) < 0) currentModifiers |= MOD_CTRL_KEY;
			if (GetKeyState(VK_SHIFT) < 0) currentModifiers |= MOD_SHIFT_KEY;
			if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) currentModifiers |= MOD_WIN_KEY;

			// 未组合修饰键
			if (currentModifiers == 0)
			{
				if (vk == VK_ESCAPE)
				{
					Println(L"Key Esc");
					HideWindow(hWnd, hEdit, listViewManager.hListView);
					return 0;
				}
			}

			// 3. 生成要查找的键
			UINT64 key = MAKE_HOTKEY_KEY(currentModifiers, vk);

			// 4. 在哈希表中查找 (高性能!)
			auto it = g_hotkeyMap.find(key);
			if (it != g_hotkeyMap.end())
			{
				// 找到了！执行对应的动作
				ShowErrorMsgBox("打开文件位置");
				switch (it->second)
				{
				case HOTKEY_ID_OPEN_FILE_LOCATION:
					{
					}
					break;
				}
				// it->second();

				return 0; // 消息已处理，不再传递
			}
		}
		break;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		{
			// 用于 编辑框的背景颜色
			HDC hdc = reinterpret_cast<HDC>(wParam);

			// 设置背景透明
			SetBkMode(hdc, TRANSPARENT);

			static HBRUSH hBrush = CreateSolidBrush(COLOR_UI_BG); // 深色背景
			return reinterpret_cast<INT_PTR>(hBrush);
		}
	case WM_ERASEBKGND:
		return 1; // 告诉系统“我已经处理了背景”
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
				if (settingsMap["pref_close_on_dismiss_focus"].defValue.get<std::string>() == "close_immediate")
					HideWindow(hWnd, hEdit, listViewManager.hListView);
			}
		}
		break;
	case WM_EDIT_CONTROL_HOTKEY:
		{
			const auto it = GetListViewSelectedAction(listViewManager.hListView, listViewManager.filteredActions);
			if (!it) return 0;
			if (pref_close_after_open_item)
				HideWindow(hWnd, hEdit, listViewManager.hListView);

			switch (wParam)
			{
			case HOTKEY_ID_RUN_ITEM:
				it->Invoke();
				break;
			case HOTKEY_ID_OPEN_FILE_LOCATION:
				it->InvokeOpenFolder();
				break;
			case HOTKEY_ID_OPEN_TARGET_LOCATION:
				it->InvokeOpenGoalFolder();
				break;
			case HOTKEY_ID_RUN_ITEM_AS_ADMIN:
				break;
			case HOTKEY_ID_OPEN_WITH_CLIPBOARD_PARAMS:
				it->InvokeWithTargetClipBoard();
				break;
			default: ;
			}
			return 0;
		}
	case WM_CONFIG_SAVED:
		{
			std::thread t([]()
			{
				doPrefChanged(trayMenuManager);
			});
			t.detach();
			return 0;
		}
	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if (pnmh->hwndFrom == listViewManager.hListView)
			{
				if (pnmh->code == NM_CLICK)
				{
					PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
				}
				if (!pref_single_click_to_open && pnmh->code == NM_DBLCLK) // 双击
				{
					LPNMITEMACTIVATE pia = (LPNMITEMACTIVATE)lParam;
					int index = pia->iItem;
					if (index != -1 && listViewManager.filteredActions.size() > index)
					{
						const auto it = listViewManager.filteredActions[index];
			if (pref_close_after_open_item)
						HideWindow(hWnd, hEdit, listViewManager.hListView);
						it->Invoke();
						// 你可以通过 index 找到 filteredActions[index]
						// 比如执行该 action
						// ExecuteAction(filteredActions[index]);
					}
					return TRUE;
				}
				else if (pref_single_click_to_open && pnmh->code == NM_CLICK)
				{
					LPNMITEMACTIVATE pia = (LPNMITEMACTIVATE)lParam;
					int index = pia->iItem;
					if (index != -1 && listViewManager.filteredActions.size() > index)
					{
						const auto it = listViewManager.filteredActions[index];
			if (pref_close_after_open_item)
						HideWindow(hWnd, hEdit, listViewManager.hListView);
						it->Invoke();
					}
					return TRUE;
				}
				else if (pnmh->code == NM_RCLICK) // 右键点击
				{
					POINT pt;
					// 获取鼠标坐标（相对 ListView 客户区）
					GetCursorPos(&pt);

					// 获取当前点击项
					LVHITTESTINFO lvhti = {};
					POINT ptClient = pt;
					ScreenToClient(listViewManager.hListView, &ptClient);
					lvhti.pt = ptClient;
					int index = ListView_HitTest(listViewManager.hListView, &lvhti);
					if (index != -1)
					{
						// 选中当前项（可选）
						// ListView_SetItemState(listViewManager.hListView, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
						std::shared_ptr<RunCommandAction> it = listViewManager.filteredActions[index];
						auto temp = it->GetTargetPath();
						POINT pt;
						GetCursorPos(&pt);

						ShowShellContextMenu(hWnd, temp, pt);
					}
					PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
					return TRUE;
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
			SetFocus(hEdit);
		}
		break;
	case WM_FOCUS_EDIT:
		SetFocus(hEdit);
		break;
	case WM_CLOSE:
		{
			HideWindow(hWnd, hEdit, listViewManager.hListView);
			return 0;
		}
	case WM_SYSCOMMAND:
		{
			if ((wParam & 0xFFF0) == SC_MINIMIZE)
			{
				// 阻止默认最小化行为
				HideWindow(hWnd, hEdit, listViewManager.hListView);
				return 0;
			}
		}
		break;
	case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED)
			{
				// 阻止默认最小化行为
				HideWindow(hWnd, hEdit, listViewManager.hListView);
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
			SaveWindowRectToRegistry(hWnd);
			ListView_DeleteAllItems(listViewManager.hListView);
			listViewManager.Cleanup();
			//Gdiplus::GdiplusShutdown(gdiplusToken);

			trayMenuManager.TrayMenuDestroy();
			UnregisterHotKey(nullptr, HOTKEY_ID_TOGGLE_MAIN_PANEL);
			PostQuitMessage(0);
		}
		break;
	case WM_TRAYICON:
		{
			if (lParam == WM_RBUTTONUP)
			{
				trayMenuManager.TrayMenuShow(hWnd);
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	// return 0;
	return DefWindowProc(hWnd, message, wParam, lParam);
}
