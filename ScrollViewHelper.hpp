#pragma once
#include "SettingsHelper.hpp"

static void UpdateScrollRange(HWND hwnd, int contentHeight)
{
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	const int viewHeight = clientRect.bottom - clientRect.top;

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE;
	si.nMin = 0;
	si.nMax = contentHeight; // 总内容高度
	si.nPage = viewHeight; // 可视区域高度
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

	// 重置滚动位置
	SetScrollPos(hwnd, SB_VERT, 0, TRUE);
}

static std::vector<std::vector<HWND>> hCtrlsByTab; // tab下所有控件句柄
static std::vector<HWND> tabContainers;

// 這是專門為 ComboBox 的下拉列表視窗設計的子類化回呼函式
static LRESULT CALLBACK ComboBoxListSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	// 我們只關心滑鼠滾輪事件
	if (uMsg == WM_MOUSEWHEEL)
	{
		// dwRefData 儲存的是我們希望接收滾動消息的父容器句柄
		HWND hParentContainer = (HWND)dwRefData;

		// 將滾輪消息原封不動地轉發給父容器
		SendMessage(hParentContainer, uMsg, wParam, lParam);

		// ShowErrorMsgBox(L"自己处理");
		// 返回 0，表示我們已經處理了此消息，
		// 從而阻止 ComboBox 的列表框滾動其自身內容。
		return 0; 
	}

	// 對於所有其他消息，調用默認的子類化處理程序
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}


static LRESULT CALLBACK ScrollContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_VSCROLL:
		{
			// 1. 获取当前滚动信息
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			GetScrollInfo(hwnd, SB_VERT, &si);

			int curPos = si.nPos;
			int newPos = curPos;

			// 2. 根据滚动条事件计算新位置
			switch (LOWORD(wParam))
			{
			case SB_LINEUP:
				newPos -= 20; // 滚动步长
				break;
			case SB_LINEDOWN:
				newPos += 20;
				break;
			case SB_PAGEUP:
				newPos -= 100; // 翻页步长
				break;
			case SB_PAGEDOWN:
				newPos += 100;
				break;
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION: // 处理拖动结束
				newPos = HIWORD(wParam);
				break;
			}

			// 3. 限制新位置在滚动范围内
			newPos = MyMax(si.nMin, MyMin(newPos, (int)(si.nMax - si.nPage)));
			if (newPos == curPos)
			{
				return 0; // 位置未变，无需操作
			}

			// 4. 更新滚动条位置
			SetScrollPos(hwnd, SB_VERT, newPos, TRUE);

			// 5. 计算滚动的偏移量
			int delta = curPos - newPos;

			// 6. 移动所有子控件 (核心改动)
			// 首先，找到当前容器(hwnd)对应的 tab 索引
			auto it = std::find(tabContainers.begin(), tabContainers.end(), hwnd);
			if (it != tabContainers.end())
			{
				int tabIdx = std::distance(tabContainers.begin(), it);
				const auto& ctrls_in_tab = hCtrlsByTab[tabIdx];

				for (HWND hCtrl : ctrls_in_tab)
				{
					if (hCtrl) // 确保句柄有效
					{
						RECT rect;
						GetWindowRect(hCtrl, &rect); // 获取屏幕坐标
						MapWindowPoints(HWND_DESKTOP, GetParent(hCtrl), (LPPOINT)&rect, 2); // 转换到父窗口(hContainer)坐标

						// 使用 SetWindowPos 移动控件
						SetWindowPos(hCtrl, NULL, rect.left, rect.top + delta, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
					}
				}
			}

			// 7. 让整个容器窗口无效，以便重绘背景
			// InvalidateRect(hwnd, NULL, TRUE);
			UpdateWindow(hwnd);

			return 0;
		}

	case WM_MOUSEWHEEL:
		{
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			int scrollAmount = (zDelta / WHEEL_DELTA) * 3; // 每次滚动3行
			for (int i = 0; i < abs(scrollAmount); ++i)
			{
				SendMessage(hwnd, WM_VSCROLL, zDelta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
			}
			return 0;
		}

	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			SetBkMode(hdcStatic, TRANSPARENT);
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
	case WM_COMMAND:
		{
			// --- 2. 處理來自複選框的特定點擊事件 ---
			int ctrlId = LOWORD(wParam);
			if (HIWORD(wParam) == BN_CLICKED && ctrlId >= 3000 && ctrlId < 3000 + settings2.size())
			{
				// 從ID計算出在 settings2 中的索引
				int settingIndex = ctrlId - 3000;
				const auto& setting = settings2[settingIndex];
				if (setting.type == "bool")
				{
					HWND hCheckbox = (HWND)lParam; // 獲取複選框的句柄
					LRESULT checkState = SendMessage(hCheckbox, BM_GETCHECK, 0, 0);
					if (setting.key == "pref_auto_start")
					{
						if (checkState == BST_CHECKED)
						{
							MessageBoxW(nullptr, L"您啟用了「特殊功能」！", L"提示", MB_OK);
						}
						else
						{
							MessageBoxW(nullptr, L"您關閉了「特殊功能」。", L"提示", MB_OK);
						}
					}

					else if (setting.key == "pref_show_tray_icon")
					{
						if (checkState == BST_CHECKED)
						{
							OutputDebugStringW(L"顯示高級選項\n");
						}
						else
						{
							OutputDebugStringW(L"隱藏高級選項\n");
						}
					}
				}
			}

			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}


// 注册窗口类。
static ATOM MyScrollViewRegisterClass()
{
	WNDCLASSEXW wcex{};
	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	// wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 用系统白背景更稳
	// wcex.hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); // 例如纯白色
	wcex.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));

	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.cbWndExtra = 0;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = ScrollContainerProc;
	wcex.hInstance = GetModuleHandle(nullptr);
	wcex.lpszMenuName = nullptr;
	// wcex.style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN;
	wcex.lpszClassName = L"SCROLLVIEW";
	return RegisterClassExW(&wcex);
}
