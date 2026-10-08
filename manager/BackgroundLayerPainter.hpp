#pragma once

#include <windows.h>
#include <gdiplus.h>
#include "../common/GlobalState.hpp"
#include "../util/ColorUtil.hpp"

// 已缩放到主窗口大小的背景图，子控件重绘时从这里按位置裁剪出自己下方的那一块
inline Gdiplus::Bitmap* g_BgScaledBitmap = nullptr;

static Gdiplus::Bitmap* createScaledBackgroundBitmap(Gdiplus::Image* image, const int width, const int height) {
	if (!image || width <= 0 || height <= 0) return nullptr;
	auto* bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppPARGB);
	if (bitmap->GetLastStatus() != Gdiplus::Ok) {
		delete bitmap;
		return nullptr;
	}
	Gdiplus::Graphics graphics(bitmap);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.DrawImage(image, 0, 0, width, height);
	return bitmap;
}

// 子控件（编辑框、列表）重绘时，主窗口不会一起重绘它们下方的区域。
// 如果子控件直接在旧像素上叠加半透明背景，每次重绘都会越叠越深（例如每次打开窗口都会刷新列表）。
// 所以子控件绘制自己的背景前，先把区域清成全透明，再补画主窗口在该位置的背景。
static void paintMainWindowBackgroundUnderChild(HWND child, Gdiplus::Graphics& graphics, const RECT& rc) {
	const Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
	if (rect.Width <= 0 || rect.Height <= 0) return;

	Gdiplus::Region oldClip;
	graphics.GetClip(&oldClip);
	graphics.SetClip(rect, Gdiplus::CombineModeIntersect);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	if (g_skinJson != nullptr) {
		POINT offset{0, 0};
		MapWindowPoints(child, g_mainHwnd, &offset, 1);
		if (g_BgScaledBitmap) {
			// 1:1 拷贝，不做缩放
			const Gdiplus::InterpolationMode oldMode = graphics.GetInterpolationMode();
			graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
			graphics.DrawImage(g_BgScaledBitmap, rect, rc.left + offset.x, rc.top + offset.y,
								rect.Width, rect.Height, Gdiplus::UnitPixel);
			graphics.SetInterpolationMode(oldMode);
		} else if (!g_BgImage) {
			if (!g_windowBgColor.empty()) {
				const Gdiplus::SolidBrush bgBrush(HexToGdiplusColor(g_windowBgColor));
				graphics.FillRectangle(&bgBrush, rect);
			}
		}
	}
	graphics.SetClip(&oldClip);
}
