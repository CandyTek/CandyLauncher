#pragma once
#include <windows.h>
#include <commctrl.h>

struct EditBorderData {
	WNDPROC oldProc{};
};

static bool ContainsDot(HWND hEdit) {
	int len = GetWindowTextLengthW(hEdit);
	if (len <= 0) return false;

	std::wstring text(len, L'\0');
	GetWindowTextW(hEdit, text.data(), len + 1);
	return text.find(L'.') != std::wstring::npos;
}

static bool IsAllValidNumberText(const std::wstring& s, bool allowFloat) {
	bool hasDot = false;
	for (wchar_t ch : s) {
		if (ch >= L'0' && ch <= L'9') continue;

		if (allowFloat && ch == L'.' && !hasDot) {
			hasDot = true;
			continue;
		}
		return false;
	}
	return true;
}


// 自定义 2px 的编辑框有问题，只有聚焦编辑框时效果才有
LRESULT CALLBACK EditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	EditBorderData* data = reinterpret_cast<EditBorderData*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

	if (msg == WM_NCPAINT) {
		HDC hdc = GetWindowDC(hWnd);

		RECT rc;
		GetWindowRect(hWnd, &rc);
		OffsetRect(&rc, -rc.left, -rc.top);

		HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
		HGDIOBJ oldPen = SelectObject(hdc, hPen);
		HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

		Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

		SelectObject(hdc, oldPen);
		SelectObject(hdc, oldBrush);
		DeleteObject(hPen);

		ReleaseDC(hWnd, hdc);
		return 0; // 阻止默认绘制
	}

	return CallWindowProc(data->oldProc, hWnd, msg, wParam, lParam);
}


static LRESULT CALLBACK EditBorder2pxProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	EditBorderData* data = reinterpret_cast<EditBorderData*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

	switch (msg) {
	case WM_NCCREATE: return TRUE;

	case WM_CREATE: return 0;

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
	case WM_ENABLE:
	case WM_SIZE: InvalidateRect(hWnd, nullptr, TRUE);
		break;

	case WM_NCPAINT:
		// 不让系统画默认边框
		return 0;

	case WM_ERASEBKGND:
		// 交给 WM_PAINT 统一处理，减少闪烁
		return 1;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			RECT rc;
			GetClientRect(hWnd, &rc);

			// 先让原生 EDIT 画文字/背景
			LRESULT lr = CallWindowProcW(data->oldProc, hWnd, msg, (WPARAM)hdc, 0);
			bool hasFocus = (GetFocus() == hWnd);
			COLORREF borderColor = hasFocus ? RGB(0, 120, 215)   // 蓝色（Win10 风格）
											: RGB(180, 180, 180); // 默认灰色
			// 再覆盖画 2px 灰色边框
			HBRUSH hBrush = CreateSolidBrush(borderColor);

			RECT rTop = {rc.left, rc.top, rc.right, rc.top + 2};
			RECT rBottom = {rc.left, rc.bottom - 2, rc.right, rc.bottom};
			RECT rLeft = {rc.left, rc.top + 2, rc.left + 2, rc.bottom - 2};
			RECT rRight = {rc.right - 2, rc.top + 2, rc.right, rc.bottom - 2};

			FillRect(hdc, &rTop, hBrush);
			FillRect(hdc, &rBottom, hBrush);
			FillRect(hdc, &rLeft, hBrush);
			FillRect(hdc, &rRight, hBrush);

			DeleteObject(hBrush);
			EndPaint(hWnd, &ps);
			return 0;
		}

	case EM_SETRECTNP:
	case EM_SETRECT: break;

	case WM_DESTROY: break;

	case WM_NCDESTROY:
		WNDPROC oldProc = data ? data->oldProc : DefWindowProcW;
		if (data) {
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
			delete data;
		}
		SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)oldProc);
		return CallWindowProcW(oldProc, hWnd, msg, wParam, lParam);
	}

	return CallWindowProcW(data->oldProc, hWnd, msg, wParam, lParam);
}


// 这个自定义2px边框也有问题，文字被边框覆盖到了，效果不好，padding也设置困难
static LRESULT CALLBACK NumberEditBorder2pxProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	EditBorderData* data = reinterpret_cast<EditBorderData*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

	switch (msg) {
	case WM_CHAR:
		{
			wchar_t ch = static_cast<wchar_t>(wParam);

			// 允许控制字符：退格、Tab、回车、Ctrl组合等
			if (ch < 32) {
				return CallWindowProcW(data->oldProc, hWnd, msg, wParam, lParam);
			}

			// 数字直接放行
			if (ch >= L'0' && ch <= L'9') {
				return CallWindowProcW(data->oldProc, hWnd, msg, wParam, lParam);
			}

			// 其他字符全部拦截
			MessageBeep(MB_OK);
			return 0;
		}

	case WM_PASTE:
		{
			if (!OpenClipboard(hWnd)) return 0;

			HANDLE hData = GetClipboardData(CF_UNICODETEXT);
			if (!hData) {
				CloseClipboard();
				return 0;
			}

			LPCWSTR pText = static_cast<LPCWSTR>(GlobalLock(hData));
			if (!pText) {
				CloseClipboard();
				return 0;
			}

			std::wstring pasteText = pText;
			GlobalUnlock(hData);
			CloseClipboard();

			// 当前文本
			int len = GetWindowTextLengthW(hWnd);
			std::wstring oldText(len, L'\0');
			GetWindowTextW(hWnd, oldText.data(), len + 1);

			// 选区位置
			DWORD selStart = 0, selEnd = 0;
			SendMessageW(hWnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

			// 模拟粘贴后的结果
			std::wstring newText =
				oldText.substr(0, selStart) +
				pasteText +
				oldText.substr(selEnd);

			if (IsAllValidNumberText(newText, false)) {
				return CallWindowProcW(data->oldProc, hWnd, msg, wParam, lParam);
			}

			MessageBeep(MB_OK);
			return 0;
		}

	case WM_NCDESTROY:
		{
			LRESULT ret = CallWindowProcW(data->oldProc, hWnd, msg, wParam, lParam);
			delete data;
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
			return ret;
		}
	}
	return EditBorder2pxProc(hWnd, msg, wParam, lParam);
}

static HWND CreateBorderEdit2px(HWND hParent, int x, int y, int w, int h, HMENU hMenu) {
	// 关键：不要 WS_BORDER，也不要 WS_EX_CLIENTEDGE
	HWND hEdit = CreateWindowExW(
		0,
		L"EDIT",
		L"",
		WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
		x, y, w, h,
		hParent,
		hMenu,
		GetModuleHandleW(nullptr),
		nullptr
	);

	if (!hEdit) return nullptr;

	auto* data = new EditBorderData;
	data->oldProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditBorder2pxProc);
	SetWindowLongPtrW(hEdit, GWLP_USERDATA, (LONG_PTR)data);

	// 文本区域内缩，避免文字压到边框
	RECT rc = {4, 2, w - 4, h - 2};
	SendMessageW(hEdit, EM_SETRECTNP, 0, (LPARAM)&rc);
	InvalidateRect(hEdit, nullptr, TRUE);

	return hEdit;
}
