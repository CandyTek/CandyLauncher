#pragma once
#include <windows.h>
#include <memory>
#include <string>

#include "util/BitmapUtil.hpp"
#include "qrcodegen.hpp"
#include <gdiplus.h>


// 从剪贴板获取文本内容并转换为UTF-8
inline std::string GetClipboardTextAsUTF8() {
	if (!OpenClipboard(NULL)) {
		return "";
	}

	std::string result;
	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData) {
		wchar_t* pwszText = static_cast<wchar_t*>(GlobalLock(hData));
		if (pwszText) {
			// 转换 Unicode 到 UTF-8
			int size = WideCharToMultiByte(CP_UTF8, 0, pwszText, -1, NULL, 0, NULL, NULL);
			if (size > 0) {
				std::vector<char> buffer(size);
				WideCharToMultiByte(CP_UTF8, 0, pwszText, -1, buffer.data(), size, NULL, NULL);
				result = buffer.data();
			}
			GlobalUnlock(hData);
		}
	}
	CloseClipboard();
	return result;
}

// 将文本写入剪贴板
inline bool SetClipboardText(const std::string& text) {
	if (!OpenClipboard(NULL)) {
		return false;
	}

	EmptyClipboard();

	// 转换 UTF-8 到 Unicode
	int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
	if (size <= 0) {
		CloseClipboard();
		return false;
	}

	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size * sizeof(wchar_t));
	if (!hMem) {
		CloseClipboard();
		return false;
	}

	wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
	if (pMem) {
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, size);
		GlobalUnlock(hMem);
		SetClipboardData(CF_UNICODETEXT, hMem);
	} else {
		GlobalFree(hMem);
		CloseClipboard();
		return false;
	}

	CloseClipboard();
	return true;
}

// 去除字符串首尾空格
inline std::string TrimString(const std::string& str) {
	size_t start = 0;
	size_t end = str.length();

	while (start < end && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r' || str[start] == '\n')) {
		start++;
	}

	while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r' || str[end - 1] == '\n')) {
		end--;
	}

	return str.substr(start, end - start);
}

// 检查是否是有效的十六进制颜色格式 #RRGGBB
inline bool IsHexColor(const std::string& str) {
	if (str.length() != 7 || str[0] != '#') return false;

	for (size_t i = 1; i < 7; i++) {
		char c = str[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
			return false;
		}
	}
	return true;
}

// 检查是否是有效的 RGB 格式 R,G,B
inline bool IsRgbColor(const std::string& str) {
	size_t comma1 = str.find(',');
	size_t comma2 = str.find(',', comma1 + 1);

	if (comma1 == std::string::npos || comma2 == std::string::npos) return false;
	if (str.find(',', comma2 + 1) != std::string::npos) return false; // 不能有第三个逗号

	try {
		std::string r = str.substr(0, comma1);
		std::string g = str.substr(comma1 + 1, comma2 - comma1 - 1);
		std::string b = str.substr(comma2 + 1);

		int rVal = std::stoi(r);
		int gVal = std::stoi(g);
		int bVal = std::stoi(b);

		return (rVal >= 0 && rVal <= 255) && (gVal >= 0 && gVal <= 255) && (bVal >= 0 && bVal <= 255);
	} catch (...) {
		return false;
	}
}

// 将 #RRGGBB 转换为 R,G,B
inline std::string HexToRgb(const std::string& hex) {
	int r = std::stoi(hex.substr(1, 2), nullptr, 16);
	int g = std::stoi(hex.substr(3, 2), nullptr, 16);
	int b = std::stoi(hex.substr(5, 2), nullptr, 16);

	return std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b);
}

// 将 R,G,B 转换为 #RRGGBB
inline std::string RgbToHex(const std::string& rgb) {
	size_t comma1 = rgb.find(',');
	size_t comma2 = rgb.find(',', comma1 + 1);

	int r = std::stoi(rgb.substr(0, comma1));
	int g = std::stoi(rgb.substr(comma1 + 1, comma2 - comma1 - 1));
	int b = std::stoi(rgb.substr(comma2 + 1));

	char hex[8];
	sprintf_s(hex, 8, "#%02X%02X%02X", r, g, b);
	return std::string(hex);
}

// --------- 用 qrcodegen 生成 GDI+ Bitmap ----------
inline std::unique_ptr<Gdiplus::Bitmap> MakeQrBitmap(
	const qrcodegen::QrCode& qr,
	int pixelsPerModule = 8, // 基础像素密度（最终会按窗口再缩放）
	int quietZoneModules = 4,
	Gdiplus::Color fg = Gdiplus::Color(255, 0, 0, 0),
	Gdiplus::Color bg = Gdiplus::Color(255, 255, 255, 255)
) {
	using namespace Gdiplus;
	const int qrSize = qr.getSize();
	const int sideModules = qrSize + quietZoneModules * 2;
	const int sidePx = sideModules * pixelsPerModule;

	auto bmp = std::make_unique<Bitmap>(sidePx, sidePx, PixelFormat32bppARGB);

	BitmapData bd{};
	Gdiplus::Rect r(0, 0, sidePx, sidePx);
	bmp->LockBits(&r, ImageLockModeWrite, PixelFormat32bppARGB, &bd);

	auto* base = static_cast<BYTE*>(bd.Scan0);
	const UINT stride = static_cast<UINT>(bd.Stride);

	const UINT bgA = bg.GetA(), bgR = bg.GetR(), bgG = bg.GetG(), bgB = bg.GetB();
	const UINT fgA = fg.GetA(), fgR = fg.GetR(), fgG = fg.GetG(), fgB = fg.GetB();

	for (int y = 0; y < sidePx; ++y) {
		BYTE* row = base + y * stride;
		const int modY = (y / pixelsPerModule) - quietZoneModules;
		for (int x = 0; x < sidePx; ++x) {
			const int modX = (x / pixelsPerModule) - quietZoneModules;
			bool on = (modX >= 0 && modX < qrSize &&
					   modY >= 0 && modY < qrSize &&
					   qr.getModule(modX, modY));
			BYTE* p = row + x * 4; // BGRA
			if (on) {
				p[0] = (BYTE)fgB; p[1] = (BYTE)fgG; p[2] = (BYTE)fgR; p[3] = (BYTE)fgA;
			} else {
				p[0] = (BYTE)bgB; p[1] = (BYTE)bgG; p[2] = (BYTE)bgR; p[3] = (BYTE)bgA;
			}
		}
	}
	bmp->UnlockBits(&bd);
	return bmp;
}
// --------- 可选：Bitmap -> HBITMAP/HICON ----------
inline HBITMAP CreateHBITMAPFromBitmap(Gdiplus::Bitmap* bmp) {
	HBITMAP hbm{};
	Gdiplus::Color bg(255, 255, 255, 255);
	bmp->GetHBITMAP(bg, &hbm);
	return hbm;
}
inline HICON CreateHICONFromBitmap(Gdiplus::Bitmap* bmp) {
	// 直接用图标：先 HBITMAP，再组 ICON
	HBITMAP color = CreateHBITMAPFromBitmap(bmp);
	ICONINFO ii{};
	ii.fIcon = TRUE;
	ii.hbmColor = color;
	// 透明蒙版给个空白 1x1
	HBITMAP mask = CreateBitmap(1, 1, 1, 1, nullptr);
	ii.hbmMask = mask;
	HICON hi = CreateIconIndirect(&ii);
	DeleteObject(mask);
	DeleteObject(color);
	return hi;
}
// --------- 窗口数据 ----------
struct QRCodeWindowData {
	std::unique_ptr<Gdiplus::Bitmap> bmp;
	std::wstring text;
	ULONG_PTR gdipToken{};
};

// --------- 窗口过程 ----------
static LRESULT CALLBACK QRCodeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* pData = reinterpret_cast<QRCodeWindowData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_ERASEBKGND:
        // 用双缓冲自己画，避免背景擦除造成闪烁
        return 1;

    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT: {
        if (!pData || !pData->bmp) break;

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // 双缓冲
        RECT rc; GetClientRect(hwnd, &rc);
        const int cw = rc.right - rc.left;
        const int ch = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBM = CreateCompatibleBitmap(hdc, cw, ch);
        HGDIOBJ old = SelectObject(memDC, memBM);
        HBRUSH white = (HBRUSH)GetStockObject(WHITE_BRUSH);
        FillRect(memDC, &rc, white);

        // 用 GDI+ 在 memDC 上画
        Gdiplus::Graphics g(memDC);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
        g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
        g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        // 预留文本高度（估算）
        int margin = 16;
        int textPad = pData->text.empty() ? 0 : 80;

        // 计算目标方形区域（保持等比）
        int availW = std::max(1, cw - margin * 2);
        int availH = std::max(1, ch - margin * 2 - textPad);
        int side = std::min(availW, availH);
        int dx = (cw - side) / 2;
        int dy = margin + (availH - side) / 2;

        Gdiplus::Rect dest(dx, dy, side, side);
        g.DrawImage(pData->bmp.get(), dest);

        // 画文本（居中换行）
        if (!pData->text.empty()) {
            Gdiplus::RectF textRc(
                (Gdiplus::REAL)margin,
                (Gdiplus::REAL)(dy + side + 8),
                (Gdiplus::REAL)(cw - margin * 2),
                (Gdiplus::REAL)(ch - (dy + side + 8) - margin)
            );
            Gdiplus::FontFamily ff(L"Segoe UI");
            Gdiplus::Font font(&ff, 14.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentNear);
            fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisWord);
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, 0, 0, 0));
            g.DrawString(pData->text.c_str(), -1, &font, textRc, &fmt, &brush);
        }

        // 拷贝到前台
        BitBlt(hdc, 0, 0, cw, ch, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, old);
        DeleteObject(memBM);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (pData) {
            // 关闭 GDI+
            if (pData->gdipToken) Gdiplus::GdiplusShutdown(pData->gdipToken);
            delete pData;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --------- 显示窗口（自适应缩放） ----------
inline void ShowQRCodeWindow(const qrcodegen::QrCode& qr, const std::string& textUtf8) {
    // 初始化 GDI+
    Gdiplus::GdiplusStartupInput gsi;
    ULONG_PTR token{};
    if (Gdiplus::GdiplusStartup(&token, &gsi, nullptr) != Gdiplus::Ok) return;

    // 先生成一个足够清晰的底图（最终会用最近邻缩放）
    auto bmp = MakeQrBitmap(qr, /*pixelsPerModule=*/10, /*quietZoneModules=*/4);

    // 组装窗口数据
    auto* data = new QRCodeWindowData;
    data->bmp = std::move(bmp);
    data->text = utf8_to_wide(textUtf8);
    data->gdipToken = token;

    // 注册类
    const wchar_t* kClass = L"QRCodeGDIPlusWinClass";
    WNDCLASSW wc{};
    wc.lpfnWndProc = QRCodeWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    UnregisterClassW(kClass, wc.hInstance);
    RegisterClassW(&wc);

    // 建一个合适的初始大小，后续用户拖动自动缩放
    int w = 520, h = 560; // 上下左右留白 + 预留文本
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClass, L"二维码",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        nullptr, nullptr, wc.hInstance, data
    );
    if (!hwnd) {
        Gdiplus::GdiplusShutdown(token);
        delete data;
        return;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}
