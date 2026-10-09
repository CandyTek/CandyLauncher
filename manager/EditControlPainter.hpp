#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <Shlwapi.h>

#include <gdiplus.h>
#include "common/GlobalState.hpp"
#include "BackgroundLayerPainter.hpp"

inline UINT_PTR g_caretTimerId = 1001;
inline bool g_caretOn = true; // 由定时器翻转
inline UINT g_caretInterval = 0; // GetCaretBlinkTime() 的返回值缓存

static Gdiplus::Color GetColor(const char* key, const char* def) {
	return g_skinJson != nullptr
				? HexToGdiplusColor(g_skinJson.value(key, def))
				: HexToGdiplusColor(def);
}

// 光标颜色：皮肤未单独指定时跟随输入文字颜色
static Gdiplus::Color GetCaretColor() {
	if (g_skinJson != nullptr && g_skinJson.contains("editbox_caret_color")) {
		return GetColor("editbox_caret_color", "#222222");
	}
	return GetColor("editbox_font_color", "#222222");
}

static void drawBackground(HWND hwnd, Gdiplus::Graphics& graphics, RECT rc) {
	if (g_skinJson != nullptr) {
		paintMainWindowBackgroundUnderChild(hwnd, graphics, rc);
		if (g_editBgCachedBitmap) {
			graphics.DrawCachedBitmap(g_editBgCachedBitmap, rc.left, rc.top);
		} else if (g_editBgImage) {
			const Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
			graphics.DrawImage(g_editBgImage, rect);
		} else {
			// 如果没有背景图，则填充纯色背景
			const Gdiplus::SolidBrush bgBrush(GetColor("editbox_bg_color", "#FFFFFF"));
			const Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
			graphics.FillRectangle(&bgBrush, rect);
		}
	} else {
		const Gdiplus::SolidBrush bgBrush(HexToGdiplusColor("#EEEEEE"));
		const Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
		graphics.FillRectangle(&bgBrush, rect);
	}
}


// 把绘制主体抽出来，供 WM_PAINT / WM_PRINTCLIENT 调用
static void PaintEdit(const HWND hwnd, const HDC hdc) {
	// 开始完全自定义绘制
	RECT rc;
	GetClientRect(hwnd, &rc);

	// 创建GDI+绘图对象
	Gdiplus::Graphics graphics(hdc);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
	// graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeHighQualityBicubic);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

	// 获取字体信息
	const HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
	const Gdiplus::Font font(hdc, hFont);

	// 绘制背景
	drawBackground(hwnd, graphics, rc);

	// 内边距：左右由原生 EM_SETMARGINS 负责（见 SkinHelper），这里只取左边距用于空文本时的 hint/光标；
	// 单行编辑框没有原生的上边距，editbox_padding_top 在绘制时整体下移实现
	const Gdiplus::REAL paddingLeft = static_cast<Gdiplus::REAL>(LOWORD(SendMessage(hwnd, EM_GETMARGINS, 0, 0)));
	const Gdiplus::REAL paddingTop = g_skinJson != nullptr
		? static_cast<Gdiplus::REAL>(g_skinJson.value("editbox_padding_top", 0))
		: 0.f;
	const Gdiplus::REAL fontH = font.GetHeight(&graphics);

	wchar_t buffer[1024];
	const int textLength = GetWindowTextW(hwnd, buffer, static_cast<int>(std::size(buffer)));

	// 获取选区信息
	DWORD selStart, selEnd;
	SendMessage(hwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

	if (textLength > 0) {
		// 每个字符的 x 坐标直接取自原生编辑框（EM_POSFROMCHAR，已包含滚动偏移和边距），
		// 保证绘制位置与原生的鼠标命中、滚动、光标计算完全一致。
		// 若把整串交给 GDI+ 排版，其字宽与 GDI 不同，文本越长偏差越大
		std::vector<Gdiplus::REAL> xs(textLength + 1);
		for (int i = 0; i < textLength; ++i) {
			const LRESULT pos = SendMessage(hwnd, EM_POSFROMCHAR, i, 0);
			xs[i] = static_cast<Gdiplus::REAL>(static_cast<short>(LOWORD(pos)));
		}
		// EM_POSFROMCHAR 对末尾位置返回 -1，用 GDI 测量最后一个字符的宽度补上
		{
			int lastStart = textLength - 1;
			if (lastStart > 0 && IS_LOW_SURROGATE(buffer[lastStart]) && IS_HIGH_SURROGATE(buffer[lastStart - 1])) --lastStart;
			const HDC measureDc = GetDC(hwnd);
			const HGDIOBJ oldFont = SelectObject(measureDc, hFont);
			SIZE sz{};
			GetTextExtentPoint32W(measureDc, buffer + lastStart, textLength - lastStart, &sz);
			SelectObject(measureDc, oldFont);
			ReleaseDC(hwnd, measureDc);
			xs[textLength] = xs[lastStart] + static_cast<Gdiplus::REAL>(sz.cx);
		}

		// Typographic 格式不带 GDI+ 默认的左右内边距，字形原点即为给定坐标
		Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
		format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsNoClip |
			Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

		// 逐字符绘制到原生坐标上，只绘制可见范围
		const auto drawChars = [&](const Gdiplus::Brush& brush, const int from, const int to) {
			for (int i = from; i < to;) {
				const int n = (IS_HIGH_SURROGATE(buffer[i]) && i + 1 < to && IS_LOW_SURROGATE(buffer[i + 1])) ? 2 : 1;
				if (buffer[i] != L' ' && xs[i + n] >= 0.f && xs[i] <= static_cast<Gdiplus::REAL>(rc.right)) {
					graphics.DrawString(buffer + i, n, &font, Gdiplus::PointF(xs[i], paddingTop), &format, &brush);
				}
				i += n;
			}
		};

		const Gdiplus::SolidBrush fontBrush(GetColor("editbox_font_color", "#222222"));
		if (selStart != selEnd) {
			const int s = static_cast<int>((std::min)((std::min)(selStart, selEnd), static_cast<DWORD>(textLength)));
			const int e = static_cast<int>((std::min)((std::max)(selStart, selEnd), static_cast<DWORD>(textLength)));

			// 1) 非选中部分按普通颜色绘制
			drawChars(fontBrush, 0, s);
			drawChars(fontBrush, e, textLength);

			// 2) 填选区背景
			const Gdiplus::SolidBrush selectionBgBrush(GetColor("editbox_selection_bg_color", "#3399FF"));
			const Gdiplus::RectF selBox(xs[s], paddingTop + 2.f, xs[e] - xs[s], fontH - 2.f);
			graphics.FillRectangle(&selectionBgBrush, selBox);

			// 3) 选区内文字用“选中文字色”绘制
			const Gdiplus::SolidBrush selectionFontBrush(GetColor("editbox_selection_font_color", "#FFFFFF"));
			drawChars(selectionFontBrush, s, e);
		} else {
			drawChars(fontBrush, 0, textLength);

			// 绘制光标：位置同样取自原生坐标
			if (GetFocus() == hwnd && g_caretOn) {
				const Gdiplus::SolidBrush caretBrush(GetCaretColor());
				const int caretIndex = static_cast<int>((std::min)(selStart, static_cast<DWORD>(textLength)));
				const Gdiplus::RectF caretRect(xs[caretIndex], paddingTop + fontH * 0.1f, 2.f, fontH - (fontH * 0.1f));
				graphics.FillRectangle(&caretBrush, caretRect);
			}
		}
	} else {
		// 文本为空时显示提示文本，但只在控件没有焦点时显示
		if (!EDIT_HINT_TEXT.empty()) {
			const Gdiplus::SolidBrush hintBrush(GetColor("editbox_hint_color", "#888888"));

			// hint 与输入文字起点对齐（同为左边距处，无 GDI+ 默认内边距）
			Gdiplus::StringFormat hintFormat(Gdiplus::StringFormat::GenericTypographic());
			hintFormat.SetAlignment(Gdiplus::StringAlignmentNear);

			const Gdiplus::RectF hintRect(
				paddingLeft,
				paddingTop,
				static_cast<Gdiplus::REAL>(rc.right) - paddingLeft,
				static_cast<Gdiplus::REAL>(rc.bottom) - paddingTop
			);

			graphics.DrawString(EDIT_HINT_TEXT.c_str(), -1, &font, hintRect, &hintFormat, &hintBrush);
		}
	}

	// 文本为空时绘制光标
	if (textLength == 0 && GetFocus() == hwnd && g_caretOn) {
		const Gdiplus::SolidBrush caretBrush(GetCaretColor());
		const Gdiplus::RectF caretRect(paddingLeft, paddingTop + fontH * 0.1f, 2.f, fontH - (fontH * 0.1f));
		graphics.FillRectangle(&caretBrush, caretRect);
	}
}
