#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>

#include <gdiplus.h>
#include "DataKeeper.hpp"
#include "EditCustomPaint.hpp"

static void drawBackground(HWND hwnd, Gdiplus::Graphics &graphics, RECT rc) {
	if (g_skinJson != nullptr) {
		if (g_editBgImage) {
			Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
			graphics.DrawImage(g_editBgImage, rect);
		} else {
			// 如果没有背景图，则填充纯色背景
			std::string bgColorStr = g_skinJson.value("editbox_bg_color", "#FFFFFF");
			if (!bgColorStr.empty()) {
				Gdiplus::SolidBrush bgBrush(HexToGdiplusColor(bgColorStr));
				Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
				graphics.FillRectangle(&bgBrush, rect);
			}
		}
	} else {
		Gdiplus::SolidBrush bgBrush(HexToGdiplusColor("#EEEEEE"));
		Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
		graphics.FillRectangle(&bgBrush, rect);
	}
}


// 把你的绘制主体抽出来，供 WM_PAINT / WM_PRINTCLIENT 调用，不使用 DirectWrite 的版本
static void PaintEdit(HWND hwnd, HDC hdc) {
	// 开始完全自定义绘制
	RECT rc;
	GetClientRect(hwnd, &rc);

	// 创建GDI+绘图对象
	Gdiplus::Graphics graphics(hdc);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
	graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeHighQualityBicubic);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

	// 获取字体信息
	HFONT hFont = (HFONT) SendMessage(hwnd, WM_GETFONT, 0, 0);
	Gdiplus::Font font(hdc, hFont);

	// 绘制背景
	drawBackground(hwnd, graphics, rc);

	// 准备绘制文本
	TCHAR buffer[1024];
	int textLength = GetWindowText(hwnd, buffer, sizeof(buffer) / sizeof(buffer[0]));

	// 计算绘制位置（考虑滚动位置）
	DWORD firstChar = (DWORD) SendMessage(hwnd, EM_CHARFROMPOS, 0, 0);
	int xOffset = LOWORD(SendMessage(hwnd, EM_POSFROMCHAR, firstChar, 0));

	// 设置文本布局
	Gdiplus::StringFormat format;
	format.SetTrimming(Gdiplus::StringTrimmingNone);
	format.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);

	if (textLength > 0) {
		// 有文本时绘制正常文本和选区
		Gdiplus::Color fontColor;
		if (g_skinJson != nullptr) {
			fontColor = HexToGdiplusColor(g_skinJson.value("editbox_font_color", "#000000"));
		} else {
			fontColor = Gdiplus::Color(0xFF, 0x22, 0x22, 0x22);
		}
		Gdiplus::SolidBrush fontBrush(fontColor);

		// 获取选区信息
		DWORD selStart, selEnd;
		SendMessage(hwnd, EM_GETSEL, (WPARAM) &selStart, (LPARAM) &selEnd);

		if (selStart != selEnd) {
			Gdiplus::RectF layout(static_cast<Gdiplus::REAL>(-xOffset), 0,
								  static_cast<Gdiplus::REAL>(rc.right + abs(xOffset)),
								  static_cast<Gdiplus::REAL>(rc.bottom));

			// 1) 先把整段文字按普通颜色画一次（非选中状态）
			std::wstring text(buffer);
			graphics.DrawString(buffer, -1, &font, layout, &format, &fontBrush);

			// 2) 用 CharacterRanges 测到“选中区”在同一 layout 下的真实边界
			Gdiplus::CharacterRange range((INT)selStart, (INT)(selEnd - selStart));
			format.SetMeasurableCharacterRanges(1, &range);
			Gdiplus::Region region;
			graphics.MeasureCharacterRanges(buffer, -1, &font, layout, &format, 1, &region);

			Gdiplus::RectF selBox;
			region.GetBounds(&selBox, &graphics);
			// 选区边缘会有残留影像，要调整一下才行
			selBox.X += 1;
			selBox.Y += 2;
			selBox.Height -= 2;
			// 3) 填选区背景
			Gdiplus::Color selectionBgColor = (g_skinJson != nullptr)
											  ? HexToGdiplusColor(g_skinJson.value("editbox_selection_bg_color", "#3399FF"))
											  : Gdiplus::Color(0xFF, 0x33, 0x99, 0xFF);
			Gdiplus::SolidBrush selectionBgBrush(selectionBgColor);
			graphics.FillRectangle(&selectionBgBrush, selBox);

			// 4) 只在选区内重绘文字为“选中文字色”
			Gdiplus::Color selectionFontColor = (g_skinJson != nullptr)
												? HexToGdiplusColor(g_skinJson.value("editbox_selection_font_color", "#FFFFFF"))
												: Gdiplus::Color(0xFF, 0xFF, 0xFF, 0xFF);
			Gdiplus::SolidBrush selectionFontBrush(selectionFontColor);

			graphics.SetClip(&region, Gdiplus::CombineModeReplace);
			graphics.DrawString(text.c_str(), (INT)text.length(), &font, layout, &format, &selectionFontBrush);
			graphics.ResetClip();
		} else {
			// 无选区时直接绘制文本
			graphics.DrawString(
					buffer, -1, &font,
					Gdiplus::RectF(static_cast<Gdiplus::REAL>(-xOffset), 0,
								   static_cast<Gdiplus::REAL>(rc.right + abs(xOffset)),
								   static_cast<Gdiplus::REAL>(rc.bottom)),
					&format, &fontBrush);
		}
	} else {
		// 文本为空时显示提示文本，但只在控件没有焦点时显示
		if (!EDIT_HINT_TEXT.empty()) {
			Gdiplus::Color hintColor;
			if (g_skinJson != nullptr) {
				hintColor = HexToGdiplusColor(g_skinJson.value("editbox_hint_color", "#888888"));
			} else {
				hintColor = Gdiplus::Color(0xFF, 0x88, 0x88, 0x88);
			}
			Gdiplus::SolidBrush hintBrush(hintColor);

			// 设置hint文本布局，左对齐，垂直居中
			Gdiplus::StringFormat hintFormat;
			hintFormat.SetAlignment(Gdiplus::StringAlignmentNear);

			Gdiplus::RectF hintRect(
					static_cast<Gdiplus::REAL>(2 /*不要减 xOffset*/),
					0,
					static_cast<Gdiplus::REAL>(rc.right - 4),
					static_cast<Gdiplus::REAL>(rc.bottom)
			);

			graphics.DrawString(EDIT_HINT_TEXT.c_str(), -1, &font, hintRect, &hintFormat, &hintBrush);
		}

	}

}
