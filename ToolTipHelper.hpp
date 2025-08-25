#pragma once


// TooltipWarn.h / .cpp
#include <windows.h>
#include <commctrl.h>
#include <string>
#pragma comment(lib, "comctl32.lib")

// 只需在程序初始化处调用一次（若你已在别处 InitCommonControlsEx，可忽略）
inline void EnsureCommonControlsForTooltip()
{
	static bool inited = false;
	if (!inited) {
		INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES };
		InitCommonControlsEx(&icc);
		inited = true;
	}
}


inline HWND s_tip = nullptr;
inline bool s_added = false;
inline std::wstring s_buf; // 保存文本，保证生命周期
inline UINT_PTR s_timer = 0; // 定时器ID
inline TTTOOLINFOW g_toolInfo{};


// 在指定坐标显示一个 2 秒自动消失的"警告"气泡提示
inline void ShowWarnTooltip(HWND owner,
							LPCWSTR text,
							int x,
							int y,
							int durationMs = 2000)
{
	EnsureCommonControlsForTooltip();

	// 如果有之前的定时器，先取消它
	if (s_timer != 0) {
		KillTimer(owner, s_timer);
		s_timer = 0;
	}

	// 如果工具提示正在显示，先隐藏它
	if (IsWindow(s_tip) && s_added) {
		SendMessageW(s_tip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&g_toolInfo);
	}

	if (!IsWindow(s_tip)) {
		s_tip = CreateWindowExW(
				WS_EX_TOPMOST,
				TOOLTIPS_CLASSW, nullptr,
				WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				owner, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (!s_tip) return;

		SetWindowPos(s_tip, HWND_TOPMOST, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		// 允许多行换行
		SendMessageW(s_tip, TTM_SETMAXTIPWIDTH, 0, 400);
	}

	s_buf = text ? text : L"";
	g_toolInfo = { sizeof(g_toolInfo) };
	g_toolInfo.uFlags   = TTF_TRACK | TTF_ABSOLUTE; // 手动定位（屏幕坐标）
	g_toolInfo.hwnd     = owner;
	g_toolInfo.uId      = 1;                        // 固定一个 ID，重复使用
	g_toolInfo.lpszText = const_cast<wchar_t*>(s_buf.c_str());

	if (!s_added) {
		SendMessageW(s_tip, TTM_ADDTOOLW, 0, (LPARAM)&g_toolInfo);
		s_added = true;
	} else {
		SendMessageW(s_tip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&g_toolInfo);
	}

	// 标题 + 警告图标
	SendMessageW(s_tip, TTM_SETTITLEW, TTI_WARNING, (LPARAM)L"提示");

	// 立刻出现、立刻允许再次出现、较长的自动隐藏时间
	SendMessageW(s_tip, TTM_SETDELAYTIME, TTDT_INITIAL, 0);
	SendMessageW(s_tip, TTM_SETDELAYTIME, TTDT_RESHOW,  0);
	SendMessageW(s_tip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 32767); // 最大值，防止自动隐藏

	// 设置位置（屏幕坐标）
	SendMessageW(s_tip, TTM_TRACKPOSITION, 0, MAKELONG(x, y));

	// 显示
	SendMessageW(s_tip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&g_toolInfo);

	// 使用定时器来控制隐藏时间
	s_timer = SetTimer(owner, 9999, durationMs, [](HWND hwnd, UINT, UINT_PTR idTimer, DWORD) -> VOID {
		// 隐藏工具提示
		if (IsWindow(s_tip) && s_added) {
			SendMessageW(s_tip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&g_toolInfo);
		}
		
		// 清理定时器
		KillTimer(hwnd, idTimer);
		s_timer = 0;
	});
}

// 在鼠标右边显示一个 2 秒自动消失的"警告"气泡提示
inline void ShowWarnTooltipAtCursor(HWND owner,
									LPCWSTR text,
									int durationMs = 2000,
									int xOffset = 16,
									int yOffset = 8)
{
	// 获取鼠标位置并添加偏移
	POINT pt{};
	GetCursorPos(&pt);
	pt.x += xOffset;
	pt.y += yOffset;
	
	// 调用基础方法
	ShowWarnTooltip(owner, text, pt.x, pt.y, durationMs);
}

// 可选：手动隐藏（如果需要在某些场景立即收起）
inline void HideWarnTooltip(HWND owner)
{
	// 查找工具提示窗口并隐藏
	HWND tipWnd = FindWindowW(TOOLTIPS_CLASSW, nullptr);
	if (IsWindow(tipWnd)) {
		TTTOOLINFOW ti{ sizeof(ti) };
		ti.uFlags = TTF_TRACK | TTF_ABSOLUTE;
		ti.hwnd = owner;
		ti.uId = 1;
		SendMessageW(tipWnd, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
	}
	
	// 取消定时器
	KillTimer(owner, 9999);
}
