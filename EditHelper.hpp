#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>

#include "Constants.hpp"
#include "MainTools.hpp"
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")

class EditHelper
{
public:
	// 初始化 - 设置子类过程
	static BOOL Attach(HWND hwndEdit, DWORD_PTR dwRefData)
	{
		EnableSmartEdit(hwndEdit);
		//return SetWindowSubclass(hwndEdit, EditProc, 1, 0,(DWORD_PTR) dwRefData);
		return SetWindowSubclass(hwndEdit, EditProc, 1, dwRefData);
	}

	// 取消子类过程（可选）
	static void Detach(HWND hwndEdit)
	{
		RemoveWindowSubclass(hwndEdit, EditProc, 1);
	}

private:
	static void drawBackground(HWND hwnd, Gdiplus::Graphics& graphics, RECT rc)
	{
		if (g_skinJson != nullptr)
		{
			if (g_editBgImage)
			{
				Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
				graphics.DrawImage(g_editBgImage, rect);
			}
			else
			{
				// 如果没有背景图，则填充纯色背景
				std::string bgColorStr = g_skinJson.value("editbox_bg_color", "#FFFFFF");
				if (!bgColorStr.empty())
				{
					Gdiplus::SolidBrush bgBrush(HexToGdiplusColor(bgColorStr));
					Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
					graphics.FillRectangle(&bgBrush, rect);
				}
			}
		}
		else
		{
			Gdiplus::SolidBrush bgBrush(HexToGdiplusColor("#EEEEEE"));
			Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
			graphics.FillRectangle(&bgBrush, rect);
		}
	}

	// static Gdiplus::Image* g_backgroundImage = nullptr;

	// 子类过程函数
	static LRESULT CALLBACK EditProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
									[[maybe_unused]] UINT_PTR uIdSubclass, const DWORD_PTR dwRefData)
	{
		// HWND hwndParent = (HWND)dwRefData;
		switch (msg)
		{
		case WM_NCPAINT:
			{
				return 0;
			}
		case WM_PAINT:
			{
				// 开始完全自定义绘制
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				// HDC hdc = GetDC(hwnd);
				RECT rc;
				GetClientRect(hwnd, &rc);
				// 创建GDI+绘图对象
				Gdiplus::Graphics graphics(hdc);
				graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
				graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeHighQualityBicubic);
				graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
				Gdiplus::Color fontColor;
				if (g_skinJson != nullptr)
				{
					fontColor = HexToGdiplusColor(g_skinJson.value("editbox_font_color", "#000000"));
				}
				else
				{
					fontColor = Gdiplus::Color(0xFF, 0x22, 0x22, 0x22);
				}
				Gdiplus::SolidBrush fontBrush(fontColor); // 文本颜色

				// 准备绘制文本
				TCHAR buffer[1024];
				GetWindowText(hwnd, buffer, sizeof(buffer) / sizeof(buffer[0]));

				// 获取字体信息
				HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
				Gdiplus::Font font(hdc, hFont);

				// 计算绘制位置（考虑滚动位置和光标）
				DWORD firstChar = (DWORD)SendMessage(hwnd, EM_CHARFROMPOS, 0, 0);
				int xOffset = LOWORD(SendMessage(hwnd, EM_POSFROMCHAR, firstChar, 0));

				drawBackground(hwnd, graphics, rc);

				// graphics.FillRectangle(&brush, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);


				// 设置文本布局
				Gdiplus::StringFormat format;
				format.SetTrimming(Gdiplus::StringTrimmingNone);
				format.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);

				// 绘制文本
				graphics.DrawString(
					buffer, -1, &font,
					Gdiplus::RectF(-xOffset, 0, rc.right + abs(xOffset), rc.bottom),
					&format, &fontBrush);

				EndPaint(hwnd, &ps);
				return 0; // 完全处理，阻止默认绘制
			}
		case WM_KEYDOWN:
			{
				// 1. 获取当前按下的虚拟键码
				UINT vk = static_cast<UINT>(wParam);
				// 2. 获取当前的修饰符状态
				UINT currentModifiers = 0;
				if (GetKeyState(VK_MENU) & 0x8000) currentModifiers |= MOD_ALT;
				if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= MOD_CONTROL;
				if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= MOD_SHIFT;
				if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) currentModifiers |= MOD_WIN_KEY;

				// 未组合修饰键
				if (currentModifiers == 0)
					switch (vk)
					{
					case VK_RETURN:
						SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, HOTKEY_ID_RUN_ITEM,
									reinterpret_cast<LPARAM>(hwnd));
						return 0;
					case VK_ESCAPE:
						HideWindow(GetParent(hwnd), hwnd, reinterpret_cast<HWND>(dwRefData));
						return 0;
					case VK_UP:
					case VK_DOWN:
						{
							// 上下键导航 ListView（循环）
							HWND hListView = reinterpret_cast<HWND>(dwRefData);
							const int count = ListView_GetItemCount(hListView);
							if (count == 0) return 0;

							int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);

							// 没有选中项时，从0开始
							if (selected == -1)
							{
								selected = 0;
							}

							if (vk == VK_DOWN)
							{
								selected = (selected + 1) % count; // 到底回到顶部
							}
							else
							{
								selected = (selected - 1 + count) % count; // 到顶回到底
							}

							// 清除当前选择
							ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
							// 设置新选择
							ListView_SetItemState(hListView, selected, LVIS_SELECTED | LVIS_FOCUSED,
												LVIS_SELECTED | LVIS_FOCUSED);
							ListView_EnsureVisible(hListView, selected, FALSE);

							return 0;
						}
					default: break;
					}

				// 生成一个定义的快捷键值
				const UINT64 key = MAKE_HOTKEY_KEY(currentModifiers, vk);

				// 在哈希表中命中所需的快捷键
				if (const auto it = g_hotkeyMap.find(key); it != g_hotkeyMap.end())
				{
					SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, it->second, 0);
					return 0;
				}
				// ctrl+bs，让主界面刷新列表
				if (key == 8589934600)
				{
					SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(1, EN_CHANGE), reinterpret_cast<LPARAM>(hwnd));
				}
			}
			break;
		case WM_SYSKEYUP:
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
			{
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}

		case WM_NOTIFY_HEDIT_REFRESH_SKIN:
			return 1;
		case WM_ERASEBKGND:
			return 1; // 阻止默认背景擦除
		case WM_NCDESTROY:
			RemoveWindowSubclass(hwnd, EditProc, uIdSubclass);
			break;

		default: break;
		}
		// ctrl+o 会发出beep声，解决不了，很多原生程序的编辑框也会这样子
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	static void EnableSmartEdit(HWND hEdit)
	{
		SHAutoComplete(hEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
	}
};
