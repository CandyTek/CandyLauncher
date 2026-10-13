#pragma once

#include <windows.h>
#include <string>
#include "LogUtil.hpp"

// 绘制耗时统计。Debug 堆会严重干扰数据，请在 Release 构建下测量。
// 关闭后（false）所有调用都会被编译掉，绘制路径零开销。
constexpr bool kPaintProfileEnabled = false;

inline double PaintProfileTicksToMs(const LONGLONG ticks) {
	static LARGE_INTEGER frequency = [] {
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		return f;
	}();
	return static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency.QuadPart);
}

// 高精度秒表，构造即开始计时
struct PaintStopwatch {
	LARGE_INTEGER start{};

	PaintStopwatch() {
		if constexpr (kPaintProfileEnabled) QueryPerformanceCounter(&start);
	}

	double elapsedMs() const {
		if constexpr (!kPaintProfileEnabled) return 0.0;
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return PaintProfileTicksToMs(now.QuadPart - start.QuadPart);
	}
};

// 一次列表 WM_PAINT 内各阶段的累计耗时
struct ListPaintStats {
	int items = 0;
	double backgroundMs = 0.0; // 列表背景（CDDS_PREPAINT）
	double itemBgMs = 0.0; // 各项背景
	double textMs = 0.0; // 各项文字
	double iconMs = 0.0; // 各项图标

	void reset() { *this = ListPaintStats{}; }
};

inline ListPaintStats g_listPaintStats;

inline std::wstring PaintProfileFormatMs(const double ms) {
	wchar_t buffer[32];
	swprintf_s(buffer, L"%.3f", ms);
	return buffer;
}

template <typename... Args>
void PaintProfileLog(const Args&... args) {
	if constexpr (kPaintProfileEnabled) Logi(L"PaintProfile", args...);
}
