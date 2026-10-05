#pragma once

#include "../Plugin.hpp"
#include "AutomationActionModel.hpp"
#include "FolderPluginData.hpp"
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <algorithm>
#include <cwctype>

constexpr wchar_t kClassName[] = L"CandyAutomationActionEditor";
constexpr wchar_t kFolder[] = L"plugins\\AutomationActions";
constexpr int ID_NAME = 101, ID_STEPS = 102, ID_TARGET = 103, ID_ARGS = 104,
	ID_DELAY = 105, ID_COMMAND = 106, ID_ADD = 107, ID_REMOVE = 108, ID_SAVE = 109,
	ID_WORKDIR = 110, ID_NEW = 111, ID_OPEN = 112, ID_SAVE_AS = 113,
	ID_UP = 114, ID_DOWN = 115;

struct State {
	HWND name{}, list{}, target{}, args{}, workdir{}, delay{}, command{}, add{}, remove{}, save{};
	std::vector<AutomationStep> steps;
	std::wstring currentPath;
	HFONT font{}, titleFont{};
	HBRUSH background{}, editBackground{};
	int editing = -1;
	bool dirty = false;
	bool loading = false;
};

inline HWND g_window = nullptr;

inline std::wstring Read(HWND control) {
	int length = GetWindowTextLengthW(control);
	std::wstring value(length + 1, L'\0');
	GetWindowTextW(control, value.data(), length + 1);
	value.resize(length);
	return value;
}

inline std::wstring Trim(std::wstring value) {
	auto visible = [](wchar_t c) { return c != L' ' && c != L'\t' && c != L'\r' && c != L'\n'; };
	auto first = std::find_if(value.begin(), value.end(), visible);
	auto last = std::find_if(value.rbegin(), value.rend(), visible).base();
	return first < last ? std::wstring(first, last) : L"";
}

inline void Alert(HWND hwnd, const wchar_t* message) {
	MessageBoxW(hwnd, message, L"创建自动化动作组", MB_OK | MB_ICONWARNING);
}

inline bool ReadStep(HWND hwnd, State* state, AutomationStep& step) {
	step.path = Trim(Read(state->target));
	step.params = Trim(Read(state->args));
	step.workingDir = Trim(Read(state->workdir));
	step.command = Trim(Read(state->command));
	std::wstring delay = Trim(Read(state->delay));
	if (step.path.empty() && step.command.empty()) {
		Alert(hwnd, L"请填写目标路径或自定义执行命令。");
		return false;
	}
	if (!step.command.empty() && (step.command.find_first_of(L"\r\n") != std::wstring::npos)) {
		Alert(hwnd, L"每个步骤的自定义命令只能占一行。");
		return false;
	}
	if (!delay.empty()) {
		if (delay.find_first_not_of(L"0123456789") != std::wstring::npos || delay.size() > 6) {
			Alert(hwnd, L"延时请输入 0 到 600000 的毫秒数。");
			return false;
		}
		step.delayMs = static_cast<unsigned>(std::stoul(delay));
		if (step.delayMs > 600000) {
			Alert(hwnd, L"延时不能超过 600000 毫秒。");
			return false;
		}
	}
	return true;
}

inline void DrawSteps(State* state) {
	ListView_DeleteAllItems(state->list);
	for (size_t i = 0; i < state->steps.size(); ++i) {
		const AutomationStep& step = state->steps[i];
		std::wstring number = std::to_wstring(i + 1);
		std::wstring label = step.command.empty() ? step.path : L"命令  " + step.command;
		std::wstring delay = std::to_wstring(step.delayMs) + L" ms";
		LVITEMW item{};
		item.mask = LVIF_TEXT;
		item.iItem = static_cast<int>(i);
		item.pszText = number.data();
		ListView_InsertItem(state->list, &item);
		ListView_SetItemText(state->list, static_cast<int>(i), 1, label.data());
		ListView_SetItemText(state->list, static_cast<int>(i), 2, delay.data());
	}
}

inline bool ValidName(const std::wstring& name) {
	if (name.empty() || name == L"." || name == L".." || name.back() == L'.' || name.back() == L' ' ||
		name.find_first_of(L"\\/:*?\"<>|\r\n") != std::wstring::npos) return false;
	std::wstring stem = name.substr(0, name.find(L'.'));
	std::transform(stem.begin(), stem.end(), stem.begin(), towupper);
	if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL") return false;
	if (stem.size() == 4 && (stem.rfind(L"COM", 0) == 0 || stem.rfind(L"LPT", 0) == 0) &&
		stem[3] >= L'1' && stem[3] <= L'9') return false;
	return true;
}

inline void ClearStepEditor(State* state) {
	state->editing = -1;
	state->loading = true;
	SetWindowTextW(state->target, L""); SetWindowTextW(state->args, L"");
	SetWindowTextW(state->workdir, L""); SetWindowTextW(state->command, L"");
	SetWindowTextW(state->delay, L"0"); SetWindowTextW(state->add, L"添加步骤");
	state->loading = false;
}

inline bool ConfirmDiscard(HWND hwnd, State* state) {
	return !state->dirty || MessageBoxW(hwnd, L"当前文件有未保存的修改，是否放弃？",
		L"自动化动作组", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

inline void NewDocument(HWND hwnd, State* state) {
	if (!ConfirmDiscard(hwnd, state)) return;
	state->loading = true;
	state->currentPath.clear();
	state->steps.clear();
	SetWindowTextW(state->name, L"");
	DrawSteps(state);
	ClearStepEditor(state);
	SetWindowTextW(hwnd, L"自动化动作组编辑器 — 新建");
	state->loading = false;
	state->dirty = false;
}

inline bool LoadDocument(HWND hwnd, State* state, const std::wstring& path) {
	AutomationDocument document;
	std::wstring error;
	if (!LoadAutomationDocument(path, document, error)) {
		MessageBoxW(hwnd, error.c_str(), L"加载自动化动作组失败", MB_OK | MB_ICONERROR);
		return false;
	}
	state->loading = true;
	state->steps = std::move(document.steps);
	SetWindowTextW(state->name, document.name.c_str());
	DrawSteps(state);
	ClearStepEditor(state);
	const auto folder = std::filesystem::path(EXE_FOLDER_PATH2) / kFolder;
	state->currentPath = _wcsicmp(std::filesystem::path(path).parent_path().lexically_normal().c_str(),
		folder.lexically_normal().c_str()) == 0 ? path : L"";
	SetWindowTextW(hwnd, (L"自动化动作组编辑器 — " + std::filesystem::path(path).filename().wstring()).c_str());
	state->loading = false;
	state->dirty = false;
	return true;
}

inline bool Save(HWND hwnd, State* state) {
	std::wstring name = Trim(Read(state->name));
	if (name.size() > 5 && _wcsicmp(name.c_str() + name.size() - 5, L".json") == 0)
		name.resize(name.size() - 5);
	if (!ValidName(name)) {
		Alert(hwnd, L"请输入有效的动作组名称。名称不能包含路径字符或使用 Windows 保留名称。");
		return false;
	}
	if (state->editing >= 0 && state->editing < static_cast<int>(state->steps.size())) {
		AutomationStep updated;
		if (!ReadStep(hwnd, state, updated)) return false;
		state->steps[state->editing] = std::move(updated);
	} else if (!Trim(Read(state->target)).empty() || !Trim(Read(state->command)).empty()) {
		AutomationStep pending;
		if (!ReadStep(hwnd, state, pending)) return false;
		state->steps.push_back(std::move(pending));
	}
	if (state->steps.empty()) {
		Alert(hwnd, L"请先添加至少一个步骤。");
		return false;
	}
	std::filesystem::path folder = std::filesystem::path(EXE_FOLDER_PATH2) / kFolder;
	std::filesystem::path path = state->currentPath.empty() ? folder / (name + L".json") : std::filesystem::path(state->currentPath);
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	if (error) {
		Alert(hwnd, L"无法创建动作组文件夹。");
		return false;
	}
	if (state->currentPath.empty() && std::filesystem::exists(path, error) &&
		MessageBoxW(hwnd, L"同名动作组已经存在，是否覆盖？", L"自动化动作组", MB_YESNO | MB_ICONQUESTION) != IDYES) return false;
	AutomationDocument document{name, state->steps};
	std::wstring failure;
	if (!SaveAutomationDocument(path.wstring(), document, failure)) {
		MessageBoxW(hwnd, failure.c_str(), L"保存自动化动作组失败", MB_OK | MB_ICONERROR);
		return false;
	}
	state->currentPath = path.wstring();
	state->dirty = false;
	ClearStepEditor(state);
	DrawSteps(state);
	SetWindowTextW(hwnd, (L"自动化动作组编辑器 — " + path.filename().wstring()).c_str());
	if (g_refreshFolderPlugin) {
		auto refresh = g_refreshFolderPlugin;
		std::thread([refresh]() { refresh(); }).detach();
	}
	MessageBoxW(hwnd, (L"已保存：" + path.wstring()).c_str(), L"自动化动作组", MB_OK | MB_ICONINFORMATION);
	return true;
}

inline void SaveAs(HWND hwnd, State* state) {
	wchar_t filename[32768] = {};
	std::wstring proposed = Trim(Read(state->name));
	if (!proposed.empty() && (proposed.size() < 5 || _wcsicmp(proposed.c_str() + proposed.size() - 5, L".json") != 0))
		proposed += L".json";
	wcsncpy_s(filename, proposed.c_str(), _TRUNCATE);
	const std::wstring folder = (std::filesystem::path(EXE_FOLDER_PATH2) / kFolder).wstring();
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	if (error) { Alert(hwnd, L"无法创建动作组文件夹。"); return; }
	OPENFILENAMEW dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = hwnd;
	dialog.lpstrFilter = L"自动化动作组 (*.json)\0*.json\0";
	dialog.lpstrFile = filename;
	dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
	dialog.lpstrInitialDir = folder.c_str();
	dialog.lpstrDefExt = L"json";
	dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	if (!GetSaveFileNameW(&dialog)) return;
	const std::filesystem::path selected(filename);
	if (_wcsicmp(selected.parent_path().lexically_normal().c_str(),
		std::filesystem::path(folder).lexically_normal().c_str()) != 0 ||
		_wcsicmp(selected.extension().c_str(), L".json") != 0) {
		Alert(hwnd, L"JSON 动作组须保存在插件的 AutomationActions 文件夹内。");
		return;
	}
	const std::wstring oldPath = state->currentPath;
	const std::wstring oldName = Read(state->name);
	state->currentPath = selected.wstring();
	state->loading = true;
	SetWindowTextW(state->name, selected.stem().c_str());
	state->loading = false;
	if (!Save(hwnd, state)) {
		state->currentPath = oldPath;
		state->loading = true;
		SetWindowTextW(state->name, oldName.c_str());
		state->loading = false;
	}
}

inline HWND Label(HWND parent, const wchar_t* text, int x, int y, int w, HFONT font) {
	HWND control = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
		x, y, w, 22, parent, nullptr, nullptr, nullptr);
	SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	return control;
}

inline HWND Edit(HWND parent, int id, int x, int y, int w, HFONT font, const wchar_t* initial = L"") {
	HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initial,
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, x, y, w, 31,
		parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
	SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	DragAcceptFiles(control, TRUE);
	return control;
}

inline HWND Button(HWND parent, int id, const wchar_t* text, int x, int y, int w, HFONT font) {
	HWND control = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		x, y, w, 34, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
	SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	return control;
}

inline LRESULT CALLBACK ChildDropProc(HWND child, UINT msg, WPARAM wParam, LPARAM lParam,
	UINT_PTR, DWORD_PTR parent) {
	if (msg == WM_DROPFILES) return SendMessageW(reinterpret_cast<HWND>(parent), msg, wParam, lParam);
	return DefSubclassProc(child, msg, wParam, lParam);
}

inline void OpenDocument(HWND hwnd, State* state) {
	if (!ConfirmDiscard(hwnd, state)) return;
	wchar_t filename[32768] = {};
	const std::wstring folder = (std::filesystem::path(EXE_FOLDER_PATH2) / kFolder).wstring();
	OPENFILENAMEW dialog{};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = hwnd;
	dialog.lpstrFilter = L"自动化动作组 (*.json)\0*.json\0所有文件 (*.*)\0*.*\0";
	dialog.lpstrFile = filename;
	dialog.nMaxFile = static_cast<DWORD>(std::size(filename));
	dialog.lpstrInitialDir = folder.c_str();
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	if (GetOpenFileNameW(&dialog)) LoadDocument(hwnd, state, filename);
}

inline LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	State* state = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg) {
	case WM_CREATE: {
		state = new State();
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
		state->font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
		state->titleFont = CreateFontW(-25, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
		state->background = CreateSolidBrush(RGB(247, 249, 252));
		state->editBackground = CreateSolidBrush(RGB(255, 255, 255));
		Label(hwnd, L"自动化动作组编辑器", 28, 20, 450, state->titleFont);
		Label(hwnd, L"打开已有 JSON 或新建文件。拖入目标文件可批量添加步骤。", 29, 58, 740, state->font);
		Label(hwnd, L"动作组名称", 30, 101, 160, state->font);
		state->name = Edit(hwnd, ID_NAME, 30, 126, 430, state->font);
		Button(hwnd, ID_NEW, L"新建", 472, 124, 88, state->font);
		Button(hwnd, ID_OPEN, L"打开 JSON", 570, 124, 94, state->font);
		Button(hwnd, ID_SAVE_AS, L"另存为", 674, 124, 91, state->font);
		Label(hwnd, L"步骤列表  ·  拖入文件可批量添加", 30, 178, 500, state->font);
		state->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
			30, 205, 735, 152, hwnd, reinterpret_cast<HMENU>((INT_PTR)ID_STEPS), nullptr, nullptr);
		SendMessageW(state->list, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
		ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
		column.pszText = const_cast<LPWSTR>(L"#"); column.cx = 45; ListView_InsertColumn(state->list, 0, &column);
		column.pszText = const_cast<LPWSTR>(L"目标 / 执行命令"); column.cx = 565; ListView_InsertColumn(state->list, 1, &column);
		column.pszText = const_cast<LPWSTR>(L"延时"); column.cx = 110; ListView_InsertColumn(state->list, 2, &column);
		DragAcceptFiles(state->list, TRUE);
		Label(hwnd, L"目标路径", 30, 378, 350, state->font);
		Label(hwnd, L"目标参数", 407, 378, 350, state->font);
		state->target = Edit(hwnd, ID_TARGET, 30, 402, 355, state->font);
		state->args = Edit(hwnd, ID_ARGS, 407, 402, 358, state->font);
		Label(hwnd, L"工作目录（可选）", 30, 447, 350, state->font);
		state->workdir = Edit(hwnd, ID_WORKDIR, 30, 471, 735, state->font);
		Label(hwnd, L"自定义执行命令（填写后优先使用）", 30, 515, 470, state->font);
		Label(hwnd, L"执行前延时（毫秒）", 568, 515, 200, state->font);
		state->command = Edit(hwnd, ID_COMMAND, 30, 539, 515, state->font);
		state->delay = Edit(hwnd, ID_DELAY, 568, 539, 197, state->font, L"0");
		state->add = Button(hwnd, ID_ADD, L"添加步骤", 30, 591, 160, state->font);
		state->remove = Button(hwnd, ID_REMOVE, L"删除选中步骤", 202, 591, 155, state->font);
		Button(hwnd, ID_UP, L"上移", 372, 591, 82, state->font);
		Button(hwnd, ID_DOWN, L"下移", 463, 591, 82, state->font);
		state->save = Button(hwnd, ID_SAVE, L"保存 JSON 动作组", 565, 591, 200, state->font);
		SetWindowLongPtrW(state->save, GWL_STYLE, GetWindowLongPtrW(state->save, GWL_STYLE) | BS_OWNERDRAW);
		for (HWND control : {state->name, state->list, state->target, state->args, state->workdir, state->command, state->delay})
			SetWindowSubclass(control, ChildDropProc, 1, reinterpret_cast<DWORD_PTR>(hwnd));
		DragAcceptFiles(hwnd, TRUE);
		return 0;
	}
	case WM_CTLCOLORSTATIC:
		SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
		SetTextColor(reinterpret_cast<HDC>(wParam), RGB(39, 51, 67));
		return reinterpret_cast<LRESULT>(state->background);
	case WM_CTLCOLOREDIT:
		return reinterpret_cast<LRESULT>(state->editBackground);
	case WM_ERASEBKGND: {
		RECT rect; GetClientRect(hwnd, &rect);
		FillRect(reinterpret_cast<HDC>(wParam), &rect, state->background);
		return 1;
	}
	case WM_DRAWITEM: {
		const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
		if (draw && draw->CtlID == ID_SAVE) {
			HBRUSH accent = CreateSolidBrush((draw->itemState & ODS_SELECTED) ? RGB(30, 83, 160) : RGB(40, 104, 198));
			FillRect(draw->hDC, &draw->rcItem, accent);
			DeleteObject(accent);
			SetBkMode(draw->hDC, TRANSPARENT);
			SetTextColor(draw->hDC, RGB(255, 255, 255));
			SelectObject(draw->hDC, state->font);
			RECT rect = draw->rcItem;
			DrawTextW(draw->hDC, L"保存 JSON 动作组", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			if (draw->itemState & ODS_FOCUS) DrawFocusRect(draw->hDC, &rect);
			return TRUE;
		}
		break;
	}
	case WM_NOTIFY: {
		const auto* notification = reinterpret_cast<const NMHDR*>(lParam);
		if (notification && notification->idFrom == ID_STEPS && notification->code == LVN_ITEMCHANGING && !state->loading) {
			const auto* changing = reinterpret_cast<const NMLISTVIEW*>(lParam);
			if ((changing->uChanged & LVIF_STATE) && !(changing->uOldState & LVIS_SELECTED) &&
				(changing->uNewState & LVIS_SELECTED) && state->editing < 0 &&
				(!Trim(Read(state->target)).empty() || !Trim(Read(state->command)).empty())) {
				if (MessageBoxW(hwnd, L"当前输入的步骤尚未添加，切换后会丢弃。是否继续？",
					L"自动化动作组", MB_YESNO | MB_ICONQUESTION) != IDYES) return TRUE;
				ClearStepEditor(state);
			}
			if ((changing->uChanged & LVIF_STATE) && (changing->uOldState & LVIS_SELECTED) &&
				!(changing->uNewState & LVIS_SELECTED) && changing->iItem == state->editing &&
				state->editing >= 0 && state->editing < static_cast<int>(state->steps.size())) {
				AutomationStep edited;
				if (!ReadStep(hwnd, state, edited)) return TRUE;
				state->steps[state->editing] = std::move(edited);
				std::wstring label = state->steps[state->editing].command.empty()
					? state->steps[state->editing].path : L"命令  " + state->steps[state->editing].command;
				std::wstring delay = std::to_wstring(state->steps[state->editing].delayMs) + L" ms";
				ListView_SetItemText(state->list, state->editing, 1, label.data());
				ListView_SetItemText(state->list, state->editing, 2, delay.data());
			}
			return FALSE;
		}
		if (notification && notification->idFrom == ID_STEPS && notification->code == LVN_ITEMCHANGED) {
			const auto* changed = reinterpret_cast<const NMLISTVIEW*>(lParam);
			if ((changed->uChanged & LVIF_STATE) && (changed->uNewState & LVIS_SELECTED) &&
				changed->iItem >= 0 && changed->iItem < static_cast<int>(state->steps.size())) {
				state->editing = changed->iItem;
				const AutomationStep& step = state->steps[state->editing];
				state->loading = true;
				SetWindowTextW(state->target, step.path.c_str());
				SetWindowTextW(state->args, step.params.c_str());
				SetWindowTextW(state->workdir, step.workingDir.c_str());
				SetWindowTextW(state->command, step.command.c_str());
				SetWindowTextW(state->delay, std::to_wstring(step.delayMs).c_str());
				SetWindowTextW(state->add, L"更新选中步骤");
				state->loading = false;
			}
			return 0;
		}
		break;
	}
	case WM_DROPFILES: {
		HDROP drop = reinterpret_cast<HDROP>(wParam);
		UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
		std::vector<std::wstring> paths;
		for (UINT i = 0; i < count; ++i) {
			UINT length = DragQueryFileW(drop, i, nullptr, 0);
			std::wstring path(length + 1, L'\0');
			DragQueryFileW(drop, i, path.data(), length + 1);
			path.resize(length);
			paths.push_back(std::move(path));
		}
		DragFinish(drop);
		if (paths.size() == 1 && _wcsicmp(std::filesystem::path(paths.front()).extension().c_str(), L".json") == 0) {
			if (ConfirmDiscard(hwnd, state)) LoadDocument(hwnd, state, paths.front());
			return 0;
		}
		for (const auto& path : paths) state->steps.push_back({path, L"", L"", L"", 0});
		ClearStepEditor(state);
		DrawSteps(state);
		state->dirty = true;
		return 0;
	}
	case WM_COMMAND:
		if (HIWORD(wParam) == EN_CHANGE && !state->loading) state->dirty = true;
		if (LOWORD(wParam) == ID_NEW) { NewDocument(hwnd, state); return 0; }
		if (LOWORD(wParam) == ID_OPEN) { OpenDocument(hwnd, state); return 0; }
		if (LOWORD(wParam) == ID_SAVE_AS) { SaveAs(hwnd, state); return 0; }
		if (LOWORD(wParam) == ID_ADD) {
			AutomationStep step;
			if (ReadStep(hwnd, state, step)) {
				if (state->editing >= 0 && state->editing < static_cast<int>(state->steps.size()))
					state->steps[state->editing] = std::move(step);
				else state->steps.push_back(std::move(step));
				ClearStepEditor(state);
				DrawSteps(state);
				state->dirty = true;
				SetFocus(state->target);
			}
			return 0;
		}
		if (LOWORD(wParam) == ID_REMOVE) {
			int selected = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
			if (selected >= 0) {
				state->steps.erase(state->steps.begin() + selected);
				ClearStepEditor(state);
				DrawSteps(state);
				state->dirty = true;
			}
			return 0;
		}
		if (LOWORD(wParam) == ID_UP || LOWORD(wParam) == ID_DOWN) {
			int selected = ListView_GetNextItem(state->list, -1, LVNI_SELECTED);
			int next = selected + (LOWORD(wParam) == ID_UP ? -1 : 1);
			if (selected >= 0 && next >= 0 && next < static_cast<int>(state->steps.size())) {
				AutomationStep edited;
				if (!ReadStep(hwnd, state, edited)) return 0;
				state->steps[selected] = std::move(edited);
				std::swap(state->steps[selected], state->steps[next]);
				state->editing = -1;
				DrawSteps(state);
				ListView_SetItemState(state->list, next, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				ListView_EnsureVisible(state->list, next, FALSE);
				state->dirty = true;
			}
			return 0;
		}
		if (LOWORD(wParam) == ID_SAVE) { Save(hwnd, state); return 0; }
		break;
	case WM_CLOSE:
		if (ConfirmDiscard(hwnd, state)) DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		g_window = nullptr;
		DeleteObject(state->font); DeleteObject(state->titleFont);
		DeleteObject(state->background); DeleteObject(state->editBackground);
		delete state;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}


inline void ShowAutomationActionWindow(HWND parent) {
	if (g_window && IsWindow(g_window)) {
		ShowWindow(g_window, SW_RESTORE);
		SetForegroundWindow(g_window);
		return;
	}
	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW cls{};
		cls.cbSize = sizeof(cls);
		cls.lpfnWndProc = WindowProc;
		cls.hInstance = GetModuleHandleW(nullptr);
		cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		cls.lpszClassName = kClassName;
		if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
		registered = true;
	}
	int width = 810, height = 700;
	g_window = CreateWindowExW(WS_EX_ACCEPTFILES, kClassName, L"自动化动作组编辑器 — 新建",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		(GetSystemMetrics(SM_CXSCREEN) - width) / 2, (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
		width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
	if (g_window) { ShowWindow(g_window, SW_SHOW); UpdateWindow(g_window); }
}
