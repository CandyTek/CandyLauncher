#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <commctrl.h> // For ListView controls
#include <shlobj.h>
#include <shellapi.h>
#include <Everything.h>
#include "FileInfo.hpp"
#include "MainTools.hpp"
#include "ToolTipHelper.hpp"


// 快捷方式修复窗口相关定义
static HWND g_shortcutFixHwnd = nullptr;
static HWND g_invalidListView = nullptr;
static HWND g_candidateListView = nullptr;
static std::vector<FileInfo> g_invalidShortcuts;
static std::vector<std::wstring> g_candidatePaths;

static std::vector<FileInfo> g_candidates;


// 搜索可能的目标文件
static void SearchPossibleTargets(const std::wstring &fileName) {
	// 构建搜索查询：搜索以fileName开头的.exe文件
	std::wstring searchQuery = fileName;

	Everything_SetSearchW(searchQuery.c_str());
	Everything_QueryW(TRUE);

	DWORD numResults = Everything_GetNumResults();

	for (DWORD i = 0; i < numResults && i < 50; i++) { // 限制结果数量避免太多
		wchar_t fullPath[MAX_PATH];
		Everything_GetResultFullPathNameW(i, fullPath, MAX_PATH);

		// 检查文件是否存在
		if (GetFileAttributesW(fullPath) != INVALID_FILE_ATTRIBUTES) {
			FileInfo candidate;
			candidate.file_path = fullPath;
			candidate.label = PathFindFileNameW(fullPath);
			g_candidates.push_back(candidate);
		}
	}
}


// 修复快捷方式
static bool FixShortcut(const std::wstring &shortcutPath, const std::wstring &newTargetPath) {
	ComInitGuard guard;
	if (FAILED(guard.hr)) return false;

	IShellLink *pShellLink = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
								  IID_IShellLink, (void **) &pShellLink);
	if (FAILED(hr)) return false;

	IPersistFile *pPersistFile = nullptr;
	hr = pShellLink->QueryInterface(IID_IPersistFile, (void **) &pPersistFile);
	if (FAILED(hr)) {
		pShellLink->Release();
		return false;
	}

	// 加载现有的快捷方式
	hr = pPersistFile->Load(shortcutPath.c_str(), STGM_READWRITE);
	if (FAILED(hr)) {
		pPersistFile->Release();
		pShellLink->Release();
		return false;
	}

	// 设置新的目标路径
	hr = pShellLink->SetPath(newTargetPath.c_str());
	if (FAILED(hr)) {
		pPersistFile->Release();
		pShellLink->Release();
		return false;
	}

	// 设置工作目录为目标文件的父目录
	wchar_t workingDir[MAX_PATH];
	wcscpy_s(workingDir, newTargetPath.c_str());
	PathRemoveFileSpecW(workingDir);
	pShellLink->SetWorkingDirectory(workingDir);

	// 保存修改
	hr = pPersistFile->Save(nullptr, TRUE);
	bool success = SUCCEEDED(hr);

	pPersistFile->Release();
	pShellLink->Release();

	return success;
}


static void RefreshInvalidListView() {
	SendMessage(g_invalidListView, WM_SETREDRAW, FALSE, 0); // 暂停重绘
	ListView_DeleteAllItems(g_invalidListView);
	// 填充左侧列表
	for (size_t i = 0; i < g_invalidShortcuts.size(); ++i) {
		LVITEMW item = {};
		item.mask = LVIF_TEXT;
		item.iItem = static_cast<int>(i);
		item.iSubItem = 0;

		// 提取文件名
		std::wstring fullPath = g_invalidShortcuts[i].file_path;
		std::wstring fileName = g_invalidShortcuts[i].label;
		item.pszText = const_cast<LPWSTR>(fileName.c_str());
		ListView_InsertItem(g_invalidListView, &item);

		// 添加第二列的完整路径
		ListView_SetItemText(g_invalidListView, static_cast<int>(i), 1, const_cast<LPWSTR>(fullPath.c_str()));
	}
	ListView_SetColumnWidth(g_invalidListView, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(g_invalidListView, 1, LVSCW_AUTOSIZE);
	SendMessage(g_invalidListView, WM_SETREDRAW, TRUE, 0);  // 恢复重绘
	InvalidateRect(g_invalidListView, nullptr, TRUE);       // 立即刷新
}

// 快捷方式修复窗口过程
static LRESULT CALLBACK ShortcutFixWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			// 创建左侧无效快捷方式列表
			g_invalidListView = CreateWindowExW(0, WC_LISTVIEW, L"",
												WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
												10, 10, 300, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
			ListView_SetExtendedListViewStyle(g_invalidListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			// 设置左侧列表的列
			LVCOLUMNW col = {};
			col.mask = LVCF_TEXT | LVCF_WIDTH;
			col.pszText = const_cast<LPWSTR>(L"无效快捷方式");
			col.cx = 150;
			ListView_InsertColumn(g_invalidListView, 0, &col);
			
			// 添加第二列显示完整路径
			col.pszText = const_cast<LPWSTR>(L"完整路径");
			col.cx = 130;
			ListView_InsertColumn(g_invalidListView, 1, &col);

			// 创建右侧候选路径列表
			g_candidateListView = CreateWindowExW(0, WC_LISTVIEW, L"",
												  WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
												  320, 10, 350, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
			ListView_SetExtendedListViewStyle(g_candidateListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			// 设置右侧列表的列
			col.pszText = const_cast<LPWSTR>(L"可能的修复路径");
			col.cx = 330;
			ListView_InsertColumn(g_candidateListView, 0, &col);
			RefreshInvalidListView();
			break;
		}

		case WM_NOTIFY: {
			LPNMHDR pnmh = (LPNMHDR) lParam;

			// 左侧列表右键点击
			if (pnmh->hwndFrom == g_invalidListView && pnmh->code == NM_RCLICK) {
				int selectedItem = ListView_GetNextItem(g_invalidListView, -1, LVNI_SELECTED);
				if (selectedItem >= 0) {
					// 获取鼠标位置
					POINT pt;
					GetCursorPos(&pt);
					
					// 获取选中的快捷方式路径
					std::wstring selectedShortcut = g_invalidShortcuts[selectedItem].file_path;
					
					// 显示上下文菜单
					ShowShellContextMenu(hwnd, selectedShortcut, pt);
				}
				break;
			}

			// 右侧列表右键点击
			if (pnmh->hwndFrom == g_candidateListView && pnmh->code == NM_RCLICK) {
				int selectedItem = ListView_GetNextItem(g_candidateListView, -1, LVNI_SELECTED);
				if (selectedItem >= 0) {
					// 获取鼠标位置
					POINT pt;
					GetCursorPos(&pt);
					
					// 获取选中的候选文件路径
					std::wstring selectedCandidate = g_candidates[selectedItem].file_path;
					
					// 显示上下文菜单
					ShowShellContextMenu(hwnd, selectedCandidate, pt);
				}
				break;
			}

			// 左侧列表选择变化
			if (pnmh->hwndFrom == g_invalidListView && pnmh->code == LVN_ITEMCHANGED) {
				LPNMLISTVIEW pnmv = (LPNMLISTVIEW) lParam;
				if (pnmv->uNewState & LVIS_SELECTED) {
					g_candidates.clear();
					ListView_DeleteAllItems(g_candidateListView);
					// 获取选中的无效快捷方式路径
					std::wstring shortcutTargetPath = GetShortcutTarget(g_invalidShortcuts[pnmv->iItem].file_path);
					if(shortcutTargetPath.empty()){
						break;
					}
					SendMessage(g_candidateListView, WM_SETREDRAW, FALSE, 0); // 暂停重绘
					// 提取目标文件名（去掉扩展名）
					std::wstring fileName = shortcutTargetPath.substr(shortcutTargetPath.find_last_of(L"\\") + 1);
					// 找到最后一个点
					size_t dotPos = fileName.find_last_of(L'.');
					if (dotPos != std::wstring::npos) {
						fileName = fileName.substr(0, dotPos);
					}

					// 使用Everything SDK搜索可能的文件
					g_candidates.clear();
					ListView_DeleteAllItems(g_candidateListView);

					// 使用Everything SDK搜索可能的目标文件
					SearchPossibleTargets(fileName);

					// 填充右侧列表
					for (size_t i = 0; i < g_candidates.size(); ++i) {
						LVITEMW item = {};
						item.mask = LVIF_TEXT;
						item.iItem = static_cast<int>(i);
						item.iSubItem = 0;
						item.pszText = const_cast<LPWSTR>(g_candidates[i].file_path.c_str());
						ListView_InsertItem(g_candidateListView, &item);
					}
					ListView_SetColumnWidth(g_candidateListView, 0, LVSCW_AUTOSIZE);
					SendMessage(g_candidateListView, WM_SETREDRAW, TRUE, 0);  // 恢复重绘
					InvalidateRect(g_candidateListView, nullptr, TRUE);       // 立即刷新
				}
			}

			// 右侧列表双击
			if (pnmh->hwndFrom == g_candidateListView && pnmh->code == NM_DBLCLK) {
				if(true){
					int selectedCandidate = ListView_GetNextItem(g_candidateListView, -1, LVNI_SELECTED);
					std::wstring targetPath = g_candidates[selectedCandidate].file_path;
					CopyTextToClipboard(g_shortcutFixHwnd,targetPath);
					
					RECT rc;
					if (ListView_GetItemRect(g_candidateListView, selectedCandidate, &rc, LVIR_BOUNDS)) {
						// rc 里就是该项在 ListView 客户区的矩形
						POINT pt;
						pt.x = (rc.left + rc.right) / 2;
						pt.y = (rc.top + rc.bottom) / 2;
						ClientToScreen(g_candidateListView, &pt);
						ShowWarnTooltip(g_shortcutFixHwnd, L"已复制！",pt.x, pt.y,1000);
					}else{
						ShowWarnTooltipAtCursor(g_shortcutFixHwnd, L"已复制！",1000);
					}
					break;
				}
				int selectedCandidate = ListView_GetNextItem(g_candidateListView, -1, LVNI_SELECTED);
				int selectedInvalid = ListView_GetNextItem(g_invalidListView, -1, LVNI_SELECTED);

				if (selectedCandidate >= 0 && selectedInvalid >= 0) {
					std::wstring message =
							L"是否使用路径 \"" + g_candidates[selectedCandidate].file_path + L"\" 修复快捷方式？";
					int result = MessageBoxW(hwnd, message.c_str(), L"确认修复", MB_YESNO | MB_ICONQUESTION);

					if (result == IDYES) {
						// 修复快捷方式
						std::wstring shortcutPath = g_invalidShortcuts[selectedInvalid].file_path;
						std::wstring targetPath = g_candidates[selectedCandidate].file_path;
						
						if (FixShortcut(shortcutPath, targetPath)) {
							// 从左侧列表中删除已修复的项
							ListView_DeleteItem(g_invalidListView, selectedInvalid);
							g_invalidShortcuts.erase(g_invalidShortcuts.begin() + selectedInvalid);

							// 清空右侧列表
							ListView_DeleteAllItems(g_candidateListView);
							g_candidates.clear();

							MessageBoxW(hwnd, L"快捷方式修复成功！", L"修复结果", MB_OK | MB_ICONINFORMATION);
						} else {
							MessageBoxW(hwnd, L"修复失败！", L"修复结果", MB_OK | MB_ICONERROR);
						}
					}
				}
			}

			break;
		}

		case WM_SIZE: {
			RECT rect;
			GetClientRect(hwnd, &rect);
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;

			// 调整控件大小
			SetWindowPos(g_invalidListView, nullptr, 10, 10, width / 2 - 15, height - 20, SWP_NOZORDER);
			SetWindowPos(g_candidateListView, nullptr, width / 2 + 5, 10, width / 2 - 15, height - 20, SWP_NOZORDER);
			break;
		}

		case WM_CLOSE: {
			DestroyWindow(g_shortcutFixHwnd);
			return 0;
		}

		case WM_DESTROY: {
			g_shortcutFixHwnd = nullptr;
			g_invalidListView = nullptr;
			g_candidateListView = nullptr;
			g_invalidShortcuts.clear();
			g_candidates.clear();
			break;
		}

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// 注册快捷方式修复窗口类
static void RegisterShortcutFixClass() {
	static bool registered = false;
	if (registered) return;

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = ShortcutFixWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = L"ShortcutFixWndClass";

	RegisterClassExW(&wc);
	registered = true;
}

// 显示快捷方式修复窗口
static void ShowShortcutFixWindow(HWND hwnd, std::vector<FileInfo>& invalidShortcuts) {
	if (g_shortcutFixHwnd != nullptr) {
		ShowWindow(g_shortcutFixHwnd, SW_SHOW);
		SetForegroundWindow(g_shortcutFixHwnd);
		return;
	}

	g_invalidShortcuts = invalidShortcuts;

	RegisterShortcutFixClass();

	// 获取屏幕尺寸并计算居中位置
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int windowWidth = 1150;
	int windowHeight = 720;
	int x = (screenWidth - windowWidth) / 2;
	int y = (screenHeight - windowHeight) / 2;

	g_shortcutFixHwnd = CreateWindowExW(
			0,
			L"ShortcutFixWndClass",
			L"快捷方式修复工具",
			WS_OVERLAPPEDWINDOW,
			x, y, windowWidth, windowHeight,
			hwnd,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr
	);

	if (g_shortcutFixHwnd) {
		ShowWindow(g_shortcutFixHwnd, SW_SHOW);
		UpdateWindow(g_shortcutFixHwnd);
	}
}

// 检测快捷方式有效性
static void CheckShortcutValidity(HWND hwnd, std::vector<FileInfo> &g_fileItems) {
	std::vector<FileInfo> invalidShortcuts;

	// 遍历当前文件集合，检测.lnk文件
	for (const auto &fileInfo: g_fileItems) {
		std::wstring filePath = fileInfo.file_path;

		// 检查是否是快捷方式文件
		if (filePath.size() > 4 &&
			_wcsicmp(filePath.substr(filePath.size() - 4).c_str(), L".lnk") == 0) {
			// 调用MainTools.hpp中的IsShortcutInvalid方法
			if (IsShortcutInvalid(filePath)) {
				FileInfo fileInfo1;
				fileInfo1.file_path = filePath;
				
				std::wstring fileName = fileInfo.file_path.substr(fileInfo.file_path.find_last_of(L"\\") + 1);
				if (fileName.size() > 4 && fileName.substr(fileName.size() - 4) == L".lnk") {
					fileName = fileName.substr(0, fileName.size() - 4);
				}

				fileInfo1.label = fileName;
				invalidShortcuts.push_back(fileInfo1);
			}
		}
	}

	if (invalidShortcuts.empty()) {
		MessageBoxW(hwnd, L"所有快捷方式均有效！", L"检测结果", MB_OK | MB_ICONINFORMATION);
	} else {
		ShowShortcutFixWindow(hwnd, invalidShortcuts);
	}
}
