#pragma once
#include <Windows.h>
#include <dwmapi.h>
#include <tchar.h>
#include "DataKeeper.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")

enum ACCENT_STATE
{
	ACCENT_DISABLED = 0,
	ACCENT_ENABLE_GRADIENT = 1,
	ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
	ACCENT_ENABLE_BLURBEHIND = 3,
	ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
	ACCENT_INVALID_STATE = 5
};

struct ACCENT_POLICY
{
	ACCENT_STATE AccentState;
	int AccentFlags;
	int GradientColor;
	int AnimationId;
};

enum WINDOWCOMPOSITIONATTRIB
{
	WCA_ACCENT_POLICY = 19
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
	WINDOWCOMPOSITIONATTRIB Attrib;
	PVOID pvData;
	SIZE_T cbData;
};

// 定义函数指针类型
typedef BOOL (WINAPI*pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

// ACCENT_ENABLE_ACRYLICBLURBEHIND 需要 Windows 10 Build 1803（April 2018 Update）及以上。
static void EnableBlur(HWND hWnd)
{
	// 加载 user32.dll 中的 SetWindowCompositionAttribute
	const HMODULE hUser = LoadLibrary(TEXT("user32.dll"));
	if (!hUser) return;

	pSetWindowCompositionAttribute SetWindowCompositionAttribute =
		reinterpret_cast<pSetWindowCompositionAttribute>(GetProcAddress(hUser, "SetWindowCompositionAttribute"));

	if (SetWindowCompositionAttribute)
	{
		// ACCENT_POLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, (0xFF << 24) | (0x00 << 16) | (0x00 << 8) | 0x00, 0 };

		// ACCENT_POLICY policy = { ACCENT_ENABLE_BLURBEHIND, 0, 0, 0 };


		ACCENT_POLICY policy{};
		policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
		// policy.AccentState = ACCENT_ENABLE_BLURBEHIND;

		// AccentFlags = 2 是经验值，避免窗口边缘的光晕异常。
		policy.AccentFlags = 2;
		policy.GradientColor = COLOR_UI_BG_GLASS; // 半透明黑色
		policy.AnimationId = 0;


		WINDOWCOMPOSITIONATTRIBDATA data{};
		data.Attrib = WCA_ACCENT_POLICY;
		data.pvData = &policy;
		data.cbData = sizeof(policy);

		SetWindowCompositionAttribute(hWnd, &data);
	}

	FreeLibrary(hUser);
}
