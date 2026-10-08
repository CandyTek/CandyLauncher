#pragma once

#include <lunasvg.h>
#include <windows.h>
#include <string>
#include <ShlObj.h>
#include <vector>
#include <commoncontrols.h>
#include <mutex>

#include "util/StringUtil.hpp"

static HBITMAP MyLoadSvgAsHBITMAP(
    const wchar_t* absolutePath,
    UINT targetWidth,
    UINT targetHeight
) {
    if (!absolutePath || targetWidth == 0 || targetHeight == 0)
        return nullptr;

    auto document = lunasvg::Document::loadFromFile(wide_to_utf8(absolutePath));
    if (!document)
        return nullptr;

    auto bitmap = document->renderToBitmap(targetWidth, targetHeight);
    if (bitmap.isNull())
        return nullptr;

    // Create top-down 32bpp HBITMAP.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(targetWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(targetHeight); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(
        nullptr,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0
    );

    if (!hBitmap || !bits) {
        return nullptr;
    }

    // lunasvg uses ARGB32 premultiplied (BGRA in memory on little-endian)
    // which matches Windows DIB format.
    const int stride = bitmap.stride();
    const int dstStride = targetWidth * 4;
    for (UINT y = 0; y < targetHeight; ++y) {
        memcpy(static_cast<BYTE*>(bits) + y * dstStride, bitmap.data() + y * stride, dstStride);
    }

    return hBitmap;
}
