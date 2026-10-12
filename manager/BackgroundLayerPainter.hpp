#pragma once

#include <windows.h>
#include <gdiplus.h>
#include "../common/GlobalState.hpp"
#include "../util/ColorUtil.hpp"

// 皮肤 window_opacity 的实现：按比例降低背景色的 alpha。
// 不能用 SetLayeredWindowAttributes：调用后主窗口会永久变成分层窗口（精灵），
// 编辑框每次输入时的 LockWindowUpdate 会让整个窗口闪烁一下，切回其他皮肤也不会恢复。
// 主窗口本身已支持逐像素透明，所以直接把透明度烘焙进背景色和背景图即可；文字和图标保持不透明
static Gdiplus::Color ApplySkinWindowOpacity(const Gdiplus::Color& color) {
	if (g_skinWindowOpacity >= 255) return color;
	const int alpha = static_cast<int>(color.GetA()) * g_skinWindowOpacity / 255;
	return Gdiplus::Color(static_cast<BYTE>(alpha), color.GetR(), color.GetG(), color.GetB());
}

// 把 window_opacity 烘焙进皮肤图片，返回新的图片对象（原图片被释放）；不透明时原样返回
static Gdiplus::Image* BakeSkinWindowOpacity(Gdiplus::Image* image) {
	if (!image || g_skinWindowOpacity >= 255) return image;
	const UINT width = image->GetWidth();
	const UINT height = image->GetHeight();
	if (width == 0 || height == 0) return image;
	auto* bitmap = new Gdiplus::Bitmap(static_cast<INT>(width), static_cast<INT>(height), PixelFormat32bppPARGB);
	if (bitmap->GetLastStatus() != Gdiplus::Ok) {
		delete bitmap;
		return image;
	}
	Gdiplus::Graphics graphics(bitmap);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	Gdiplus::ColorMatrix matrix{};
	for (int i = 0; i < 5; ++i) matrix.m[i][i] = 1.f;
	matrix.m[3][3] = static_cast<Gdiplus::REAL>(g_skinWindowOpacity) / 255.f;
	Gdiplus::ImageAttributes attributes;
	attributes.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
	graphics.DrawImage(image, Gdiplus::Rect(0, 0, static_cast<INT>(width), static_cast<INT>(height)),
						0, 0, static_cast<INT>(width), static_cast<INT>(height), Gdiplus::UnitPixel, &attributes);
	delete image;
	return bitmap;
}


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
		}
	}
	graphics.SetClip(&oldClip);
}
