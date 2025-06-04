#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include "framework.h"
#include "WindowsProject1.h"
#include "ListedRunnerPlugin.h"
#include <commctrl.h>
#include <vector>
#include <string>
#include <shlguid.h>
#include <objbase.h>
#include <commoncontrols.h>
#include <shlobj.h>
#include <sstream>
#include "ListViewManager.h"
#include "MainTools.hpp"
#include "GlassHelper.hpp"
#include "DataKeeper.hpp"
#include <gdiplus.h>

#include "EditHelper.hpp"
#include "TrayMenuManager.h"

// 全局变量:
HWND hEdit;
// 当前实例
HINSTANCE hInst;
ListViewManager listViewManager;
TrayMenuManager trayMenuManager;
ListedRunnerPlugin plugin = nullptr;
// 此代码模块中包含的函数的前向声明:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
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

	if (!InitInstance(hInstance, nCmdShow))
	{
		ShowErrorMsgBox(L"窗口创建失败");
		return FALSE;
	}

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
int refreshList(const int num)
{
	plugin.LoadConfiguration();
	listViewManager.LoadActions(plugin.GetActions());
	return 0;
}

// 注册窗口类。
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	//wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.hbrBackground = nullptr;

	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"MyWindowClass";

	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

// 创建主窗口，初始化
BOOL InitInstance(HINSTANCE hInstance, const int nCmdShow)
{
	HWND hWnd = CreateWindowW(
		L"MyWindowClass",
		L"My App Title",
		//WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_EX_CONTROLPARENT,
		// 去除窗口边框
		WS_POPUP | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		620, 480,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (!hWnd)
		return FALSE;

	CustomRegisterHotKey(hWnd);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	MyMoveWindow(hWnd);

	// 编辑框（搜索框）
	hEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
						10, 10, 580, 35, hWnd, reinterpret_cast<HMENU>(1), hInstance, nullptr);


	listViewManager.Initialize(hWnd, hInstance, 10, 45, 580, 380);
	plugin = ListedRunnerPlugin(refreshList);

	listViewManager.LoadActions(plugin.GetActions());
	EditHelper::Attach(hEdit, reinterpret_cast<DWORD_PTR>(listViewManager.hListView));
	SetWindowSubclass(listViewManager.hListView, ListViewSubclassProc, 1,
					reinterpret_cast<DWORD_PTR>(hEdit));
	SetWindowLong(hEdit, GWL_EXSTYLE,
				GetWindowLong(hEdit, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
	ListView_SetBkColor(listViewManager.hListView, COLOR_UI_BG);
	ListView_SetTextBkColor(listViewManager.hListView, COLOR_UI_BG);

	SetFocus(hEdit);
	EnableBlur(hWnd);
	trayMenuManager.Init(hWnd, hInstance);

	Println(L"Windows inited.");
	Println(L"Windows inited.");
	Println(L"Windows inited.");
	Println(L"Windows inited.");

	return TRUE;
}

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
				wchar_t buffer[256];
				GetWindowText(hEdit, buffer, 256);
				SendMessage(listViewManager.hListView, WM_SETREDRAW, FALSE, 0);
				listViewManager.Filter(buffer);
				SendMessage(listViewManager.hListView, WM_SETREDRAW, TRUE, 0);
			}
			else if (wmId > 10000 && wmId < 10010)
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
			if (wParam == 1)
			{
				Println(L"Hotkey Alt + K");
				if (IsWindowVisible(hWnd))
				{
					HideWindow(hWnd, hEdit, listViewManager.hListView);
					return 0;
				}
				else
				{
					ReleaseAltKey();
					ShowMainWindowSimple(hWnd, hEdit);
					return 0;
				}
			}
		}
		break;
	case WM_KEYDOWN:
		{
			// 判断 Ctrl 是否按下
			const bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			if (ctrlPressed)
			{
				switch (wParam)
				{
				case 'O':
					{
						const auto selected1 = GetListViewSelectedAction(
							listViewManager.hListView, listViewManager.filteredActions);
						if (selected1)
						{
							HideWindow(hWnd, hEdit, listViewManager.hListView);
							selected1->InvokeOpenFolder();
						}
					}
					return 0;
				case 'I':
					{
						const auto selected1 = GetListViewSelectedAction(
							listViewManager.hListView, listViewManager.filteredActions);
						if (selected1)
						{
							HideWindow(hWnd, hEdit, listViewManager.hListView);
							selected1->InvokeOpenGoalFolder();
						}
					}

					return 0;
				}
			}

			if (wParam == VK_ESCAPE)
			{
				Println(L"Key Esc");
				HideWindow(hWnd, hEdit, listViewManager.hListView);
				return 0;
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
	case WM_EDIT_DONE:
		{
			const auto it = GetListViewSelectedAction(listViewManager.hListView, listViewManager.filteredActions);
			if (!it) return 0;
			HideWindow(hWnd, hEdit, listViewManager.hListView);

			switch (wParam)
			{
			case 0:
				it->Invoke();
				break;
			case 1:
				it->InvokeOpenFolder();
				break;
			case 2:
				it->InvokeOpenGoalFolder();
				break;
			default: ;
			}
			return 0;
		}
	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if (pnmh->hwndFrom == listViewManager.hListView)
			{
				if (pnmh->code == NM_DBLCLK) // 双击
				{
					LPNMITEMACTIVATE pia = (LPNMITEMACTIVATE)lParam;
					int index = pia->iItem;
					if (index != -1 && listViewManager.filteredActions.size() > index)
					{
						const auto it = listViewManager.filteredActions[index];
						HideWindow(hWnd, hEdit, listViewManager.hListView);
						it->Invoke();
						// 你可以通过 index 找到 filteredActions[index]
						// 比如执行该 action
						// ExecuteAction(filteredActions[index]);
					}
					return TRUE;
				}     else   if (pnmh->code == NM_RCLICK) // 右键点击
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
					auto temp=it->GetTargetPath();
						POINT pt;
						GetCursorPos(&pt);

						ShowShellContextMenu(hWnd,temp,pt);
					}
					return TRUE;
				}

				// 右键事件同上...
			}
			break;
		}

	case WM_CLOSE:
		{
			HideWindow(hWnd, hEdit, listViewManager.hListView);
			return 0;
		}
	case WM_SYSCOMMAND:
		{
			if ((wParam & 0xFFF0) == SC_MINIMIZE)
			{
				HideWindow(hWnd, hEdit, listViewManager.hListView);
				// 阻止默认最小化行为
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
			ListView_DeleteAllItems(listViewManager.hListView);
			listViewManager.Cleanup();
			//Gdiplus::GdiplusShutdown(gdiplusToken);

			trayMenuManager.TrayMenuDestroy();
			UnregisterHotKey(nullptr, 1);
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
	return 0;
}
