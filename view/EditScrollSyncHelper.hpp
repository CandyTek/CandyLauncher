#pragma once

#include <windef.h>
#include <string>
#include <Shlwapi.h>

class CEditScrollSync
{
public:
	CEditScrollSync() : m_hEdit1(nullptr), m_hEdit2(nullptr),
						m_oldProc1(nullptr), m_oldProc2(nullptr), m_syncing(false) {}

	void Attach(HWND hEdit1, HWND hEdit2)
	{
		m_hEdit1 = hEdit1;
		m_hEdit2 = hEdit2;

		// 子类化两个 Edit 控件
		m_oldProc1 = (WNDPROC)SetWindowLongPtrW(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditProcStatic);
		m_oldProc2 = (WNDPROC)SetWindowLongPtrW(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditProcStatic);

		// 保存 this 指针
		SetWindowLongPtrW(hEdit1, GWLP_USERDATA, (LONG_PTR)this);
		SetWindowLongPtrW(hEdit2, GWLP_USERDATA, (LONG_PTR)this);
	}

private:
	HWND m_hEdit1, m_hEdit2;
	WNDPROC m_oldProc1, m_oldProc2; // 原始窗口过程保存
	bool m_syncing;  // 防止递归同步

	static LRESULT CALLBACK EditProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		CEditScrollSync* pThis = (CEditScrollSync*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
		if (pThis)
			return pThis->EditProc(hwnd, msg, wParam, lParam);

		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC oldProc = (hwnd == m_hEdit1) ? m_oldProc1 : m_oldProc2;
		// 调用原始过程处理消息
		LRESULT result = CallWindowProcW(oldProc, hwnd, msg, wParam, lParam);

		// 处理各种可能导致滚动的消息
		switch (msg)
		{
			case WM_VSCROLL:
			case WM_MOUSEWHEEL:
				Synchronize(hwnd);
				break;

			case WM_KEYDOWN:
				// 处理键盘导航键
				switch (wParam)
				{
					case VK_UP: case VK_DOWN:
					case VK_PRIOR: case VK_NEXT: // PageUp/PageDown
					case VK_HOME: case VK_END:
						// 延迟同步，让原始消息先处理完
						PostMessageW(hwnd, WM_USER + 1000, 0, 0);
						break;
				}
				break;

			case WM_USER + 1000:// 自定义消息用于延迟同步
				Synchronize(hwnd);
				break;
		}

		return result;
	}

	void Synchronize(HWND currentEdit)
	{
		if (m_syncing) return; // 防止递归
		m_syncing = true;
		// 判断是哪个控件在滚动
		HWND hOther = (currentEdit == m_hEdit1) ? m_hEdit2 : m_hEdit1;

		// 获取当前编辑框的第一个可见行
		int lineThis = (int)SendMessageW(currentEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
		int lineOther = (int)SendMessageW(hOther, EM_GETFIRSTVISIBLELINE, 0, 0);

		// 如果不同步，就滚动另一边
		if (lineThis != lineOther)
		{
			SendMessageW(hOther, EM_LINESCROLL, 0, lineThis - lineOther);
		}

		m_syncing = false;
	}
};
