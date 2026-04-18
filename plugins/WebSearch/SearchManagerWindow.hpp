#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "WebSearchPluginData.hpp"
#include "WebSearchUtil.hpp"
#include "util/StringUtil.hpp"
#include "view/HotkeyEditView.hpp"

#define WSM_IDC_LISTVIEW  3001
#define WSM_IDC_KEY_EDIT  3002
#define WSM_IDC_NAME_EDIT 3003
#define WSM_IDC_URL_EDIT  3004
#define WSM_IDC_HK_EDIT   3005
#define WSM_IDC_BTN_ADD   3006
#define WSM_IDC_BTN_DEL   3007
#define WSM_IDC_BTN_SAVE  3008
#define WSM_IDC_BTN_CLEAR 3009

static HWND wsm_hwnd = nullptr;
static HWND wsm_lv = nullptr;
static HWND wsm_keyEdit = nullptr;
static HWND wsm_nameEdit = nullptr;
static HWND wsm_urlEdit = nullptr;
static HWND wsm_hkEdit = nullptr;
static std::vector<SearchEngineInfo> wsm_engines;

// Strip the "(mod)(vk)" suffix added by HotkeyEditView for display in the listview
static std::wstring WSM_HotkeyDisplay(const std::wstring& s) {
	size_t p2End = s.rfind(L')');
	if (p2End == std::wstring::npos) return s;
	size_t p2Start = s.rfind(L'(', p2End);
	if (p2Start == std::wstring::npos || p2Start == 0) return s;
	size_t p1End = s.rfind(L')', p2Start - 1);
	if (p1End == std::wstring::npos) return s;
	size_t p1Start = s.rfind(L'(', p1End);
	if (p1Start == std::wstring::npos) return s;
	return s.substr(0, p1Start);
}

static void WSM_RefreshLV() {
	ListView_DeleteAllItems(wsm_lv);
	for (int i = 0; i < (int)wsm_engines.size(); ++i) {
		auto& e = wsm_engines[i];
		LVITEMW item = {};
		item.mask = LVIF_TEXT;
		item.iItem = i;
		std::wstring key = e.key;
		item.pszText = const_cast<LPWSTR>(key.c_str());
		ListView_InsertItem(wsm_lv, &item);
		std::wstring name = e.name, url = e.url;
		std::wstring hkDisplay = WSM_HotkeyDisplay(e.hotkey);
		ListView_SetItemText(wsm_lv, i, 1, const_cast<LPWSTR>(name.c_str()));
		ListView_SetItemText(wsm_lv, i, 2, const_cast<LPWSTR>(url.c_str()));
		ListView_SetItemText(wsm_lv, i, 3, const_cast<LPWSTR>(hkDisplay.c_str()));
	}
}

static std::wstring WSM_GetText(HWND h) {
	int len = GetWindowTextLengthW(h);
	if (!len) return {};
	std::wstring s(static_cast<size_t>(len) + 1, 0);
	GetWindowTextW(h, s.data(), len + 1);
	s.resize(static_cast<size_t>(len));
	return s;
}

static void WSM_ClearForm() {
	SetWindowTextW(wsm_keyEdit,  L"");
	SetWindowTextW(wsm_nameEdit, L"");
	SetWindowTextW(wsm_urlEdit,  L"");
	SetWindowTextW(wsm_hkEdit,   L"");
	ListView_SetItemState(wsm_lv, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

static LRESULT CALLBACK WSM_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE: {
		const int LV_W = 528, WIN_H = 480, LV_H = WIN_H - 46;
		const int FX = LV_W + 12, FW = 295;

		wsm_lv = CreateWindowExW(0, WC_LISTVIEWW, L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
			5, 5, LV_W, LV_H, hwnd, (HMENU)WSM_IDC_LISTVIEW, GetModuleHandle(nullptr), nullptr);
		ListView_SetExtendedListViewStyle(wsm_lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

		LVCOLUMNW col = {};
		col.mask = LVCF_TEXT | LVCF_WIDTH;
		auto addCol = [&](const wchar_t* t, int w, int i) {
			col.pszText = const_cast<LPWSTR>(t); col.cx = w;
			ListView_InsertColumn(wsm_lv, i, &col);
		};
		addCol(L"Key", 65, 0);
		addCol(L"Name", 108, 1);
		addCol(L"URL", 290, 2);
		addCol(L"Hotkey", 65, 3);

		int y = 10;
		const int LH = 18, EH = 24;

		auto mkLabel = [&](const wchar_t* t) {
			CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE,
				FX, y, FW, LH, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
			y += LH;
		};
		auto mkEdit = [&](HWND& out, int id) {
			out = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
				WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
				FX, y, FW, EH, hwnd, (HMENU)(UINT_PTR)id, GetModuleHandle(nullptr), nullptr);
			y += EH + 6;
		};

		mkLabel(L"Key (e.g. gg):");
		mkEdit(wsm_keyEdit, WSM_IDC_KEY_EDIT);
		mkLabel(L"Name:");
		mkEdit(wsm_nameEdit, WSM_IDC_NAME_EDIT);
		mkLabel(L"URL  (%s = search query):");
		mkEdit(wsm_urlEdit, WSM_IDC_URL_EDIT);
		mkLabel(L"Hotkey  (click then press key combo):");
		mkEdit(wsm_hkEdit, WSM_IDC_HK_EDIT);

		// Install hotkey capture subclass on the hotkey edit
		InstallHotkeySubclass(wsm_hkEdit, hwnd, L"websearch_engine_hotkey");

		y += 10;
		CreateWindowExW(0, L"BUTTON", L"Add / Update",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			FX, y, 138, 26, hwnd, (HMENU)WSM_IDC_BTN_ADD, GetModuleHandle(nullptr), nullptr);
		CreateWindowExW(0, L"BUTTON", L"Clear",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			FX + 144, y, 80, 26, hwnd, (HMENU)WSM_IDC_BTN_CLEAR, GetModuleHandle(nullptr), nullptr);
		y += 34;
		CreateWindowExW(0, L"BUTTON", L"Delete Selected",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			FX, y, 138, 26, hwnd, (HMENU)WSM_IDC_BTN_DEL, GetModuleHandle(nullptr), nullptr);

		CreateWindowExW(0, L"BUTTON", L"Save Config",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			5, WIN_H - 38, LV_W, 30, hwnd, (HMENU)WSM_IDC_BTN_SAVE, GetModuleHandle(nullptr), nullptr);

		wsm_engines = g_searchEngines;
		WSM_RefreshLV();
		return 0;
	}
	case WM_APP_HOTKEY_COMMIT:
		// HotkeyEditView already updated the edit control text; nothing else to do here.
		return 0;
	case WM_COMMAND: {
		int id = LOWORD(wParam);
		if (id == WSM_IDC_BTN_CLEAR) {
			WSM_ClearForm();
		} else if (id == WSM_IDC_BTN_ADD) {
			SearchEngineInfo info;
			info.key    = MyTrim(WSM_GetText(wsm_keyEdit));
			info.name   = MyTrim(WSM_GetText(wsm_nameEdit));
			info.url    = MyTrim(WSM_GetText(wsm_urlEdit));
			info.hotkey = MyTrim(WSM_GetText(wsm_hkEdit));
			if (info.key.empty() || info.name.empty() || info.url.empty()) {
				MessageBoxW(hwnd, L"Key, Name and URL are required.", L"Validation", MB_ICONWARNING);
				break;
			}
			int sel = ListView_GetNextItem(wsm_lv, -1, LVNI_SELECTED);
			if (sel >= 0 && sel < (int)wsm_engines.size()) {
				wsm_engines[sel] = info;
			} else {
				wsm_engines.push_back(info);
			}
			WSM_RefreshLV();
			WSM_ClearForm();
		} else if (id == WSM_IDC_BTN_DEL) {
			int sel = ListView_GetNextItem(wsm_lv, -1, LVNI_SELECTED);
			if (sel >= 0 && sel < (int)wsm_engines.size()) {
				wsm_engines.erase(wsm_engines.begin() + sel);
				WSM_RefreshLV();
				WSM_ClearForm();
			}
		} else if (id == WSM_IDC_BTN_SAVE) {
			g_searchEngines = wsm_engines;
			SaveWebSearchConfig();
			MessageBoxW(hwnd, L"Saved. Reindex to apply changes.", L"Saved", MB_ICONINFORMATION);
		}
		break;
	}
	case WM_NOTIFY: {
		auto* pnmh = reinterpret_cast<LPNMHDR>(lParam);
		if (pnmh->idFrom == WSM_IDC_LISTVIEW && pnmh->code == NM_CLICK) {
			auto* nia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
			int idx = nia->iItem;
			if (idx >= 0 && idx < (int)wsm_engines.size()) {
				SetWindowTextW(wsm_keyEdit,  wsm_engines[idx].key.c_str());
				SetWindowTextW(wsm_nameEdit, wsm_engines[idx].name.c_str());
				SetWindowTextW(wsm_urlEdit,  wsm_engines[idx].url.c_str());
				// Show full hotkey string (with mod/vk codes) so user can see/re-press
				SetWindowTextW(wsm_hkEdit,   wsm_engines[idx].hotkey.c_str());
			}
		}
		break;
	}
	case WM_DESTROY:
		wsm_hwnd = nullptr;
		break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowSearchManagerWindow(HWND parent) {
	if (wsm_hwnd) {
		ShowWindow(wsm_hwnd, SW_SHOW);
		SetForegroundWindow(wsm_hwnd);
		return;
	}

	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WSM_WndProc;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = L"WebSearchManagerClass";
		RegisterClassExW(&wc);
		registered = true;
	}

	const int WW = 878, WH = 528;
	int x = (GetSystemMetrics(SM_CXSCREEN) - WW) / 2;
	int y = (GetSystemMetrics(SM_CYSCREEN) - WH) / 2;

	wsm_hwnd = CreateWindowExW(0, L"WebSearchManagerClass", L"Search Manager",
		WS_OVERLAPPEDWINDOW,
		x, y, WW, WH,
		parent, nullptr, GetModuleHandle(nullptr), nullptr);

	if (wsm_hwnd) {
		ShowWindow(wsm_hwnd, SW_SHOW);
		UpdateWindow(wsm_hwnd);
	}
}

