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
#include <atomic>

#include "AppTools.hpp"
#include "DxgiUtils.h"
#include "EditHelper.hpp"
#include "SettingWindow.hpp"
#include "TrayMenuManager.h"
#include <Richedit.h>

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
std::atomic<bool> g_shouldStop{false}; // 停止标志
std::thread g_watcherThread;

// 主进程函数
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
					_In_opt_ HINSTANCE hPrevInstance,
					_In_ LPWSTR lpCmdLine,
					_In_ int nCmdShow)
{
	// 加载富文本专用
	// LoadLibrary(L"Msftedit.dll");
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	MyRegisterClass(hInstance);
	MyRegisterClass2(hInstance);

	InitInstance(hInstance, nCmdShow);
	// 启动监听线程 用于皮肤测试
	// std::thread watcfeherThread(watchSkinFile);
	//std::thread(watchSkinFile).detach();
	g_watcherThread = std::thread(watchSkinFile);

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
	// 程序退出前通知线程停止
	if (g_watcherThread.joinable())
	{
		g_shouldStop = true;
		// Windows 平台使用 CancelSynchronousIo

		HANDLE hThread = reinterpret_cast<HANDLE>(g_watcherThread.native_handle());
		if (!CancelSynchronousIo(hThread))
		{
			DWORD err = GetLastError();
			std::wcerr << L"CancelSynchronousIo 失败，错误码: " << err << std::endl;
		}

		g_watcherThread.join();
	}

	return static_cast<int>(msg.wParam);
}

// 重新加载快捷方式列表
static void refreshList()
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
	wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));

	wc.lpszClassName = L"SettingsWndClass";
	return RegisterClassExW(&wc);
}

static void CreateMainWindow(HINSTANCE hInstance, const int nCmdShow)
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
	RECT lastWindowPosition = LoadWindowRectFromRegistry();
	last_open_window_position_x = lastWindowPosition.left;
	last_open_window_position_y = lastWindowPosition.top;

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
	if (!pref_hide_window_after_run)
	{
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
		MyMoveWindow(hWnd);
	}

	// 用于皮肤测试
	RegisterHotkeyFromString(s_mainHwnd, "xx(1)(81)",HOTKEY_ID_REFRESH_SKIN);
}

static void InitControls(HINSTANCE hInstance, HWND hWnd)
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
	// 不要使用透明，和主窗口的透明起冲突了
	// SetWindowLong(hEdit, GWL_EXSTYLE,
	// 			GetWindowLong(hEdit, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
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
	// 另一种指定透明效果，但是并不太行，有很严重的锯齿，而且没有透明度概念，很生硬
	// SetLayeredWindowAttributes(s_mainHwnd, RGB(80, 81, 82), 0, LWA_COLORKEY);

	// EnableBlur(hWnd);
	// EnableBlur2(listViewManager.hListView);
	// EnableBlur2(hEdit);

	// MARGINS margins={-1};
	// DwmExtendFrameIntoClientArea(s_mainHwnd,&margins);
	// DwmExtendFrameIntoClientArea(listViewManager.hListView,&margins);
	// DwmExtendFrameIntoClientArea(hEdit,&margins);

	DWM_BLURBEHIND db{};
	db.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
	db.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
	db.fEnable = TRUE;
	DwmEnableBlurBehindWindow(s_mainHwnd, &db);
	// DwmEnableBlurBehindWindow(listViewManager.hListView, &db);
	// DwmEnableBlurBehindWindow(hEdit, &db);

	Println(L"Windows inited.");
}

static void getSkinPictureFile(Gdiplus::Image*& image, const std::string& skinKey)
{
	const std::string picturePath = g_skinJson.value(skinKey, "");
	if (image)
	{
		// 删除旧的图片对象
		delete image;
		image = nullptr;
	}
	if (!picturePath.empty())
	{
		image = new Gdiplus::Image(utf8_to_wide(picturePath).c_str());
		if (image->GetLastStatus() != Gdiplus::Ok)
		{
			delete image;
			image = nullptr;
			ShowErrorMsgBox(L"加载背景图片失败");
		}
	}
}

static void refreshSkin()
{
	std::wstring skinPath = LR"(C:\Users\Administrator\source\repos\WindowsProject1\skin_test.json)";
	std::ifstream in((skinPath.data()));
	if (!in)
	{
		std::wcerr << L"文件不存在：" << skinPath << std::endl;
		return;
	}

	// 读取整个文件内容（UTF-8 编码）
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string utf8json = buffer.str();

	// 解析 JSON
	try
	{
		g_skinJson = nlohmann::json::parse(utf8json);
		in.close();
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::wcerr << L"JSON 解析错误：" << utf8_to_wide(e.what()) << std::endl;
		return;
	}
	// --- 1. 更新主窗口 ---
	int windowWidth = g_skinJson.value("window_width", MAIN_WINDOW_WIDTH);
	int windowHeight = g_skinJson.value("window_height", MAIN_WINDOW_HEIGHT);
	SetWindowPos(hWnd, nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER);

	// 处理背景图片
	getSkinPictureFile(g_BgImage, "window_bg_picture");
	getSkinPictureFile(g_editBgImage, "editbox_bg_picture");
	getSkinPictureFile(g_listViewBgImage, "listview_bg_picture");
	getSkinPictureFile(g_listItemBgImage, "item_font_bg_picture");
	getSkinPictureFile(g_listItemBgImageSelected, "item_font_bg_picture_selected");

	// --- 2. 更新编辑框 (hEdit) ---
	int editX = g_skinJson.value("editbox_x", 10);
	int editY = g_skinJson.value("editbox_y", 10);
	int editWidth = g_skinJson.value("editbox_width", 580);
	int editHeight = g_skinJson.value("editbox_height", 35);
	SetWindowPos(hEdit, nullptr, editX, editY, editWidth, editHeight, SWP_NOZORDER);

	// 更新编辑框字体
	std::wstring editFontFamily = utf8_to_wide(g_skinJson.value("editbox_font_family", "宋体"));
	double editFontSize = g_skinJson.value("editbox_font_size", 12.0);
	HDC hdc = GetDC(hEdit);
	HFONT hEditFont = CreateFontW(
		-MulDiv(static_cast<int>(editFontSize), GetDeviceCaps(hdc, LOGPIXELSY), 72),
		0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, editFontFamily.c_str()
	);
	ReleaseDC(hEdit, hdc);
	if (hEditFont)
	{
		SendMessage(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hEditFont), TRUE);
	}
	SendMessage(hEdit, WM_NOTIFY_HEDIT_REFRESH_SKIN, 0, TRUE);

	// 初始化GDI+图形对象


	// Gdiplus::Graphics graphics(hEdit);
	// graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	// Gdiplus::FontFamily fontFamily(L"微软雅黑");
	// Gdiplus::Font gdiFont(&fontFamily, 25, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	// LOGFONTW logfont;
	// Gdiplus::Status status = gdiFont.GetLogFontW(&graphics,&logfont);
	// HFONT hFont = CreateFontIndirect(&logfont);
	// SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

	// 注意：编辑框的背景色和前景色需要在 WM_CTLCOLOREDIT 中处理

	// --- 3. 更新列表视图 (ListView) ---

	int listX = g_skinJson.value("listview_x", 10);
	int listY = g_skinJson.value("listview_y", 45);
	g_itemListWidth = g_skinJson.value("listview_width", 580);
	g_itemListHeight = g_skinJson.value("listview_height", 380);
	SetWindowPos(listViewManager.hListView, nullptr, listX, listY, g_itemListWidth, g_itemListHeight, SWP_NOZORDER);
	int thumbWidth = GetWindowVScrollBarThumbWidth(listViewManager.hListView, true);
	ListView_SetColumnWidth(listViewManager.hListView, 0, g_itemListWidth-thumbWidth-6);
	listViewManager.InitializeGraphicsResources(); // 确保字体和画刷已初始化

	// 更新列表视图颜色
	// COLORREF listBgColor = HexToCOLORREF(g_skinJson.value("listview_bg_color", "#FFFFFF"));
	// COLORREF listFontColor = HexToCOLORREF(g_skinJson.value("listview_font_color", "#000000"));
	// ListView_SetBkColor(listViewManager.hListView, listBgColor);
	// ListView_SetTextBkColor(listViewManager.hListView, listBgColor); // 文本背景色通常和列表背景色一致
	// ListView_SetTextColor(listViewManager.hListView, listFontColor);

	// 更新列表视图字体 (与编辑框类似)
	// 注意: ListViewManager 的自定义绘制(WM_DRAWITEM)也需要使用新的字体和颜色信息
	// 你可能需要在 ListViewManager 类中添加方法来存储这些皮肤设置

	// ListView_SetBkColor(listViewManager.hListView, CLR_NONE);
	// ListView_SetTextBkColor(listViewManager.hListView, CLR_NONE);

	// 暴力强制渲染
	RECT rc;
	GetWindowRect(hWnd, &rc);
	// 临时缩小窗口
	SetWindowPos(hWnd, nullptr, rc.left, rc.top, rc.left + 1, rc.top + 1,
				SWP_NOZORDER);
	// 恢复原大小
	SetWindowPos(hWnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
				SWP_NOZORDER);
	ShowWindow(hWnd,SW_FORCEMINIMIZE);
	SetTimer(s_mainHwnd, TIMER_SHOW_WINDOW, 50, nullptr);
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
			HDC hdc = BeginPaint(hWnd, &ps);

			// 使用 GDI+ 绘制背景图
			// 如果没有背景图，则填充纯色背景
			if (g_skinJson != nullptr)
			{
				if (g_BgImage)
				{
					Gdiplus::Graphics graphics(hdc);
					graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
					Gdiplus::Rect rect(ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
					graphics.DrawImage(g_BgImage, rect);
				}
				else
				{
					std::string bgColorStr = g_skinJson.value("window_bg_color", "#FFFFFF");
					if (bgColorStr.empty())
					{
						EndPaint(hWnd, &ps);
						return 1;
					}
					else
					{
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
			}
			else
			{
				HBRUSH hBrush = CreateSolidBrush(COLOR_UI_BG);
				FillRect(hdc, &ps.rcPaint, hBrush);
				DeleteObject(hBrush);
			}

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
			case HOTKEY_ID_REFRESH_SKIN:
				{
					refreshSkin();
				}
				return 0;
			default: break;
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
				default: break;
				}
				// it->second();

				return 0; // 消息已处理，不再传递
			}
		}
		break;
	case WM_ERASEBKGND:
		{
			return 1; // Return non-zero to indicate you have handled erasing the background
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
				if (settingsMap["pref_close_on_dismiss_focus"].defValue.get<std::string>() == "close_immediate")
					HideWindow(hWnd, hEdit, listViewManager.hListView);
			}
		}
		break;
	case WM_EDIT_CONTROL_HOTKEY:
		{
			const std::shared_ptr<RunCommandAction> it = GetListViewSelectedAction(
				listViewManager.hListView, listViewManager.filteredActions);
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
			LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
			if (pnmh->hwndFrom == listViewManager.hListView)
			{
				if (pnmh->code == NM_CLICK)
				{
					PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
				}
				if (!pref_single_click_to_open && pnmh->code == NM_DBLCLK) // 双击
				{
					LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
					int index = pia->iItem;
					if (index != -1 && listViewManager.filteredActions.size() > index)
					{
						const std::shared_ptr<RunCommandAction> it = listViewManager.filteredActions[index];
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
					LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
					int index = pia->iItem;
					if (index != -1 && listViewManager.filteredActions.size() > index)
					{
						const std::shared_ptr<RunCommandAction> it = listViewManager.filteredActions[index];
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
						ShowShellContextMenu(hWnd, temp, pt);
					}
					PostMessage(hWnd, WM_FOCUS_EDIT, 0, 0);
					return TRUE;
				}
				else if (pnmh->code == NM_CUSTOMDRAW)
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
						SetWindowLongPtr(hWnd, DWLP_MSGRESULT, listViewManager.OnCustomDraw(lplvcd));
						// return TRUE;
					}
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
		else if (wParam == TIMER_SET_GLOBAL_HOTKEY)
		{
			KillTimer(hWnd, TIMER_SET_GLOBAL_HOTKEY);
			RegisterHotkeyFromString(s_mainHwnd, pref_hotkey_toggle_main_panel,HOTKEY_ID_TOGGLE_MAIN_PANEL);
		}
		else if (wParam == TIMER_SHOW_WINDOW)
		{
			KillTimer(hWnd, TIMER_SHOW_WINDOW);
			ShowMainWindowSimple(hWnd, hEdit);
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
			// 程序退出时释放 GDI+ 资源
			if (g_BgImage)
			{
				delete g_BgImage;
				g_BgImage = nullptr;
			}

			SaveWindowRectToRegistry(hWnd);
			ListView_DeleteAllItems(listViewManager.hListView);
			listViewManager.Cleanup();
			//Gdiplus::GdiplusShutdown(gdiplusToken);

			trayMenuManager.TrayMenuDestroy();
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
