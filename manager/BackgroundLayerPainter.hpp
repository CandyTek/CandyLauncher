#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <cstring>
#include "../common/GlobalState.hpp"
#include "../util/ColorUtil.hpp"
#include "../util/LogUtil.hpp"

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

// 预乘 alpha 的 32 位 DIB 图层。皮肤加载时用 GDI+ 合成一次，之后每帧只做 GDI 位块传送：
// BitBlt(SRCCOPY) 会把 alpha 字节原样复制到窗口表面（DWM 会按 alpha 合成），AlphaBlend 则按像素混合。
// 实测 GDI+ 的 Clear + DrawCachedBitmap 在整窗（880x587）上要 10~20 ms，BitBlt 不到 1 ms。
struct PremultipliedDibLayer {
	HBITMAP bitmap = nullptr;
	HDC memDc = nullptr;
	int width = 0;
	int height = 0;

	explicit operator bool() const { return bitmap != nullptr && memDc != nullptr; }

	void reset() {
		if (memDc) {
			DeleteDC(memDc);
			memDc = nullptr;
		}
		if (bitmap) {
			DeleteObject(bitmap);
			bitmap = nullptr;
		}
		width = 0;
		height = 0;
	}
};

// 主窗口背景（整窗大小，背景图或纯色）
inline PremultipliedDibLayer g_mainBgLayer;
// 编辑框 / 列表的完整背景：主窗口背景在该位置的那一块 + 控件自己的背景图或背景色
inline PremultipliedDibLayer g_editBgLayer;
inline PremultipliedDibLayer g_listViewBgLayer;
// 列表项背景图（已缩放到 item 大小），绘制时 AlphaBlend 叠在列表背景上
inline PremultipliedDibLayer g_listItemBgLayer;
inline PremultipliedDibLayer g_listItemBgLayerSelected;

static void clearBackgroundDibLayers() {
	g_mainBgLayer.reset();
	g_editBgLayer.reset();
	g_listViewBgLayer.reset();
	g_listItemBgLayer.reset();
	g_listItemBgLayerSelected.reset();
}

// 把 GDI+ 位图（任意格式）复制成预乘 DIB 图层
static bool createPremultipliedDibLayer(PremultipliedDibLayer& layer, Gdiplus::Bitmap* source) {
	layer.reset();
	if (!source) return false;
	const int width = static_cast<int>(source->GetWidth());
	const int height = static_cast<int>(source->GetHeight());
	if (width <= 0 || height <= 0) return false;

	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height; // 自上而下，与 GDI+ LockBits 行序一致
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!bitmap || !bits) {
		if (bitmap) DeleteObject(bitmap);
		Loge(L"BackgroundLayerPainter", L"CreateDIBSection 失败");
		return false;
	}

	Gdiplus::BitmapData data{};
	Gdiplus::Rect bounds(0, 0, width, height);
	if (source->LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &data) != Gdiplus::Ok) {
		DeleteObject(bitmap);
		Loge(L"BackgroundLayerPainter", L"LockBits 失败");
		return false;
	}
	const size_t rowBytes = static_cast<size_t>(width) * 4;
	for (int y = 0; y < height; ++y) {
		std::memcpy(static_cast<BYTE*>(bits) + y * rowBytes,
					static_cast<const BYTE*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride, rowBytes);
	}
	source->UnlockBits(&data);

	HDC memDc = CreateCompatibleDC(nullptr);
	if (!memDc) {
		DeleteObject(bitmap);
		return false;
	}
	SelectObject(memDc, bitmap);
	layer.bitmap = bitmap;
	layer.memDc = memDc;
	layer.width = width;
	layer.height = height;
	return true;
}

// 用纯色（含 alpha）生成图层
static bool createSolidColorDibLayer(PremultipliedDibLayer& layer, const Gdiplus::Color& color, const int width,
									const int height) {
	layer.reset();
	if (width <= 0 || height <= 0) return false;
	Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppPARGB);
	if (bitmap.GetLastStatus() != Gdiplus::Ok) return false;
	Gdiplus::Graphics graphics(&bitmap);
	graphics.Clear(color);
	return createPremultipliedDibLayer(layer, &bitmap);
}

// 把图层中 (srcX, srcY) 起 width x height 的区域原样复制到目标 DC 的 (x, y)，alpha 一起复制
static void blitDibLayer(const HDC hdc, const PremultipliedDibLayer& layer, const int x, const int y,
						const int width, const int height, const int srcX, const int srcY) {
	BitBlt(hdc, x, y, width, height, layer.memDc, srcX, srcY, SRCCOPY);
}

// 把整个图层按像素 alpha 混合到目标 DC 的 (x, y)
static void alphaBlendDibLayer(const HDC hdc, const PremultipliedDibLayer& layer, const int x, const int y) {
	BLENDFUNCTION blend{};
	blend.BlendOp = AC_SRC_OVER;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = AC_SRC_ALPHA;
	AlphaBlend(hdc, x, y, layer.width, layer.height, layer.memDc, 0, 0, layer.width, layer.height, blend);
}

// 子控件的完整背景图层：先铺主窗口背景在该控件位置的那一块（1:1 裁剪），
// 再叠上控件自己的背景图（缩放到控件大小）或背景色。绘制时一次 BitBlt 覆盖即可，
// 不需要再先清成透明（见下方 paintMainWindowBackgroundUnderChild 的说明）
static bool createChildBackgroundLayer(PremultipliedDibLayer& layer, HWND child, const int width, const int height,
										Gdiplus::Image* ownBackground, const Gdiplus::Color& ownColor) {
	layer.reset();
	if (!child || !IsWindow(child) || width <= 0 || height <= 0) return false;
	Gdiplus::Bitmap composite(width, height, PixelFormat32bppPARGB);
	if (composite.GetLastStatus() != Gdiplus::Ok) return false;
	Gdiplus::Graphics graphics(&composite);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	if (g_BgScaledBitmap && g_mainHwnd) {
		POINT offset{0, 0};
		MapWindowPoints(child, g_mainHwnd, &offset, 1);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
		graphics.DrawImage(g_BgScaledBitmap, Gdiplus::Rect(0, 0, width, height), offset.x, offset.y, width, height,
							Gdiplus::UnitPixel);
	}
	if (ownBackground) {
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
		graphics.DrawImage(ownBackground, Gdiplus::Rect(0, 0, width, height));
	} else {
		const Gdiplus::SolidBrush brush(ownColor);
		graphics.FillRectangle(&brush, Gdiplus::Rect(0, 0, width, height));
	}
	return createPremultipliedDibLayer(layer, &composite);
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
