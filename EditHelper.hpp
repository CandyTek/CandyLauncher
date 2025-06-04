#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <Shlwapi.h>
#include "MainTools.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")
#define WM_EDIT_DONE (WM_USER + 100)
#define WM_EDIT_OPEN_PATH (WM_USER + 101)
#define WM_EDIT_OPEN_TARGET_PATH (WM_USER + 102)

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
		if (msg == WM_KEYDOWN)
		{
			//if (GetKeyState(VK_CONTROL) & 0x8000) {
			//if (wParam == 'A') {
			//    return 0;
			//}
			//}
			if (wParam == VK_RETURN)  // 按下回车键
			{
				SendMessage(GetParent(hwnd), WM_EDIT_DONE, 0, reinterpret_cast<LPARAM>(hwnd));
				return 0;
			}
			else if (wParam == VK_ESCAPE)
			{
				HideWindow(GetParent(hwnd), hwnd, reinterpret_cast<HWND>(dwRefData));
				return 0;
			}
			else if (wParam == VK_UP || wParam == VK_DOWN)
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

				if (wParam == VK_DOWN)
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
				ListView_SetItemState(hListView, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				ListView_EnsureVisible(hListView, selected, FALSE);

				return 0;
			}
			if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
			{
				if (wParam == VK_BACK)
				{
					SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(1, EN_CHANGE), reinterpret_cast<LPARAM>(hwnd));
				}else if (wParam == 'O')
				{
					PostMessage(GetParent(hwnd), WM_EDIT_DONE, 1, 0);
					return 0; 
				}else if (wParam == 'I')
				{
					PostMessage(GetParent(hwnd), WM_EDIT_DONE, 2, 0);
					return 0; 
				}
			}

		}else if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_SYSCHAR)
		{
			return 0; // 屏蔽 ALT 键的干扰和 beep
		}
		// ctrl+o 会发出beep声，解决不了，很多原生程序的编辑框也会这样子
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}
	
	static void EnableSmartEdit(HWND hEdit)
	{
		SHAutoComplete(hEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
	}
};
