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
#include "json.hpp"
#include "BaseTools.hpp" // For utf8_to_wide
#include "TraverseFilesHelper.hpp"
#include "MainTools.hpp"
#include "EditScrollSyncHelper.hpp"
#include "RenameDialog.hpp"
#include "FileInfo.hpp"
#include "ShortcutFixWindow.hpp"
#include "ToolTipHelper.hpp"

// 索引管理器窗口相关定义
static HWND g_indexManagerHwnd = nullptr;
static HWND g_folderListView = nullptr;
static HWND g_fileListView = nullptr;

// 右键菜单相关定义
#define IDM_EXCLUDE_ITEM 2001
#define IDM_RENAME_ITEM 2002
#define IDM_CHECK_SHORTCUTS 2003
#define IDM_NEW_CONFIG 2004
#define IDM_DELETE_CONFIG 2005
static HMENU g_contextMenu = nullptr;
static HMENU g_folderContextMenu = nullptr;

// 配置编辑控件
static HWND g_commandEdit = nullptr;
static HWND g_folderEdit = nullptr;
static HWND g_excludeWordsEdit = nullptr;
static HWND g_excludesEdit = nullptr;
static HWND g_labelExcludesEdit = nullptr;
static HWND g_renameSourcesEdit = nullptr;
static HWND g_labelRenameSourcesEdit = nullptr;
static HWND g_labelRenameTargetsEdit = nullptr;
static HWND g_renameTargetsEdit = nullptr;
static HWND g_indexButton = nullptr;
static HWND g_saveButton = nullptr;
static int leftPanelWidth = 170;
static int centerPanelEditHeight = 25;

static int centerPanelX = leftPanelWidth + 4;
static int centerPanelLabelHeight = 20;

static int sizeChangePosY = 0;


static std::vector<TraverseOptions> g_runnerConfigs;
static std::vector<FileInfo> g_fileItems;
static int index_last_selected = -1;
static CEditScrollSync g_editSync;
static int g_rightClickedItem = -1;
static int g_rightClickedConfigItem = -1;


static void TipEditEmpty();

// 检查编辑框是否为空
static bool IsConfigEditEmpty() {
	wchar_t buffer[4096];

	// 检查命令编辑框
	GetWindowTextW(g_commandEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	if (wcslen(buffer) == 0) {
		return true;
	}

	// 检查文件夹路径编辑框
	GetWindowTextW(g_folderEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	if (wcslen(buffer) == 0) {
		return true;
	}

	return false;
}

// 更新配置显示区域
static void UpdateConfigDisplayText(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(g_runnerConfigs.size())) {
		SetWindowTextW(g_commandEdit, L"");
		SetWindowTextW(g_folderEdit, L"");
		SetWindowTextW(g_excludeWordsEdit, L"");
		SetWindowTextW(g_excludesEdit, L"");
		SetWindowTextW(g_renameSourcesEdit, L"");
		SetWindowTextW(g_renameTargetsEdit, L"");
		return;
	}

	const TraverseOptions &config = g_runnerConfigs[selectedIndex];

	SetWindowTextW(g_commandEdit, config.command.c_str());
	SetWindowTextW(g_folderEdit, config.folder.c_str());
	SetWindowTextW(g_excludeWordsEdit, VectorToString(config.excludeWords).c_str());
	SetWindowTextW(g_excludesEdit, VectorToString(config.excludeNames).c_str());
	SetWindowTextW(g_renameSourcesEdit, VectorToString(config.renameSources).c_str());
	SetWindowTextW(g_renameTargetsEdit, VectorToString(config.renameTargets).c_str());
}

// 保存当前配置更改
static void SaveCurrentConfigItem(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(g_runnerConfigs.size())) {
		return;
	}

	wchar_t buffer[4096];

	GetWindowTextW(g_commandEdit, buffer, sizeof(buffer));
	g_runnerConfigs[selectedIndex].command = buffer;

	GetWindowTextW(g_folderEdit, buffer, sizeof(buffer));
	g_runnerConfigs[selectedIndex].folder = buffer;

	GetWindowTextW(g_excludeWordsEdit, buffer, sizeof(buffer));
	g_runnerConfigs[selectedIndex].excludeWords = StringToVector(buffer);

	GetWindowTextW(g_excludesEdit, buffer, sizeof(buffer));
	g_runnerConfigs[selectedIndex].excludeNames = StringToVector(buffer);

	GetWindowTextW(g_renameSourcesEdit, buffer, sizeof(buffer));
	g_runnerConfigs[selectedIndex].renameSources = StringToVector(buffer);

	GetWindowTextW(g_renameTargetsEdit, buffer, sizeof(buffer));
	g_runnerConfigs[selectedIndex].renameTargets = StringToVector(buffer);
}


static void UpdateIndexFileList(int selectedIndex) {
	g_fileItems.clear();

	if (selectedIndex < 0 || selectedIndex >= (int) g_runnerConfigs.size()) {
		ListView_SetItemCountEx(g_fileListView, 0, LVSICF_NOINVALIDATEALL);
		return;
	}

	const TraverseOptions &config = g_runnerConfigs[selectedIndex];
	TraverseFilesForEverythingSDK(config.folder, config,
								  [&](const std::wstring &name, const std::wstring &fullPath,
									  const std::wstring &parent, const std::wstring &ext) {
									  FileInfo fileInfo;
									  fileInfo.file_path = fullPath;
									  fileInfo.label = name;
									  g_fileItems.push_back(fileInfo);
								  });

	ListView_SetItemCountEx(g_fileListView, (int) g_fileItems.size(), LVSICF_NOINVALIDATEALL);
	// 调整列宽
	ListView_SetColumnWidth(g_fileListView, 0, LVSCW_AUTOSIZE);
	ListView_SetColumnWidth(g_fileListView, 1, LVSCW_AUTOSIZE);
	InvalidateRect(g_fileListView, nullptr, TRUE);       // 立即刷新

}

// 重新索引当前选中的配置
static void ReindexCurrentConfig(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(g_runnerConfigs.size())) {
		return;
	}
	// 首先保存当前项的修改
	SaveCurrentConfigItem(selectedIndex);
	// 刷新右侧列表
	UpdateIndexFileList(selectedIndex);
}

// 排除文件项
static void ExcludeFileItem(int itemIndex) {
	if (itemIndex < 0 || itemIndex >= static_cast<int>(g_fileItems.size())) {
		return;
	}

	const FileInfo &fileInfo = g_fileItems[itemIndex];
	std::wstring fileName = fileInfo.label;

	// 移除文件扩展名
	size_t dotPos = fileName.find_last_of(L'.');
	if (dotPos != std::wstring::npos) {
		fileName = fileName.substr(0, dotPos);
	}

	// 获取当前排除列表内容
	wchar_t buffer[4096];
	GetWindowTextW(g_excludesEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	std::wstring currentExcludes = buffer;

	// 检查是否已经存在
	if (currentExcludes.find(fileName) == std::wstring::npos) {
		if (!currentExcludes.empty()) {
			currentExcludes += L"\r\n";
		}
		currentExcludes += fileName;
		SetWindowTextW(g_excludesEdit, currentExcludes.c_str());
	}

	// 从文件项集合中移除该项
	g_fileItems.erase(g_fileItems.begin() + itemIndex);

	// 刷新列表视图
	ListView_SetItemCountEx(g_fileListView, (int) g_fileItems.size(), LVSICF_NOINVALIDATEALL);
	InvalidateRect(g_fileListView, nullptr, TRUE);
}

// 重命名文件项
static void RenameFileItem(int itemIndex) {
	if (itemIndex < 0 || itemIndex >= static_cast<int>(g_fileItems.size())) {
		return;
	}

	const FileInfo &fileInfo = g_fileItems[itemIndex];
	std::wstring originalName = fileInfo.label;

	// 移除文件扩展名
	size_t dotPos = originalName.find_last_of(L'.');
	if (dotPos != std::wstring::npos) {
		originalName = originalName.substr(0, dotPos);
	}

	std::wstring newName;
	if (!ShowRenameDialog(g_indexManagerHwnd, originalName, newName)) {
		return;
	}

	if (newName.empty() || newName == originalName) {
		return;
	}

	// 获取当前重命名源列表内容
	wchar_t sourcesBuffer[4096];
	GetWindowTextW(g_renameSourcesEdit, sourcesBuffer, sizeof(sourcesBuffer) / sizeof(wchar_t));
	std::wstring currentSources = sourcesBuffer;

	// 获取当前重命名目标列表内容
	wchar_t targetsBuffer[4096];
	GetWindowTextW(g_renameTargetsEdit, targetsBuffer, sizeof(targetsBuffer) / sizeof(wchar_t));
	std::wstring currentTargets = targetsBuffer;

	// 分割源和目标列表
	std::vector<std::wstring> sourceLines;
	std::vector<std::wstring> targetLines;

	std::wstringstream ss1(currentSources);
	std::wstring line;
	while (std::getline(ss1, line)) {
		if (line.back() == L'\r') line.pop_back();
		sourceLines.push_back(line);
	}

	std::wstringstream ss2(currentTargets);
	while (std::getline(ss2, line)) {
		if (line.back() == L'\r') line.pop_back();
		targetLines.push_back(line);
	}

	// 查找是否已存在该原始名称
	bool found = false;
	for (size_t i = 0; i < sourceLines.size(); ++i) {
		if (sourceLines[i] == originalName) {
			// 更新对应的目标值
			if (i < targetLines.size()) {
				targetLines[i] = newName;
			} else {
				// 如果目标列表不够长，补齐
				targetLines.resize(i + 1);
				targetLines[i] = newName;
			}
			found = true;
			break;
		}
	}

	// 如果没有找到，添加新的条目
	if (!found) {
		sourceLines.push_back(originalName);
		targetLines.push_back(newName);
	}

	// 重新组装字符串
	std::wstring newSources, newTargets;
	for (size_t i = 0; i < sourceLines.size(); ++i) {
		if (i > 0) newSources += L"\r\n";
		newSources += sourceLines[i];
	}
	for (size_t i = 0; i < targetLines.size(); ++i) {
		if (i > 0) newTargets += L"\r\n";
		newTargets += targetLines[i];
	}

	// 更新编辑框
	SetWindowTextW(g_renameSourcesEdit, newSources.c_str());
	SetWindowTextW(g_renameTargetsEdit, newTargets.c_str());
}

// 新建配置项
static void NewConfigItem() {
	// 创建新的配置项
	TraverseOptions newConfig;
	newConfig.command = L"new";
	newConfig.folder = L"";
	newConfig.excludeWords.clear();
	newConfig.excludeNames.clear();
	newConfig.renameSources.clear();
	newConfig.renameTargets.clear();

	// 添加到配置集合
	g_runnerConfigs.push_back(newConfig);

	// 添加新项到列表视图
	int newIndex = (int) g_runnerConfigs.size() - 1;
	LVITEMW item = {};
	item.mask = LVIF_TEXT;
	item.iItem = newIndex;
	item.iSubItem = 0;
	item.pszText = const_cast<LPWSTR>(newConfig.command.c_str());
	ListView_InsertItem(g_folderListView, &item);

	// 选中新添加的项
	ListView_SetItemState(g_folderListView, newIndex, LVIS_SELECTED, LVIS_SELECTED);
	ListView_EnsureVisible(g_folderListView, newIndex, FALSE);

	// 清空右侧文件列表
	g_fileItems.clear();
	ListView_SetItemCountEx(g_fileListView, 0, LVSICF_NOINVALIDATEALL);

	// 更新配置显示
	index_last_selected = newIndex;
	UpdateConfigDisplayText(newIndex);

	// 刷新视图
	InvalidateRect(g_folderListView, nullptr, TRUE);
}

// 删除配置项
static void DeleteConfigItem(int itemIndex) {
	if (itemIndex < 0 || itemIndex >= static_cast<int>(g_runnerConfigs.size())) {
		return;
	}

	// 删除配置项
	g_runnerConfigs.erase(g_runnerConfigs.begin() + itemIndex);

	// 从列表视图中删除项
	ListView_DeleteItem(g_folderListView, itemIndex);

	// 清空列表选中项（如果还有项的话，会在后面重新选择）

	// 清空配置显示和文件列表
	UpdateConfigDisplayText(-1);
	g_fileItems.clear();
	ListView_SetItemCountEx(g_fileListView, 0, LVSICF_NOINVALIDATEALL);

	// 重置选中索引
	index_last_selected = -1;

	// 如果还有配置项，选中第一个
	if (!g_runnerConfigs.empty()) {
		int selectIndex = (itemIndex >= (int) g_runnerConfigs.size()) ? (int) g_runnerConfigs.size() - 1 : itemIndex;
		ListView_SetItemState(g_folderListView, selectIndex, LVIS_SELECTED, LVIS_SELECTED);
		ListView_EnsureVisible(g_folderListView, selectIndex, FALSE);
		index_last_selected = selectIndex;
		UpdateConfigDisplayText(selectIndex);
		UpdateIndexFileList(selectIndex);
	}

	// 刷新视图
	InvalidateRect(g_folderListView, nullptr, TRUE);
}


// 索引管理器窗口过程
static LRESULT CALLBACK IndexManagerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			// 解析配置文件
			g_runnerConfigs = ParseRunnerConfig();

			// 创建右键菜单
			g_contextMenu = CreatePopupMenu();
			AppendMenuW(g_contextMenu, MF_STRING, IDM_EXCLUDE_ITEM, L"排除该项");
			AppendMenuW(g_contextMenu, MF_STRING, IDM_RENAME_ITEM, L"重命名该项");
			AppendMenuW(g_contextMenu, MF_STRING, IDM_CHECK_SHORTCUTS, L"检测快捷方式有效性");

			// 创建文件夹列表右键菜单
			g_folderContextMenu = CreatePopupMenu();
			AppendMenuW(g_folderContextMenu, MF_STRING, IDM_NEW_CONFIG, L"新建项");
			AppendMenuW(g_folderContextMenu, MF_STRING, IDM_DELETE_CONFIG, L"删除该项");

			// 创建保存按钮（在左侧区域上方）
			g_saveButton = CreateWindowExW(0, L"BUTTON", L"保存配置",
										   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
										   1, 1, leftPanelWidth, 30, hwnd, (HMENU) 1002, GetModuleHandle(nullptr),
										   nullptr);

			// 创建左侧文件夹列表（向下移动给保存按钮留空间）
			g_folderListView = CreateWindowExW(0, WC_LISTVIEW, L"",
											   WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
											   1, 35, leftPanelWidth, 365, hwnd, nullptr, GetModuleHandle(nullptr),
											   nullptr);
			ListView_SetExtendedListViewStyle(g_folderListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			// 设置文件夹列表的列
			LVCOLUMNW col = {};
			col.mask = LVCF_TEXT | LVCF_WIDTH;
			col.pszText = const_cast<LPWSTR>(L"索引配置");
			col.cx = 180;
			ListView_InsertColumn(g_folderListView, 0, &col);

			// 添加文件夹到列表
			for (size_t i = 0; i < g_runnerConfigs.size(); ++i) {
				LVITEMW item = {};
				item.mask = LVIF_TEXT;
				item.iItem = static_cast<int>(i);
				item.iSubItem = 0;
				std::wstring command = g_runnerConfigs[i].command;
				item.pszText = const_cast<LPWSTR>(command.c_str());
				ListView_InsertItem(g_folderListView, &item);
			}

			// 创建中间配置编辑区域
			int editY = 13;
			int editWidth = 300;
			int labelWidth = 120;
			int spacing = 35;

			// Command 标签和编辑框
			CreateWindowExW(0, L"STATIC", L"名称:",
							WS_CHILD | WS_VISIBLE,
							centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
							GetModuleHandle(nullptr), nullptr);
			editY += centerPanelLabelHeight;
			g_commandEdit = CreateWindowExW(0, L"EDIT", L"",
											WS_CHILD | WS_VISIBLE | WS_BORDER,
											centerPanelX, editY, editWidth - 85, centerPanelEditHeight, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			editY += centerPanelEditHeight + 2;

			// Folder 标签和编辑框
			CreateWindowExW(0, L"STATIC", L"路径:",
							WS_CHILD | WS_VISIBLE,
							centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
							GetModuleHandle(nullptr), nullptr);
			editY += centerPanelLabelHeight;

			g_folderEdit = CreateWindowExW(0, L"EDIT", L"",
										   WS_CHILD | WS_VISIBLE | WS_BORDER,
										   centerPanelX, editY, editWidth - 85, centerPanelEditHeight, hwnd, nullptr,
										   GetModuleHandle(nullptr), nullptr);
			editY += centerPanelEditHeight + 2;

			// Exclude Words 标签和编辑框
			CreateWindowExW(0, L"STATIC", L"排除词汇:",
							WS_CHILD | WS_VISIBLE,
							centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
							GetModuleHandle(nullptr), nullptr);
			editY += centerPanelLabelHeight;
			// 以下控件使用动态宽高
			sizeChangePosY = editY;

			g_excludeWordsEdit = CreateWindowExW(0, L"EDIT", L"",
												 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
												 centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
												 GetModuleHandle(nullptr), nullptr);
			editY += 70;

			// Excludes 标签和编辑框
			g_labelExcludesEdit = CreateWindowExW(0, L"STATIC", L"排除名称:",
												  WS_CHILD | WS_VISIBLE,
												  centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
												  nullptr, GetModuleHandle(nullptr), nullptr);
			g_excludesEdit = CreateWindowExW(0, L"EDIT", L"",
											 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
											 centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
											 GetModuleHandle(nullptr), nullptr);
			editY += 70;

			// Rename Sources 标签和编辑框
			g_labelRenameSourcesEdit = CreateWindowExW(0, L"STATIC", L"原索引名称:",
													   WS_CHILD | WS_VISIBLE,
													   centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
													   nullptr, GetModuleHandle(nullptr), nullptr);
			g_renameSourcesEdit = CreateWindowExW(0, L"EDIT", L"",
												  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL |
												  ES_AUTOHSCROLL,
												  centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
												  GetModuleHandle(nullptr), nullptr);
			editY += 70;

			// Rename Targets 标签和编辑框
			g_labelRenameTargetsEdit = CreateWindowExW(0, L"STATIC", L"重命名:",
													   WS_CHILD | WS_VISIBLE,
													   centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
													   nullptr, GetModuleHandle(nullptr), nullptr);
			g_renameTargetsEdit = CreateWindowExW(0, L"EDIT", L"",
												  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL |
												  ES_AUTOHSCROLL,
												  centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
												  GetModuleHandle(nullptr), nullptr);
			editY += 80;
			g_editSync.Attach(g_renameSourcesEdit, g_renameTargetsEdit);
			// 索引按钮
			g_indexButton = CreateWindowExW(0, L"BUTTON", L"测试索引",
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											centerPanelX + 100, 0, 100, 32, hwnd, (HMENU) 1001,
											GetModuleHandle(nullptr),
											nullptr);

			// 创建右侧文件列表
			g_fileListView = CreateWindowExW(0, WC_LISTVIEW, L"",
											 WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL |
											 LVS_OWNERDATA,
											 530, 10, 200, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
			ListView_SetExtendedListViewStyle(g_fileListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			// 设置列表的列
			col.pszText = const_cast<LPWSTR>(L"文件名称");
			col.cx = 180;
			ListView_InsertColumn(g_fileListView, 0, &col);
			col.pszText = const_cast<LPWSTR>(L"文件路径");
			col.cx = 180;
			ListView_InsertColumn(g_fileListView, 1, &col);


			// 如果有配置，选择第一个
			if (!g_runnerConfigs.empty()) {
				ListView_SetItemState(g_folderListView, 0, LVIS_SELECTED, LVIS_SELECTED);
				index_last_selected = 0;
				UpdateConfigDisplayText(0);
				UpdateIndexFileList(0);
			}

			break;
		}

		case WM_COMMAND: {
			if (LOWORD(wParam) == 1001) { // 重新索引按钮
				// 检查编辑框是否为空
				if (IsConfigEditEmpty()) {
					TipEditEmpty();
					break;
				}
				int selectedIndex = ListView_GetNextItem(g_folderListView, -1, LVNI_SELECTED);
				if (selectedIndex >= 0) {
					ReindexCurrentConfig(selectedIndex);
				}
			} else if (LOWORD(wParam) == 1002) { // 保存按钮
				// 检查编辑框是否为空
				if (IsConfigEditEmpty()) {
					TipEditEmpty();
					break;
				}
				SaveCurrentConfigItem(index_last_selected);
				SaveConfigToFile(g_runnerConfigs);
				DestroyWindow(g_indexManagerHwnd);
				return 0;
			} else if (LOWORD(wParam) == IDM_EXCLUDE_ITEM) { // 排除该项
				if (g_rightClickedItem >= 0) {
					ExcludeFileItem(g_rightClickedItem);
					g_rightClickedItem = -1;
				}
			} else if (LOWORD(wParam) == IDM_RENAME_ITEM) { // 重命名该项
				if (g_rightClickedItem >= 0) {
					RenameFileItem(g_rightClickedItem);
					g_rightClickedItem = -1;
				}
			} else if (LOWORD(wParam) == IDM_CHECK_SHORTCUTS) { // 检测快捷方式有效性
				CheckShortcutValidity(g_indexManagerHwnd, g_fileItems);
			} else if (LOWORD(wParam) == IDM_NEW_CONFIG) { // 新建配置项
				NewConfigItem();
			} else if (LOWORD(wParam) == IDM_DELETE_CONFIG) { // 删除配置项
				if (g_rightClickedConfigItem >= 0) {
					DeleteConfigItem(g_rightClickedConfigItem);
					g_rightClickedConfigItem = -1;
				}
			}
			break;
		}

		case WM_NOTIFY: {
			LPNMHDR pnmh = (LPNMHDR) lParam;

			if (pnmh->hwndFrom == g_folderListView) {
				switch (pnmh->code) {
					case LVN_ITEMCHANGING: {
						LPNMLISTVIEW p = (LPNMLISTVIEW) lParam;

						// 只在状态变化且“选中位”发生变化时处理
						if ((p->uChanged & LVIF_STATE) && ((p->uOldState ^ p->uNewState) & LVIS_SELECTED)) {
							const bool goingToSelect = (p->uNewState & LVIS_SELECTED) != 0;
							const bool goingToDeselect = (p->uOldState & LVIS_SELECTED) != 0 && !goingToSelect;

							// 是否要阻止——当已有选中项且编辑框为空时，不允许切换/清空
							if (index_last_selected >= 0 && IsConfigEditEmpty()) {
								// 1) 准备切到别的项：目标不是当前项 且 goingToSelect
								if (goingToSelect && p->iItem != index_last_selected) {
									TipEditEmpty();
									return TRUE; // ⭐ 直接阻止
								}
								// 2) 准备把当前项取消选中（包括点空白导致的清空）
								if (goingToDeselect && (p->iItem == index_last_selected || p->iItem == -1)) {
									return TRUE; // ⭐ 也要拦，避免被清空/切走
								}
							}
						}
						return 0; // 其他情况放行
					}
						break;
					case LVN_ITEMCHANGED: {
						// 选中变化
						LPNMLISTVIEW pnmv = (LPNMLISTVIEW) lParam;
						if (pnmv->uNewState & LVIS_SELECTED) {
							// 当切换文件夹选择的时候保存当前配置
							if (index_last_selected >= 0 && index_last_selected != pnmv->iItem) {
								SaveCurrentConfigItem(index_last_selected);
							}
							if (pnmv->iItem != index_last_selected) {
								index_last_selected = pnmv->iItem;
								UpdateConfigDisplayText(pnmv->iItem);
								UpdateIndexFileList(pnmv->iItem);
							}

						}
					}
						break;
					case NM_RCLICK: {
						// 文件夹列表右键点击
						LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE) lParam;
						g_rightClickedConfigItem = pnmia->iItem;

						// 获取鼠标位置
						POINT pt;
						GetCursorPos(&pt);

						// 显示右键菜单
						TrackPopupMenu(g_folderContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
					}
						break;

					default:
						break;

				}

			}

			// 文件列表
			if (pnmh->hwndFrom == g_fileListView && pnmh->code == LVN_GETDISPINFO) {
				NMLVDISPINFOW *plvdi = (NMLVDISPINFOW *) lParam;
				if (plvdi->item.mask & LVIF_TEXT) {
					if (plvdi->item.iItem >= 0 && plvdi->item.iItem < (int) g_fileItems.size()) {
						const auto &item = g_fileItems[plvdi->item.iItem];

						if (plvdi->item.iSubItem == 0) {
							// 第一列：文件名称
							lstrcpynW(plvdi->item.pszText, item.label.c_str(), plvdi->item.cchTextMax);
						} else if (plvdi->item.iSubItem == 1) {
							// 第二列：文件路径
							lstrcpynW(plvdi->item.pszText, item.file_path.c_str(), plvdi->item.cchTextMax);
						}
					}
				}
			} else if (pnmh->hwndFrom == g_fileListView && pnmh->code == NM_RCLICK) {
				// 文件列表右键点击
				LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE) lParam;
				g_rightClickedItem = pnmia->iItem;

				if (g_rightClickedItem >= 0 && g_rightClickedItem < static_cast<int>(g_fileItems.size())) {
					// 获取鼠标位置
					POINT pt;
					GetCursorPos(&pt);

					// 显示右键菜单
					TrackPopupMenu(g_contextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
				}
			}


			break;
		}

		case WM_SIZE: {
			// 处理窗口大小变化
			RECT rect;
			GetClientRect(hwnd, &rect);
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;
			int tempY = sizeChangePosY;
			int threeRectHeight = ((height - sizeChangePosY - (centerPanelLabelHeight * 2) - 4) / 3);
			// 重新调整控件大小
			int centerPanelWidth = static_cast<int>((width - leftPanelWidth) * 0.4) - 5;
			SetWindowPos(g_commandEdit, nullptr, -1, -1, centerPanelWidth, centerPanelEditHeight,
						 SWP_NOZORDER | SWP_NOMOVE);
			SetWindowPos(g_folderEdit, nullptr, -1, -1, centerPanelWidth, centerPanelEditHeight,
						 SWP_NOZORDER | SWP_NOMOVE);
			SetWindowPos(g_excludeWordsEdit, nullptr, -1, -1, centerPanelWidth, threeRectHeight,
						 SWP_NOZORDER | SWP_NOMOVE);
			tempY += threeRectHeight + 2;
			SetWindowPos(g_labelExcludesEdit, nullptr, centerPanelX, tempY, -1, -1, SWP_NOZORDER | SWP_NOSIZE);
			tempY += centerPanelLabelHeight;
			SetWindowPos(g_excludesEdit, nullptr, centerPanelX, tempY, centerPanelWidth, threeRectHeight, SWP_NOZORDER);
			tempY += threeRectHeight + 2;
			SetWindowPos(g_labelRenameSourcesEdit, nullptr, centerPanelX, tempY, -1, -1, SWP_NOZORDER | SWP_NOSIZE);
			SetWindowPos(g_labelRenameTargetsEdit, nullptr, centerPanelX + centerPanelWidth / 2, tempY, -1, -1,
						 SWP_NOZORDER | SWP_NOSIZE);
			tempY += centerPanelLabelHeight;
			SetWindowPos(g_renameSourcesEdit, nullptr, centerPanelX, tempY, centerPanelWidth / 2 - 1, threeRectHeight,
						 SWP_NOZORDER);
			SetWindowPos(g_renameTargetsEdit, nullptr, centerPanelX + centerPanelWidth / 2, tempY, centerPanelWidth / 2,
						 threeRectHeight, SWP_NOZORDER);
			SetWindowPos(g_indexButton, nullptr, centerPanelX + centerPanelWidth - 100, 0, -1, -1,
						 SWP_NOZORDER | SWP_NOSIZE);


			// 调整保存按钮位置（保持在左侧上方）
			SetWindowPos(g_saveButton, nullptr, 1, 0, leftPanelWidth, 32, SWP_NOZORDER);
			// 调整文件夹列表位置（留出保存按钮的空间）
			SetWindowPos(g_folderListView, nullptr, 1, 33, leftPanelWidth, height - 34, SWP_NOZORDER);
			SetWindowPos(g_fileListView, nullptr, static_cast<int>((width - leftPanelWidth) * 0.4 + leftPanelWidth) + 1,
						 0, static_cast<int>((width - leftPanelWidth) * 0.6), height - 1,
						 SWP_NOZORDER);

			// 中间编辑区域保持固定布局，不需要调整
			break;
		}

		case WM_CLOSE: {
			int ret = MessageBoxW(hwnd, L"是否保存？", L"是否保存？", MB_OKCANCEL | MB_ICONQUESTION);
			if (ret == IDOK) {
				SendMessage(g_indexManagerHwnd, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED), 0);
			} else {
				DestroyWindow(g_indexManagerHwnd);
			}
			return 0;
		}

		case WM_DESTROY: {
			if (g_contextMenu) {
				DestroyMenu(g_contextMenu);
				g_contextMenu = nullptr;
			}
			if (g_folderContextMenu) {
				DestroyMenu(g_folderContextMenu);
				g_folderContextMenu = nullptr;
			}
			g_indexManagerHwnd = nullptr;
			g_folderListView = nullptr;
			g_fileListView = nullptr;
			g_commandEdit = nullptr;
			g_folderEdit = nullptr;
			g_excludeWordsEdit = nullptr;
			g_excludesEdit = nullptr;
			g_labelExcludesEdit = nullptr;
			g_renameSourcesEdit = nullptr;
			g_labelRenameSourcesEdit = nullptr;
			g_labelRenameTargetsEdit = nullptr;
			g_renameTargetsEdit = nullptr;
			g_indexButton = nullptr;
			g_saveButton = nullptr;
			g_fileItems.clear();
			g_runnerConfigs.clear();
			index_last_selected = -1; // 重置选中索引状态
			g_rightClickedItem = -1;
			g_rightClickedConfigItem = -1;
			break;
		}

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static void TipEditEmpty() {
	RECT rc;
	if (ListView_GetItemRect(g_folderListView, index_last_selected, &rc, LVIR_BOUNDS)) {
		// rc 里就是该项在 ListView 客户区的矩形
		POINT pt;
		pt.x = (rc.left + rc.right) / 2;
		pt.y = (rc.top + rc.bottom) / 2;
		ClientToScreen(g_folderListView, &pt);
		ShowWarnTooltip(g_indexManagerHwnd, L"请先填写名称和路径再切换配置项！",pt.x, pt.y);
	}else{
		ShowWarnTooltipAtCursor(g_indexManagerHwnd, L"请先填写名称和路径再切换配置项！");
	}
}

// 注册索引管理器窗口类
static void RegisterIndexManagerClass() {
	static bool registered = false;
	if (registered) return;

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = IndexManagerWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wc.lpszClassName = L"IndexManagerWndClass";

	RegisterClassExW(&wc);
	registered = true;
}

// 显示索引管理器窗口
static void ShowIndexedManagerWindow(HWND parent) {
	if (g_indexManagerHwnd != nullptr) {
		ShowWindow(g_indexManagerHwnd, SW_SHOW);
		SetForegroundWindow(g_indexManagerHwnd);
		return;
	}

	RegisterIndexManagerClass();

	// 获取屏幕尺寸并计算居中位置
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int windowWidth = 1150;
	int windowHeight = 720;
	int x = (screenWidth - windowWidth) / 2;
	int y = (screenHeight - windowHeight) / 2;

	g_indexManagerHwnd = CreateWindowExW(
			WS_EX_ACCEPTFILES,
			L"IndexManagerWndClass",
			L"索引管理器",
			WS_OVERLAPPEDWINDOW,
			x, y, windowWidth, windowHeight,
			parent,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr
	);

	if (g_indexManagerHwnd) {
		ShowWindow(g_indexManagerHwnd, SW_SHOW);
		UpdateWindow(g_indexManagerHwnd);
	}
}
