#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>

#include <gdiplus.h>
#include "common/GlobalState.hpp"

inline UINT_PTR g_caretTimerId = 1001;
inline bool g_caretOn = true; // 由定时器翻转
inline UINT g_caretInterval = 0; // GetCaretBlinkTime() 的返回值缓存
inline int g_renderXShift = -4; // 负数=向左偏移4像素；想向右就改成 +4

static Gdiplus::Color GetColor(const char* key, const char* def) {
	return g_skinJson != nullptr
				? HexToGdiplusColor(g_skinJson.value(key, def))
				: HexToGdiplusColor(def);
}

static void drawBackground(HWND hwnd, Gdiplus::Graphics& graphics, RECT rc) {
	if (g_skinJson != nullptr) {
		if (g_editBgImage) {
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

	// 如果文本末尾有空格的话，MeasureCharacterRanges 计算会有问题，使用这种办法来解决
	TCHAR buffer2[1024];
	const int textLength = GetWindowText(hwnd, buffer2, std::size(buffer2));
	TCHAR buffer[1024];
	_tcscpy_s(buffer, std::size(buffer), buffer2);
	const int len = lstrlen(buffer);  // 获取字符串长度（不含 '\0'）
	// 替换空格为 'a'
	for (int i = len - 1; i >= 0; --i)
	{
		if (buffer[i] == _T(' '))
			buffer[i] = _T('i');
		else {
			break;
		}
	}


	// 计算绘制位置（考虑滚动位置）
	// 获取第一个字符（索引0）的位置，它会告诉我们当前的滚动偏移
	const LRESULT firstCharPos = SendMessage(hwnd, EM_POSFROMCHAR, 0, 0);
	// 注意：坐标可能是负数（当文本滚动时），需要作为有符号整数处理
	const short xOffsetSigned = (short)LOWORD(firstCharPos);
	const int layoutX = xOffsetSigned + g_renderXShift;
	
	// 获取选区信息
	DWORD selStart, selEnd;
	SendMessage(hwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

	// 获取编辑框的格式化矩形（实际文本绘制区域）
	RECT formatRect;
	SendMessage(hwnd, EM_GETRECT, 0, (LPARAM)&formatRect);

	// 先测量文本的实际宽度
	Gdiplus::StringFormat tempFormat;
	tempFormat.SetTrimming(Gdiplus::StringTrimmingNone);
	// 使用与绘制时相同的标志，确保测量准确
	tempFormat.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit |
		Gdiplus::StringFormatFlagsNoClip |
		Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
	Gdiplus::RectF textBounds;
	graphics.MeasureString(buffer, textLength, &font, Gdiplus::PointF(0, 0), &tempFormat, &textBounds);

	// 使用实际文本宽度，并添加额外的边距确保完全显示
	const int layoutWidth = static_cast<int>(textBounds.Width) + 20; // 添加20像素边距

	// 设置文本布局
	Gdiplus::StringFormat format;
	format.SetTrimming(Gdiplus::StringTrimmingNone);
	// 添加 NoClip 和 NoFitBlackBox 标志来移除 GDI+ 的内部边距
	format.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit |
		Gdiplus::StringFormatFlagsNoClip |
		Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

	if (textLength > 0) {
		// 有文本时绘制正常文本和选区
		const Gdiplus::SolidBrush fontBrush(GetColor("editbox_font_color", "#222222"));

		if (selStart != selEnd) {
			// 有选区的情况：使用 xOffsetSigned 来调整文本位置，使用格式化矩形的宽度
			const Gdiplus::RectF layout((Gdiplus::REAL)layoutX, 0,
										static_cast<Gdiplus::REAL>(layoutWidth),
										static_cast<Gdiplus::REAL>(rc.bottom));

			// 1) 先把整段文字按普通颜色画一次（非选中状态）
			const std::wstring text(buffer2);
			graphics.DrawString(buffer2, -1, &font, layout, &format, &fontBrush);

			// 2) 用 CharacterRanges 测到“选中区”在同一 layout 下的真实边界
			const Gdiplus::CharacterRange range((INT)selStart, (INT)(selEnd - selStart));
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
			const Gdiplus::SolidBrush selectionBgBrush(GetColor("editbox_selection_bg_color", "#3399FF"));
			graphics.FillRectangle(&selectionBgBrush, selBox);

			// 4) 只在选区内重绘文字为“选中文字色”
			const Gdiplus::SolidBrush selectionFontBrush(GetColor("editbox_selection_font_color", "#FFFFFF"));

			graphics.SetClip(&region, Gdiplus::CombineModeReplace);
			graphics.DrawString(text.c_str(), (INT)text.length(), &font, layout, &format, &selectionFontBrush);
			graphics.ResetClip();
		} else {
			// 无选区时直接绘制文本：使用 xOffsetSigned 来调整文本位置，使用格式化矩形的宽度
			const Gdiplus::RectF layout((Gdiplus::REAL)layoutX, 0,
										static_cast<Gdiplus::REAL>(layoutWidth),
										static_cast<Gdiplus::REAL>(rc.bottom));
			graphics.DrawString(buffer2, -1, &font, layout, &format, &fontBrush);

			// 绘制光标（复用选区的测量逻辑）
			if (GetFocus() == hwnd && g_caretOn) {
				const Gdiplus::REAL caretH = font.GetHeight(&graphics);
				const Gdiplus::SolidBrush caretBrush(GetColor("editbox_caret_color", "#000000"));

				if (selStart > 0 && selStart <= (DWORD)textLength) {
					// 光标在文本中间或末尾：使用与选区相同的 MeasureCharacterRanges 逻辑
					// 测量 selStart 前一个字符的位置
					const Gdiplus::CharacterRange cr((INT)selStart - 1, 1);
					format.SetMeasurableCharacterRanges(1, &cr);
					Gdiplus::Region rgn;
					if (graphics.MeasureCharacterRanges(buffer, -1, &font, layout, &format, 1, &rgn) == Gdiplus::Ok) {
						Gdiplus::RectF box;
						rgn.GetBounds(&box, &graphics);
						// 光标绘制在前一个字符的右边界
						Gdiplus::RectF caretRect(box.X + box.Width + (selStart == (DWORD)textLength?0:caretH * 0.05f), box.Y + (caretH * 0.1f),
												2.f, caretH - (caretH * 0.1f));
						graphics.FillRectangle(&caretBrush, caretRect);
					}
				} else {
					// 光标在开头（selStart == 0）
					Gdiplus::RectF caretRect(layout.X + 2.f + (caretH * 0.05f), (caretH * 0.1f),
											2.f, caretH - (caretH * 0.1f));
					graphics.FillRectangle(&caretBrush, caretRect);
				}
			}
		}
	} else {
		// 文本为空时显示提示文本，但只在控件没有焦点时显示
		if (!EDIT_HINT_TEXT.empty()) {
			const Gdiplus::SolidBrush hintBrush(GetColor("editbox_hint_color", "#888888"));

			// 设置hint文本布局，左对齐，垂直居中
			Gdiplus::StringFormat hintFormat;
			hintFormat.SetAlignment(Gdiplus::StringAlignmentNear);

			const Gdiplus::RectF hintRect(
				static_cast<Gdiplus::REAL>(2+g_renderXShift /*不要减 xOffset*/),
				0,
				static_cast<Gdiplus::REAL>(rc.right - 4),
				static_cast<Gdiplus::REAL>(rc.bottom)
			);

			graphics.DrawString(EDIT_HINT_TEXT.c_str(), -1, &font, hintRect, &hintFormat, &hintBrush);
		}
	}

	// 文本为空时绘制光标
	if (textLength == 0 && GetFocus() == hwnd && g_caretOn) {
		const Gdiplus::REAL caretH = font.GetHeight(&graphics);
		const Gdiplus::SolidBrush caretBrush(GetColor("editbox_caret_color", "#000000"));
		Gdiplus::RectF caretRect((2.f + g_renderXShift + (caretH * 0.15f)), (caretH * 0.1f),
								2.f, caretH - (caretH * 0.1f));
		graphics.FillRectangle(&caretBrush, caretRect);
	}
}
