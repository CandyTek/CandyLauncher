#pragma once

#include <windows.h>
#include <vector>

#include <gdiplus.h>
#include "common/GlobalState.hpp"
#include "BackgroundLayerPainter.hpp"

inline UINT_PTR g_caretTimerId = 1001;
inline bool g_caretOn = true; // 由定时器翻转
inline UINT g_caretInterval = 0; // GetCaretBlinkTime() 的返回值缓存

static Gdiplus::Color GetColor(const char* key, const char* def) {
	return HexToGdiplusColor(g_skinJson != nullptr ? g_skinJson.value(key, def) : def);
}

// 光标颜色：皮肤未单独指定时跟随输入文字颜色
static Gdiplus::Color GetCaretColor() {
	const bool hasCaretColor = g_skinJson != nullptr && g_skinJson.contains("editbox_caret_color");
	return GetColor(hasCaretColor ? "editbox_caret_color" : "editbox_font_color", "#222222");
}

static void drawBackground(HWND hwnd, Gdiplus::Graphics& graphics, const RECT& rc) {
	const Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
	if (g_skinJson == nullptr) {
		const Gdiplus::SolidBrush bgBrush(HexToGdiplusColor("#EEEEEE"));
		graphics.FillRectangle(&bgBrush, rect);
		return;
	}
	paintMainWindowBackgroundUnderChild(hwnd, graphics, rc);
	if (g_editBgCachedBitmap) {
		graphics.DrawCachedBitmap(g_editBgCachedBitmap, rc.left, rc.top);
	} else if (g_editBgImage) {
		graphics.DrawImage(g_editBgImage, rect);
	} else {
		// 没有背景图则填充纯色背景
		const Gdiplus::SolidBrush bgBrush(GetColor("editbox_bg_color", "#FFFFFF"));
		graphics.FillRectangle(&bgBrush, rect);
	}
}

// 每个字符的 x 坐标直接取自原生编辑框（EM_POSFROMCHAR，已包含滚动偏移和边距），
// 保证绘制位置与原生的鼠标命中、滚动、光标计算完全一致。
// 若把整串交给 GDI+ 排版，其字宽与 GDI 不同，文本越长偏差越大
static std::vector<Gdiplus::REAL> GetCharPositions(HWND hwnd, HFONT hFont, const wchar_t* text, const int length) {
	std::vector<Gdiplus::REAL> xs(length + 1);
	for (int i = 0; i < length; ++i) {
		xs[i] = static_cast<Gdiplus::REAL>(static_cast<short>(LOWORD(SendMessageW(hwnd, EM_POSFROMCHAR, i, 0))));
	}
	// EM_POSFROMCHAR 对末尾位置返回 -1，用 GDI 测量最后一个字符的宽度补上
	int lastStart = length - 1;
	if (lastStart > 0 && IS_LOW_SURROGATE(text[lastStart]) && IS_HIGH_SURROGATE(text[lastStart - 1])) --lastStart;
	const HDC hdc = GetDC(hwnd);
	const HGDIOBJ oldFont = SelectObject(hdc, hFont);
	SIZE sz{};
	GetTextExtentPoint32W(hdc, text + lastStart, length - lastStart, &sz);
	SelectObject(hdc, oldFont);
	ReleaseDC(hwnd, hdc);
	xs[length] = xs[lastStart] + static_cast<Gdiplus::REAL>(sz.cx);
	return xs;
}

// 把绘制主体抽出来，供 WM_PAINT / WM_PRINTCLIENT 调用
static void PaintEdit(const HWND hwnd, const HDC hdc) {
	RECT rc;
	GetClientRect(hwnd, &rc);

	Gdiplus::Graphics graphics(hdc);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

	const HFONT hFont = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
	const Gdiplus::Font font(hdc, hFont);

	drawBackground(hwnd, graphics, rc);

	// 内边距：左右由原生 EM_SETMARGINS 负责（见 SkinHelper），这里只取左边距用于空文本时的 hint/光标；
	// 单行编辑框没有原生的上边距，editbox_padding_top 在绘制时整体下移实现
	const Gdiplus::REAL paddingLeft = static_cast<Gdiplus::REAL>(LOWORD(SendMessageW(hwnd, EM_GETMARGINS, 0, 0)));
	const Gdiplus::REAL paddingTop = g_skinJson != nullptr
		? static_cast<Gdiplus::REAL>(g_skinJson.value("editbox_padding_top", 0))
		: 0.f;
	const Gdiplus::REAL fontH = font.GetHeight(&graphics);

	wchar_t buffer[1024];
	const int textLength = GetWindowTextW(hwnd, buffer, static_cast<int>(std::size(buffer)));

	DWORD selStart = 0, selEnd = 0;
	SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
	const int s = (std::min)(static_cast<int>((std::min)(selStart, selEnd)), textLength);
	const int e = (std::min)(static_cast<int>((std::max)(selStart, selEnd)), textLength);

	// Typographic 格式不带 GDI+ 默认的左右内边距，字形原点即为给定坐标
	Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());

	// 空文本时光标位于左边距处
	Gdiplus::REAL caretX = paddingLeft;
	if (textLength > 0) {
		const std::vector<Gdiplus::REAL> xs = GetCharPositions(hwnd, hFont, buffer, textLength);
		caretX = xs[s];
		format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsNoClip |
			Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

		// 逐字符绘制到原生坐标上，只绘制可见范围
		const auto drawChars = [&](const Gdiplus::Color& color, const int from, const int to) {
			const Gdiplus::SolidBrush brush(color);
			for (int i = from; i < to;) {
				const int n = (IS_HIGH_SURROGATE(buffer[i]) && i + 1 < to && IS_LOW_SURROGATE(buffer[i + 1])) ? 2 : 1;
				if (buffer[i] != L' ' && xs[i + n] >= 0.f && xs[i] <= static_cast<Gdiplus::REAL>(rc.right)) {
					graphics.DrawString(buffer + i, n, &font, Gdiplus::PointF(xs[i], paddingTop), &format, &brush);
				}
				i += n;
			}
		};

		const Gdiplus::Color fontColor = GetColor("editbox_font_color", "#222222");
		drawChars(fontColor, 0, s);
		if (s != e) {
			const Gdiplus::SolidBrush selectionBgBrush(GetColor("editbox_selection_bg_color", "#3399FF"));
			graphics.FillRectangle(&selectionBgBrush, Gdiplus::RectF(xs[s], paddingTop + 2.f, xs[e] - xs[s], fontH - 2.f));
			drawChars(GetColor("editbox_selection_font_color", "#FFFFFF"), s, e);
		}
		drawChars(fontColor, e, textLength);
	} else if (!EDIT_HINT_TEXT.empty()) {
		// 文本为空时显示提示文本，与输入文字起点对齐
		const Gdiplus::SolidBrush hintBrush(GetColor("editbox_hint_color", "#888888"));
		const Gdiplus::RectF hintRect(paddingLeft, paddingTop,
									static_cast<Gdiplus::REAL>(rc.right) - paddingLeft,
									static_cast<Gdiplus::REAL>(rc.bottom) - paddingTop);
		graphics.DrawString(EDIT_HINT_TEXT.c_str(), -1, &font, hintRect, &format, &hintBrush);
	}

	// 有选区时不绘制光标
	if (s == e && g_caretOn && GetFocus() == hwnd) {
		const Gdiplus::SolidBrush caretBrush(GetCaretColor());
		graphics.FillRectangle(&caretBrush, Gdiplus::RectF(caretX, paddingTop + fontH * 0.1f, 2.f, fontH * 0.9f));
	}
}
