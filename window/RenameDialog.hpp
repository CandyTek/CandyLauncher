#pragma once


#include <windef.h>
#include <string>
#include <Shlwapi.h>
#include "../util/DpiUtil.hpp"
#include "../util/FontUtil.hpp"

struct RenameDlgData {
	std::wstring *pResult = nullptr;
	HWND hEdit = nullptr;
	int outcome = IDCANCEL; // 默认取消
	// 对话框自行适配 DPI（简单方案，见 util/DpiUtil.hpp）：按窗口 DPI 创建的字体和当前 DPI
	HFONT font = nullptr;
	UINT dpi = Dpi::kBaseDpi;
};

static HFONT CreateRenameDialogFont(const UINT dpi) {
	return Dpi::CreateScaledFont(-15, FW_NORMAL, ResolveFontFamily(L"Microsoft YaHei UI").c_str(), dpi);
}

static LRESULT CALLBACK RenameDialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			auto *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
			auto *data = reinterpret_cast<RenameDlgData *>(cs->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) data);
			// 布局常量是 96 DPI 下的值，按窗口 DPI 换算
			data->dpi = Dpi::WindowDpi(hwnd);
			data->font = CreateRenameDialogFont(data->dpi);
			const auto S = [data](const int v) { return Dpi::Scale(v, data->dpi); };

			CreateWindowExW(0, L"STATIC", L"新名称:",
							WS_CHILD | WS_VISIBLE, S(10), S(10), S(80), S(20), hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

			data->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
										  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
										  S(10), S(35), S(270), S(25), hwnd, (HMENU) 100, GetModuleHandle(nullptr), nullptr);

			CreateWindowExW(0, L"BUTTON", L"确定",
							WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
							S(150), S(70), S(60), S(25), hwnd, (HMENU) IDOK, GetModuleHandle(nullptr), nullptr);

			CreateWindowExW(0, L"BUTTON", L"取消",
							WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
							S(220), S(70), S(60), S(25), hwnd, (HMENU) IDCANCEL, GetModuleHandle(nullptr), nullptr);

			Dpi::SetFontForChildren(hwnd, data->font);
			SetFocus(data->hEdit);
			return 0;
		}
		case WM_DPICHANGED: {
			// 拖到另一台显示器：移到建议矩形，子控件等比缩放并换字体
			auto *data = reinterpret_cast<RenameDlgData *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			const UINT newDpi = Dpi::DpiFromWParam(wParam);
			Dpi::ApplySuggestedRect(hwnd, lParam);
			if (data && newDpi != data->dpi) {
				Dpi::RescaleChildWindows(hwnd, data->dpi, newDpi);
				if (data->font) DeleteObject(data->font);
				data->font = CreateRenameDialogFont(newDpi);
				Dpi::SetFontForChildren(hwnd, data->font);
				data->dpi = newDpi;
			}
			return 0;
		}
		case WM_DESTROY: {
			auto *data = reinterpret_cast<RenameDlgData *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (data && data->font) {
				DeleteObject(data->font);
				data->font = nullptr;
			}
			break;
		}
		case WM_COMMAND: {
			WORD id = LOWORD(wParam);
			if (id == IDOK || id == IDCANCEL) {
				auto *data = reinterpret_cast<RenameDlgData *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
				if (id == IDOK && data && data->pResult && data->hEdit) {
					wchar_t buffer[256]{};
					GetWindowTextW(data->hEdit, buffer, 256);
					if (wcslen(buffer) == 0) {
						MessageBeep(MB_ICONWARNING);
						return 0; // 空名字不关窗
					}
					*(data->pResult) = buffer;
					data->outcome = IDOK;
				} else if (data) {
					data->outcome = IDCANCEL;
				}
				DestroyWindow(hwnd); // 普通窗口用 DestroyWindow 结束
				return 0;
			}
			break;
		}
		case WM_CLOSE:
			SendMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
			return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void RegisterRenameDialogClass() {
	static bool registered = false;
	if (registered) return;

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = RenameDialogWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = L"RenameDialogClass";
	RegisterClassExW(&wc);
	registered = true;
}

static bool ShowRenameDialog(HWND parent, const std::wstring &originalName, std::wstring &newName,
							 const wchar_t *caption = L"重命名文件") {
	// 以 Per-Monitor V2 创建并自行适配 DPI（简单方案，见 util/DpiUtil.hpp）：按父窗口所在显示器的 DPI 换算尺寸
	Dpi::ThreadContextScope perMonitor(Dpi::PerMonitorV2Context());
	RegisterRenameDialogClass();
	newName.clear();

	const UINT dpi = Dpi::MonitorDpiForWindow(parent);
	const int width = Dpi::Scale(300, dpi);
	const int height = Dpi::Scale(150, dpi);
	RECT pr{};
	if (!parent || !GetWindowRect(parent, &pr)) {
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &pr, 0);
	}
	int x = pr.left + (pr.right - pr.left - width) / 2;
	int y = pr.top + (pr.bottom - pr.top - height) / 2;

	RenameDlgData data{};
	data.pResult = &newName;

	HWND hDlg = CreateWindowExW(
			WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
			L"RenameDialogClass", caption,
			WS_POPUP | WS_CAPTION | WS_SYSMENU,
			x, y, width, height, parent, nullptr, GetModuleHandle(nullptr), &data);

	if (!hDlg) return false;

	// 设置原名 & 选中
	HWND hEdit = GetDlgItem(hDlg, 100);
	if (hEdit) {
		SetWindowTextW(hEdit, originalName.c_str());
		SendMessage(hEdit, EM_SETSEL, 0, -1);
	}

	ShowWindow(hDlg, SW_SHOW);
	if (parent) EnableWindow(parent, FALSE);

	// Enter/ESC 加速键（非对话框不会自动触发）
	ACCEL acc[2] = {
			{FVIRTKEY, VK_RETURN, IDOK},
			{FVIRTKEY, VK_ESCAPE, IDCANCEL}
	};
	HACCEL hAcc = CreateAcceleratorTable(acc, 2);

	MSG msg{};
	while (IsWindow(hDlg) && GetMessage(&msg, nullptr, 0, 0)) {
		if (!TranslateAccelerator(hDlg, hAcc, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if (hAcc) DestroyAcceleratorTable(hAcc);

	if (parent) {
		EnableWindow(parent, TRUE);
		SetForegroundWindow(parent);
	}

	return (data.outcome == IDOK && !newName.empty());
}
