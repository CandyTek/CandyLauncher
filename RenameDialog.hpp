#pragma once


#include <windef.h>
#include <string>
#include <Shlwapi.h>

struct RenameDlgData {
	std::wstring *pResult = nullptr;
	HWND hEdit = nullptr;
	int outcome = IDCANCEL; // 默认取消
};

static LRESULT CALLBACK RenameDialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			auto *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
			auto *data = reinterpret_cast<RenameDlgData *>(cs->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) data);

			CreateWindowExW(0, L"STATIC", L"新名称:",
							WS_CHILD | WS_VISIBLE, 10, 10, 80, 20, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

			data->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
										  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
										  10, 35, 270, 25, hwnd, (HMENU) 100, GetModuleHandle(nullptr), nullptr);

			CreateWindowExW(0, L"BUTTON", L"确定",
							WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
							150, 70, 60, 25, hwnd, (HMENU) IDOK, GetModuleHandle(nullptr), nullptr);

			CreateWindowExW(0, L"BUTTON", L"取消",
							WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
							220, 70, 60, 25, hwnd, (HMENU) IDCANCEL, GetModuleHandle(nullptr), nullptr);

			SetFocus(data->hEdit);
			return 0;
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

static bool ShowRenameDialog(HWND parent, const std::wstring &originalName, std::wstring &newName) {
	RegisterRenameDialogClass();
	newName.clear();

	RECT pr{};
	GetWindowRect(parent, &pr);
	int x = pr.left + (pr.right - pr.left - 300) / 2;
	int y = pr.top + (pr.bottom - pr.top - 150) / 2;

	RenameDlgData data{};
	data.pResult = &newName;

	HWND hDlg = CreateWindowExW(
			WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
			L"RenameDialogClass", L"重命名文件",
			WS_POPUP | WS_CAPTION | WS_SYSMENU,
			x, y, 300, 150, parent, nullptr, GetModuleHandle(nullptr), &data);

	if (!hDlg) return false;

// 设置原名 & 选中
	HWND hEdit = GetDlgItem(hDlg, 100);
	if (hEdit) {
		SetWindowTextW(hEdit, originalName.c_str());
		SendMessage(hEdit, EM_SETSEL, 0, -1);
	}

	ShowWindow(hDlg, SW_SHOW);
	EnableWindow(parent, FALSE);

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

	EnableWindow(parent, TRUE);
	SetForegroundWindow(parent);

	return (data.outcome == IDOK && !newName.empty());
}
