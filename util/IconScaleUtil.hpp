#pragma once

// 列表图标的 DPI 缩放。主窗口 DPI 不是 96 时列表图标也要放大（48 -> 72 等），
// 这里用 GDI+ 高质量重采样一次性生成放大后的预乘 32 位 DIB，绘制时和原来一样只做一次 AlphaBlend；
// 缩放结果缓存在条目 / 系统图标索引上，不会每帧重算。

#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <cstring>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

// 创建 width x height 的预乘 32 位 top-down DIB，bits 返回像素指针（可为 nullptr）
inline HBITMAP CreatePargbDibSection(const int width, const int height, void** bits) {
	if (width <= 0 || height <= 0) return nullptr;
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* localBits = nullptr;
	HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &localBits, nullptr, 0);
	if (!bitmap || !localBits) {
		if (bitmap) DeleteObject(bitmap);
		return nullptr;
	}
	std::memset(localBits, 0, static_cast<size_t>(width) * height * 4);
	if (bits) *bits = localBits;
	return bitmap;
}

// 把 GDI+ 位图复制成预乘 DIB
inline HBITMAP GdiplusBitmapToPargbDib(Gdiplus::Bitmap& source) {
	const int width = static_cast<int>(source.GetWidth());
	const int height = static_cast<int>(source.GetHeight());
	void* bits = nullptr;
	HBITMAP bitmap = CreatePargbDibSection(width, height, &bits);
	if (!bitmap) return nullptr;
	Gdiplus::BitmapData data{};
	Gdiplus::Rect bounds(0, 0, width, height);
	if (source.LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &data) != Gdiplus::Ok) {
		DeleteObject(bitmap);
		return nullptr;
	}
	const size_t rowBytes = static_cast<size_t>(width) * 4;
	for (int y = 0; y < height; ++y) {
		std::memcpy(static_cast<BYTE*>(bits) + y * rowBytes,
					static_cast<const BYTE*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride, rowBytes);
	}
	source.UnlockBits(&data);
	return bitmap;
}

// 读取任意 HBITMAP 的 32 位像素（top-down）。全部 alpha 为 0 的位图（DDB、没有 alpha 的图）视为不透明
inline bool ReadBitmapPixels32(const HBITMAP source, int& width, int& height, std::vector<BYTE>& pixels) {
	BITMAP bm{};
	if (!source || !GetObject(source, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) return false;
	width = bm.bmWidth;
	height = bm.bmHeight;
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	pixels.assign(static_cast<size_t>(width) * height * 4, 0);
	const HDC dc = CreateCompatibleDC(nullptr);
	if (!dc) return false;
	const int lines = GetDIBits(dc, source, 0, static_cast<UINT>(height), pixels.data(), &info, DIB_RGB_COLORS);
	DeleteDC(dc);
	if (lines != height) return false;
	bool anyAlpha = false;
	for (size_t i = 3; i < pixels.size(); i += 4) {
		if (pixels[i] != 0) {
			anyAlpha = true;
			break;
		}
	}
	if (!anyAlpha) {
		for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 0xFF;
	}
	return true;
}

// 把 PARGB 像素缩放到 size x size，返回新的预乘 DIB
inline HBITMAP ScalePargbPixelsToDib(BYTE* pixels, const int width, const int height, const int size) {
	if (!pixels || width <= 0 || height <= 0 || size <= 0) return nullptr;
	Gdiplus::Bitmap source(width, height, width * 4, PixelFormat32bppPARGB, pixels);
	Gdiplus::Bitmap target(size, size, PixelFormat32bppPARGB);
	if (source.GetLastStatus() != Gdiplus::Ok || target.GetLastStatus() != Gdiplus::Ok) return nullptr;
	Gdiplus::Graphics graphics(&target);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
	Gdiplus::ImageAttributes attributes;
	attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
	graphics.DrawImage(&source, Gdiplus::Rect(0, 0, size, size), 0, 0, width, height, Gdiplus::UnitPixel, &attributes);
	graphics.Flush(Gdiplus::FlushIntentionSync);
	return GdiplusBitmapToPargbDib(target);
}

// 把任意 32 位 HBITMAP（插件给的图标位图）缩放到 size x size
inline HBITMAP ScaleBitmapToPargbDib(const HBITMAP source, const int size) {
	int width = 0, height = 0;
	std::vector<BYTE> pixels;
	if (!ReadBitmapPixels32(source, width, height, pixels)) return nullptr;
	return ScalePargbPixelsToDib(pixels.data(), width, height, size);
}

// 把图像列表的一项原尺寸画到一张透明 DIB 上，返回像素；entrySize 返回该列表的图标边长
inline bool ReadImageListEntryPixels(IImageList* list, const int index, int& entrySize, std::vector<BYTE>& pixels) {
	int cx = 0, cy = 0;
	if (!list || index < 0 || FAILED(list->GetIconSize(&cx, &cy)) || cx <= 0 || cy <= 0) return false;
	entrySize = cx;
	void* bits = nullptr;
	HBITMAP bitmap = CreatePargbDibSection(cx, cy, &bits);
	if (!bitmap) return false;
	const HDC dc = CreateCompatibleDC(nullptr);
	if (!dc) {
		DeleteObject(bitmap);
		return false;
	}
	const HGDIOBJ old = SelectObject(dc, bitmap);
	// HIMAGELIST 就是 IImageList*，项目里 GetSystemImageListEx 也是这样转换的
	ImageList_Draw(reinterpret_cast<HIMAGELIST>(list), index, dc, 0, 0, ILD_TRANSPARENT);
	GdiFlush();
	pixels.assign(static_cast<const BYTE*>(bits), static_cast<const BYTE*>(bits) + static_cast<size_t>(cx) * cy * 4);
	SelectObject(dc, old);
	DeleteDC(dc);
	DeleteObject(bitmap);
	return true;
}

// 系统图标缩放到 size：优先用 256px 的 SHIL_JUMBO 缩小（清晰）。
// 没有 256px 版本的图标在 jumbo 列表里只是把 48px 的图画在角落，这种情况退回用 SHIL_EXTRALARGE（48px）放大。
inline HBITMAP CreateScaledSystemIconBitmap(IImageList* jumbo, IImageList* extraLarge, const int index, const int size) {
	if (index < 0 || size <= 0) return nullptr;
	std::vector<BYTE> pixels;
	int entrySize = 0;
	if (jumbo && ReadImageListEntryPixels(jumbo, index, entrySize, pixels) && entrySize > size) {
		// 不透明像素的包围盒：只占画布一小角的说明没有大图
		int minX = entrySize, minY = entrySize, maxX = -1, maxY = -1;
		for (int y = 0; y < entrySize; ++y) {
			const BYTE* row = pixels.data() + static_cast<size_t>(y) * entrySize * 4;
			for (int x = 0; x < entrySize; ++x) {
				if (row[x * 4 + 3] == 0) continue;
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
		}
		const bool degenerate = maxX < 0 || ((maxX - minX + 1) * 3 <= entrySize && (maxY - minY + 1) * 3 <= entrySize);
		if (!degenerate) return ScalePargbPixelsToDib(pixels.data(), entrySize, entrySize, size);
	}
	if (extraLarge && ReadImageListEntryPixels(extraLarge, index, entrySize, pixels)) {
		return ScalePargbPixelsToDib(pixels.data(), entrySize, entrySize, size);
	}
	return nullptr;
}
