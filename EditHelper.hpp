#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>

#include "Constants.hpp"
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")

class EditHelper {
public:
	// 初始化 - 设置子类过程
	static BOOL Attach(HWND hwndEdit, DWORD_PTR dwRefData) {
		EnableSmartEdit(hwndEdit);
		//return SetWindowSubclass(hwndEdit, EditProc, 1, 0,(DWORD_PTR) dwRefData);
		return SetWindowSubclass(hwndEdit, EditProc, 1, dwRefData);
	}

	// 取消子类过程（可选）
	static void Detach(HWND hwndEdit) {
		RemoveWindowSubclass(hwndEdit, EditProc, 1);
	}

private:
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

	// static Gdiplus::Image* g_backgroundImage = nullptr;

	static void doNumberAction(UINT vk) {
		int relativeIndex = vk - '1'; // 第几个数字键（0-8）

		// 当前可见的第一个 item
		int topIndex = ListView_GetTopIndex(g_listViewHwnd);
		// 当前一页能显示多少项
		int perPage = ListView_GetCountPerPage(g_listViewHwnd);

		// 实际对应的全局索引 = 顶部索引 + 相对索引
		int realIndex = topIndex + relativeIndex;

		if (relativeIndex < perPage &&
			realIndex < static_cast<int>(ListViewManager::filteredActions.size())) {
			const std::shared_ptr<RunCommandAction> action =
					ListViewManager::filteredActions[realIndex];
			if (action) {
				if (pref_close_after_open_item)
					HideWindow(g_mainHwnd, g_editHwnd, g_listViewHwnd);
				action->Invoke();
			}
		}

	}

// 把你的绘制主体抽出来，供 WM_PAINT / WM_PRINTCLIENT 调用
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
				// 有选区时分段绘制
				std::wstring text(buffer);

				// 绘制选区前的文本
				if (selStart > 0) {
					std::wstring beforeSelection = text.substr(0, selStart);
					graphics.DrawString(
							beforeSelection.c_str(), (int) beforeSelection.length(), &font,
							Gdiplus::RectF(static_cast<Gdiplus::REAL>(-xOffset), 0,
										   static_cast<Gdiplus::REAL>(rc.right + abs(xOffset)),
										   static_cast<Gdiplus::REAL>(rc.bottom)),
							&format, &fontBrush);
				}

				// 计算选区文本的位置和大小
				std::wstring beforeSelection = (selStart > 0) ? text.substr(0, selStart) : L"";
				std::wstring selectedText = text.substr(selStart, selEnd - selStart);

				Gdiplus::RectF beforeRect;
				graphics.MeasureString(beforeSelection.c_str(), (int) beforeSelection.length(), &font,
									   Gdiplus::RectF(0, 0, 1000, 100), &format, &beforeRect);

				Gdiplus::RectF selectedRect;
				graphics.MeasureString(selectedText.c_str(), (int) selectedText.length(), &font,
									   Gdiplus::RectF(0, 0, 1000, 100), &format, &selectedRect);

				// 绘制选区背景
				Gdiplus::Color selectionBgColor;
				if (g_skinJson != nullptr) {
					selectionBgColor = HexToGdiplusColor(g_skinJson.value("editbox_selection_bg_color", "#3399FF"));
				} else {
					selectionBgColor = Gdiplus::Color(0xFF, 0x33, 0x99, 0xFF);
				}
				Gdiplus::SolidBrush selectionBgBrush(selectionBgColor);

				Gdiplus::REAL selectionX = beforeRect.Width - static_cast<Gdiplus::REAL>(xOffset);
				graphics.FillRectangle(&selectionBgBrush, selectionX, 0.0f, selectedRect.Width,
									   static_cast<Gdiplus::REAL>(rc.bottom));

				// 绘制选区文本
				Gdiplus::Color selectionFontColor;
				if (g_skinJson != nullptr) {
					selectionFontColor = HexToGdiplusColor(g_skinJson.value("editbox_selection_font_color", "#FFFFFF"));
				} else {
					selectionFontColor = Gdiplus::Color(0xFF, 0xFF, 0xFF, 0xFF);
				}
				Gdiplus::SolidBrush selectionFontBrush(selectionFontColor);

				graphics.DrawString(
						selectedText.c_str(), (int) selectedText.length(), &font,
						Gdiplus::RectF(selectionX, 0, selectedRect.Width, static_cast<Gdiplus::REAL>(rc.bottom)),
						&format, &selectionFontBrush);

				// 绘制选区后的文本
				if (selEnd < (DWORD) textLength) {
					std::wstring afterSelection = text.substr(selEnd);
					Gdiplus::REAL afterX = beforeRect.Width + selectedRect.Width - static_cast<Gdiplus::REAL>(xOffset);
					graphics.DrawString(
							afterSelection.c_str(), (int) afterSelection.length(), &font,
							Gdiplus::RectF(afterX, 0,
										   static_cast<Gdiplus::REAL>(rc.right + abs(xOffset)) - afterX,
										   static_cast<Gdiplus::REAL>(rc.bottom)),
							&format, &fontBrush);
				}
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

	// 子类过程函数
	static LRESULT CALLBACK EditProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
									 [[maybe_unused]] UINT_PTR uIdSubclass, const DWORD_PTR dwRefData) {
		// HWND hwndParent = (HWND)dwRefData;
		switch (msg) {
			case WM_NCPAINT: {
				return 0;
			}
			case WM_PAINT: {
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				PaintEdit(hwnd, hdc);
				EndPaint(hwnd, &ps);
				return 0; // 拦截默认绘制
			}

			case WM_PRINTCLIENT:
				// 父窗口要求直接在提供的 HDC 上画（比如组合绘制/双缓冲）
				PaintEdit(hwnd, (HDC) wParam);
				return 0;

			case WM_KEYDOWN: {
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
					switch (vk) {
						case VK_RETURN:
							SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, HOTKEY_ID_RUN_ITEM,
										reinterpret_cast<LPARAM>(hwnd));
							return 0;
						case VK_ESCAPE: {
							std::string mode = g_settings_map["pref_esc_key_feature"].stringValue;
							if (mode == "close") {
								HideWindow(GetParent(hwnd), hwnd, reinterpret_cast<HWND>(dwRefData));
							} else if (mode == "clear") {
								SetWindowText(hwnd, L"");
								SendMessage(hwnd, EM_SETSEL, 0, -1);             // 选中全部文本
								SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L""); // 替换为空
							} else if (mode == "clear_and_close") {
								int length = GetWindowTextLength(hwnd);
								if (length > 0) {
									SetWindowText(hwnd, L"");
									SendMessage(hwnd, EM_SETSEL, 0, -1);             // 选中全部文本
									SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L""); // 替换为空
								} else {
									HideWindow(GetParent(hwnd), hwnd, reinterpret_cast<HWND>(dwRefData));  // 否则关闭
								}
							}
						}
							return 0;
						case VK_UP:
						case VK_DOWN: {
							// 上下键导航 ListView（循环）
							HWND hListView = reinterpret_cast<HWND>(dwRefData);
							const int count = ListView_GetItemCount(hListView);
							if (count == 0) return 0;

							int selected = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);

							// 没有选中项时，从0开始
							if (selected == -1) {
								selected = 0;
							}

							if (vk == VK_DOWN) {
								selected = (selected + 1) % count; // 到底回到顶部
							} else {
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
						default:
							break;
					}

				// 生成一个定义的快捷键值
				const UINT64 key = MAKE_HOTKEY_KEY(currentModifiers, vk);

				// 在哈希表中命中所需的快捷键
				if (const auto it = g_hotkeyMap.find(key); it != g_hotkeyMap.end()) {
					SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, it->second, 0);
					return 0;
				}
				// ctrl+bs，让主界面刷新列表
				if (key == 8589934600) {
					SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(1, EN_CHANGE), reinterpret_cast<LPARAM>(hwnd));
				}
					// Ctrl或alt + 数字键 (1-9) 快速启动列表项
				else {
					if ((pref_ctrl_number_launch_item && currentModifiers == MOD_CTRL_KEY)) {
						if (vk >= '1' && vk <= '9') {
							doNumberAction(vk);
							return 0;
						}
					}
				}


			}
				break;
			case WM_SYSCHAR:
			case WM_SYSKEYDOWN: {
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}
			case WM_SYSKEYUP: {
				// alt 的快捷键监听只能在这里
				UINT vk = static_cast<UINT>(wParam);
				// 2. 获取当前的修饰符状态
				UINT currentModifiers = 0;
				if (GetKeyState(VK_MENU) & 0x8000) currentModifiers |= MOD_ALT;
				if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= MOD_CONTROL;
				if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= MOD_SHIFT;
				if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) currentModifiers |= MOD_WIN_KEY;

				if ((pref_alt_number_launch_item && currentModifiers == MOD_ALT_KEY)) {
					if (vk >= '1' && vk <= '9') {
						doNumberAction(vk);
					}
				}

				return 0; // 屏蔽 ALT 键的干扰和 beep
			}

			case WM_SETFOCUS:
			case WM_KILLFOCUS: {
				// 焦点变化时重绘，以更新hint文本显示
				InvalidateRect(hwnd, nullptr, FALSE);
				break;
			}
			case WM_NOTIFY_HEDIT_REFRESH_SKIN:
				return 1;
			case WM_ERASEBKGND:
				return 1; // 阻止默认背景擦除
			case WM_NCDESTROY:
				RemoveWindowSubclass(hwnd, EditProc, uIdSubclass);
				break;

			default:
				break;
		}
		// ctrl+o 会发出beep声，解决不了，很多原生程序的编辑框也会这样子
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	static void EnableSmartEdit(HWND hEdit) {
		SHAutoComplete(hEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
	}
};
