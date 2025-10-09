#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <shlwapi.h>

#include "../common/Constants.hpp"
#include "EditControlPainter.hpp"
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")

class EditManager {
public:
	// 初始化 - 设置子类过程
	static BOOL Attach(HWND hwndEdit, DWORD_PTR dwRefData) {
		//return SetWindowSubclass(hwndEdit, EditProc, 1, 0,(DWORD_PTR) dwRefData);
		return SetWindowSubclass(hwndEdit, EditProc, 1, dwRefData);
	}

	// 取消子类过程（可选）
	static void Detach(HWND hwndEdit) {
		RemoveWindowSubclass(hwndEdit, EditProc, 1);
	}

	static void EnableSmartEdit(HWND hEdit) {
		HRESULT hr = SHAutoComplete(hEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
		if (FAILED(hr)) {
			wchar_t buf[128];
			swprintf_s(buf, L"SHAutoComplete failed: 0x%08X", hr);
			Loge(L"EnableSmartEdit", buf);
		}
	}

private:
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
			realIndex < static_cast<int>(filteredActions.size())) {
			const std::shared_ptr<BaseAction> action =
				filteredActions[realIndex];
			if (action) {
				if (pref_close_after_open_item) HideWindow();
				// action->Invoke();
			}
		}
	}


	// 子类过程函数
	static LRESULT CALLBACK EditProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
									[[maybe_unused]] UINT_PTR uIdSubclass, const DWORD_PTR dwRefData) {
		switch (msg) {
		case WM_CREATE:
			{
				g_caretInterval = ::GetCaretBlinkTime();
				if (g_caretInterval == 0 || g_caretInterval == INFINITE) g_caretInterval = 530; // 兜底
				::SetTimer(hwnd, g_caretTimerId, g_caretInterval, nullptr);
				g_caretOn = true;
				break;
			}
		case WM_COMMAND:
			{
				break;
			}
		case WM_NCPAINT:
			{
				return 0;
			}
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				PaintEdit(hwnd, hdc);
				EndPaint(hwnd, &ps);
				HideCaret(hwnd);
				return 0; // 拦截默认绘制
			}

		case WM_PRINTCLIENT:
			// 父窗口要求直接在提供的 HDC 上画（比如组合绘制/双缓冲）
			PaintEdit(hwnd, (HDC)wParam);
			return 0;

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
					switch (vk) {
					case VK_RETURN:
						SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, HOTKEY_ID_RUN_ITEM,
									reinterpret_cast<LPARAM>(hwnd));
						return 0;
					case VK_ESCAPE:
						{
							std::string mode = g_settings_map["pref_esc_key_feature"].stringValue;
							if (mode == "close") {
								HideWindow();
							} else if (mode == "clear") {
								SetWindowText(hwnd, L"");
								SendMessage(hwnd, EM_SETSEL, 0, -1); // 选中全部文本
								SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L""); // 替换为空
							} else if (mode == "clear_and_close") {
								int length = GetWindowTextLength(hwnd);
								if (length > 0) {
									SetWindowText(hwnd, L"");
									SendMessage(hwnd, EM_SETSEL, 0, -1); // 选中全部文本
									SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L""); // 替换为空
								} else {
									HideWindow(); // 否则关闭
								}
							}
						}
						return 0;
					case VK_UP:
					case VK_DOWN:
						{
							// 上下键导航 ListView（循环）
							const int count = ListView_GetItemCount(g_listViewHwnd);
							if (count == 0) return 0;

							int selected = ListView_GetNextItem(g_listViewHwnd, -1, LVNI_SELECTED);

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
							ListView_SetItemState(g_listViewHwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
							// 设置新选择
							ListView_SetItemState(g_listViewHwnd, selected, LVIS_SELECTED | LVIS_FOCUSED,
												LVIS_SELECTED | LVIS_FOCUSED);
							ListView_EnsureVisible(g_listViewHwnd, selected, FALSE);

							return 0;
						}
					case VK_PRIOR:
					case VK_NEXT:
						{
							// 转发翻页键给listview
							SendMessage(g_listViewHwnd, msg, wParam, lParam);
							return 0;
						}

					default: break;
					}

				// 生成一个定义的快捷键值
				const UINT64 key = MAKE_HOTKEY_KEY(currentModifiers, vk);

				// 在哈希表中命中所需的快捷键
				if (const auto it = g_hotkeyMap.find(key); it != g_hotkeyMap.end()) {
					SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, static_cast<WPARAM>(it->second), 0);
					return 0;
				}

				if (currentModifiers == MOD_CTRL_KEY) {
					// ctrl+bs，让主界面刷新列表
					if (key == 8589934600) {
						SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(1, EN_CHANGE),
									reinterpret_cast<LPARAM>(hwnd));
					} else {
						// Ctrl + 数字键 (1-9) 快速启动列表项
						if ((pref_ctrl_number_launch_item)) {
							if (vk >= '1' && vk <= '9') {
								doNumberAction(vk);
								return 0;
							}
						}
					}
				}
				if (!filteredActions.empty()) {
					int pluginResult = g_pluginManager->DispatchSendHotKey(filteredActions[0], vk, currentModifiers, wParam);
					if (pluginResult == 0) {
						break;
					} else {
						return pluginResult;
					}
				}
			}
			g_caretOn = true;
			break;
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
			{
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
				if (!filteredActions.empty()) {
					return g_pluginManager->DispatchSendHotKey(filteredActions[0], vk, currentModifiers, wParam);
				}
				g_caretOn = true;
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}
		case WM_SYSKEYUP:
			{
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}
		case WM_TIMER:
			{
				if (wParam == g_caretTimerId) {
					g_caretOn = !g_caretOn;
					::InvalidateRect(hwnd, nullptr, FALSE);
					return 0;
				}
				break;
			}
		case WM_SETFOCUS:
			{
				// 获得焦点时确保定时器存在并打开
				if (g_caretInterval == 0 || g_caretInterval == INFINITE) g_caretInterval = ::GetCaretBlinkTime();
				::SetTimer(hwnd, g_caretTimerId, g_caretInterval, nullptr);
				g_caretOn = true;
				::InvalidateRect(hwnd, nullptr, FALSE);
				break;
			}
		case WM_KILLFOCUS:
			{
				// 失焦时不再显示 caret
				::KillTimer(hwnd, g_caretTimerId);
				g_caretOn = false;
				// 焦点变化时重绘，以更新hint文本显示
				InvalidateRect(hwnd, nullptr, FALSE);
				break;
			}
		case WM_NOTIFY_HEDIT_REFRESH_SKIN: return 1;
		case WM_ERASEBKGND: return 1; // 阻止默认背景擦除
		case WM_NCDESTROY: RemoveWindowSubclass(hwnd, EditProc, uIdSubclass);
			break;
		case WM_DESTROY:
			{
				::KillTimer(hwnd, g_caretTimerId);
				break;
			}
		default: break;
		}
		// ctrl+o 会发出beep声，解决不了，很多原生程序的编辑框也会这样子
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}
};
