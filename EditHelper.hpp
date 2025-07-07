#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>

#include "Constants.hpp"
#include "MainTools.hpp"

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
	// 子类过程函数
	static LRESULT CALLBACK EditProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
									[[maybe_unused]] UINT_PTR uIdSubclass, const DWORD_PTR dwRefData)
	{
		switch (msg)
		{
		case WM_KEYDOWN:
			{
				// 1. 获取当前按下的虚拟键码
				UINT vk = wParam;
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
					SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(1, EN_CHANGE),reinterpret_cast<LPARAM>(hwnd));
				}
			}
			break;
		case WM_SYSKEYUP:
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
			{
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}
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
