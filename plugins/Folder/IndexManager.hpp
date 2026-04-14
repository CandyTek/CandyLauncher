#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <commctrl.h>
#include <shlobj.h>

#include "FileSystemTraverserWrapper.hpp"
#include "FolderPluginConfigUtils.hpp"
#include "util/BaseTools.hpp"
#include "util/FileSystemTraverser.hpp"
#include "view/EditScrollSynchronizer.hpp"
#include "window/RenameDialog.hpp"
#include "model/FileInfo.hpp"
#include "ShortcutRepairTool.hpp"
#include "UWPAppsTraverserWrapper.hpp"
#include "manager/TrayMenuManager.hpp"
#include "util/BitmapUtil.hpp"
#include "view/ToolTipHelper.hpp"

// 索引管理器窗口相关定义
static HWND g_indexManagerHwnd = nullptr;
static HWND g_folderListView = nullptr;
static HWND g_fileListView = nullptr;
static HWND g_fileFilterEdit = nullptr;
static HWND g_fileFilterClearButton = nullptr;
static HIMAGELIST g_fileListSysImageList = nullptr;


// 右键菜单相关定义
#define IDM_EXCLUDE_ITEM 2001
#define IDM_RENAME_ITEM 2002
#define IDM_CHECK_SHORTCUTS 2003
#define IDM_NEW_CONFIG 2004
#define IDM_DELETE_CONFIG 2005
#define IDC_FILE_FILTER_EDIT 3001
#define IDC_FILE_FILTER_CLEAR 3002
#define IDC_TOGGLE_FILE_ICON 3003
static HMENU fileItemContextMenu = nullptr;
static HMENU folderContextMenu = nullptr;

// 配置编辑控件
static HWND g_typeComboBox = nullptr;
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
static HWND g_toggleIconButton = nullptr;
static int leftPanelWidth = 170;
static int centerPanelEditHeight = 25;
// static bool g_showFileIcons = true;
static bool g_showFileIcons = false;

static int centerPanelX = leftPanelWidth + 4;
static int centerPanelLabelHeight = 20;

static int sizeChangePosY = 0;

static std::vector<TraverseOptions> runnerConfigs;
static std::vector<FileInfo> allFileItems;
static std::vector<int> filteredFileItemIndices;
// 有一个类成员变量来存储所有文本
static std::vector<std::wstring> folderItemTexts;
static int index_last_selected = -1;
static EditScrollSync editSyncHelper;
static int rightClickedItemIndex = -1;
static int rightClickedConfigItemIndex = -1;

static std::wstring ToLowerString(const std::wstring& input) {
	std::wstring result = input;
	std::transform(result.begin(), result.end(), result.begin(),
					[](wchar_t ch) {
						return static_cast<wchar_t>(towlower(ch));
					});
	return result;
}

static bool ContainsCaseInsensitive(const std::wstring& text, const std::wstring& keyword) {
	if (keyword.empty()) {
		return true;
	}
	return ToLowerString(text).find(ToLowerString(keyword)) != std::wstring::npos;
}

static void SetFileListColumnImageMode(bool enableImage) {
	if (!g_fileListView) {
		return;
	}

	LVCOLUMNW col = {};
	col.mask = LVCF_FMT;
	if (ListView_GetColumn(g_fileListView, 0, &col)) {
		if (enableImage) {
			col.fmt |= LVCFMT_IMAGE;
		} else {
			col.fmt &= ~LVCFMT_IMAGE;
		}
		ListView_SetColumn(g_fileListView, 0, &col);
	}
}

static void UpdateFilteredFileList() {
	if (!g_fileListView) {
		return;
	}

	filteredFileItemIndices.clear();

	std::wstring keyword;
	if (g_fileFilterEdit) {
		wchar_t buffer[1024] = {};
		GetWindowTextW(g_fileFilterEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
		keyword = buffer;
	}

	for (int i = 0; i < static_cast<int>(allFileItems.size()); ++i) {
		const auto& item = allFileItems[i];
		if (keyword.empty() ||
			ContainsCaseInsensitive(item.label, keyword) ||
			ContainsCaseInsensitive(item.file_path, keyword)) {
			filteredFileItemIndices.push_back(i);
		}
	}

	ListView_SetItemCountEx(g_fileListView, static_cast<int>(filteredFileItemIndices.size()), LVSICF_NOINVALIDATEALL);
	ListView_SetColumnWidth(g_fileListView, 0, LVSCW_AUTOSIZE_USEHEADER);
	ListView_SetColumnWidth(g_fileListView, 1, LVSCW_AUTOSIZE_USEHEADER);
	InvalidateRect(g_fileListView, nullptr, TRUE);

	if (g_fileFilterClearButton) {
		ShowWindow(g_fileFilterClearButton, keyword.empty() ? SW_HIDE : SW_SHOW);
	}
}

static void ClearFileFilter() {
	if (!g_fileFilterEdit) {
		return;
	}
	SetWindowTextW(g_fileFilterEdit, L"");
	UpdateFilteredFileList();
	SetFocus(g_fileFilterEdit);
}

static void UpdateFileListIconState() {
	if (!g_fileListView) {
		return;
	}

	if (g_showFileIcons) {
		ListView_SetImageList(g_fileListView, g_fileListSysImageList, LVSIL_SMALL);
		SetFileListColumnImageMode(true);
		if (g_toggleIconButton) {
			SetWindowTextW(g_toggleIconButton, L"隐藏图标");
		}
	} else {
		SetFileListColumnImageMode(false);
		ListView_SetImageList(g_fileListView, nullptr, LVSIL_SMALL);
		if (g_toggleIconButton) {
			SetWindowTextW(g_toggleIconButton, L"显示图标");
		}
	}

	RedrawWindow(g_fileListView, nullptr, nullptr,
				RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

static void TipEditEmpty() {
	RECT rc;
	ListView_GetItemRect(g_folderListView, index_last_selected, &rc, LVIR_BOUNDS);
	ShowWarnTooltipAtRect(g_folderListView, L"请先填写名称和路径再切换配置项！", rc);
}

static void SyncCurrentConfigNameToLeftList() {
	if (index_last_selected < 0 || index_last_selected >= static_cast<int>(runnerConfigs.size())) {
		return;
	}
	if (!g_commandEdit || !g_folderListView) {
		return;
	}

	wchar_t buffer[4096];
	GetWindowTextW(g_commandEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	runnerConfigs[index_last_selected].command = buffer;

	if (index_last_selected >= static_cast<int>(folderItemTexts.size())) {
		folderItemTexts.resize(runnerConfigs.size());
	}

	folderItemTexts[index_last_selected] = buffer;
	ListView_SetItemText(g_folderListView, index_last_selected, 0, folderItemTexts[index_last_selected].data());
	InvalidateRect(g_folderListView, nullptr, TRUE);
}

static void NormalizeFolderEditPathOnKillFocus() {
	if (!g_folderEdit) {
		return;
	}

	wchar_t buffer[4096];
	GetWindowTextW(g_folderEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	std::wstring path = buffer;
	if (path.empty() || path == L"default") {
		return;
	}

	std::wstring normalized;
	normalized.reserve(path.size());
	bool lastWasSlash = false;
	for (wchar_t ch : path) {
		if (ch == L'\\') {
			if (!lastWasSlash) {
				normalized.push_back(ch);
			}
			lastWasSlash = true;
		} else {
			normalized.push_back(ch);
			lastWasSlash = false;
		}
	}

	if (normalized != path) {
		SetWindowTextW(g_folderEdit, normalized.c_str());
	}
}

// 更新配置显示区域
static void UpdateConfigDisplayText(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(runnerConfigs.size())) {
		SendMessage(g_typeComboBox, CB_SETCURSEL, 0, 0);
		SetWindowTextW(g_commandEdit, L"");
		SetWindowTextW(g_folderEdit, L"");
		SetWindowTextW(g_excludeWordsEdit, L"");
		SetWindowTextW(g_excludesEdit, L"");
		SetWindowTextW(g_renameSourcesEdit, L"");
		SetWindowTextW(g_renameTargetsEdit, L"");
		// 默认启用所有控件
		EnableWindow(g_folderEdit, TRUE);
		return;
	}

	const TraverseOptions& config = runnerConfigs[selectedIndex];

	// 设置类型下拉框
	int typeIndex = 0;
	if (config.type == L"uwp") typeIndex = 1;
	else if (config.type == L"regedit") typeIndex = 2;
	else if (config.type == L"path") typeIndex = 3;
	SendMessage(g_typeComboBox, CB_SETCURSEL, typeIndex, 0);


	// 根据类型设置folder编辑框的状态和内容
	if (config.type == L"folder" || config.type.empty()) {
		// folder类型：启用编辑框，显示实际路径
		EnableWindow(g_folderEdit, TRUE);
		SetWindowTextW(g_folderEdit, config.folder.c_str());
		EnableWindow(g_commandEdit, TRUE);
		SetWindowTextW(g_commandEdit, config.command.c_str());
	} else {
		// uwp/regedit/path类型：禁用编辑框，显示"default"
		EnableWindow(g_folderEdit, FALSE);
		SetWindowTextW(g_folderEdit, L"default");
		EnableWindow(g_commandEdit, FALSE);
		SetWindowTextW(g_commandEdit, L"default");
	}

	SetWindowTextW(g_excludeWordsEdit, VectorToString(config.excludeWords).c_str());
	SetWindowTextW(g_excludesEdit, VectorToString(config.excludeNames).c_str());
	SetWindowTextW(g_renameSourcesEdit, VectorToString(config.renameSources).c_str());
	SetWindowTextW(g_renameTargetsEdit, VectorToString(config.renameTargets).c_str());
}

// 保存当前配置更改
static void SaveCurrentConfigItem(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(runnerConfigs.size())) {
		return;
	}

	wchar_t buffer[4096];

	// 保存类型
	int typeIndex = static_cast<int>(SendMessage(g_typeComboBox, CB_GETCURSEL, 0, 0));
	switch (typeIndex) {
	case 0: runnerConfigs[selectedIndex].type = L"";
		break;
	case 1: runnerConfigs[selectedIndex].type = L"uwp";
		break;
	case 2: runnerConfigs[selectedIndex].type = L"regedit";
		break;
	case 3: runnerConfigs[selectedIndex].type = L"path";
		break;
	default: runnerConfigs[selectedIndex].type = L"";
		break;
	}

	GetWindowTextW(g_commandEdit, buffer, sizeof(buffer));
	runnerConfigs[selectedIndex].command = buffer;

	// 保存folder字段，如果是非folder类型且显示"default"，则保存为空字符串
	GetWindowTextW(g_folderEdit, buffer, sizeof(buffer));
	if (typeIndex != 0 && wcscmp(buffer, L"default") == 0) {
		runnerConfigs[selectedIndex].folder = L"";
	} else {
		runnerConfigs[selectedIndex].folder = buffer;
	}

	GetWindowTextW(g_excludeWordsEdit, buffer, sizeof(buffer));
	runnerConfigs[selectedIndex].excludeWords = StringToVectorAndLower(buffer);

	GetWindowTextW(g_excludesEdit, buffer, sizeof(buffer));
	runnerConfigs[selectedIndex].excludeNames = StringToVector(buffer);

	GetWindowTextW(g_renameSourcesEdit, buffer, sizeof(buffer));
	runnerConfigs[selectedIndex].renameSources = StringToVector(buffer);

	GetWindowTextW(g_renameTargetsEdit, buffer, sizeof(buffer));
	runnerConfigs[selectedIndex].renameTargets = StringToVector(buffer);

	runnerConfigs[selectedIndex].renameMap.clear();
	const auto& sources = runnerConfigs[selectedIndex].renameSources;
	const auto& targets = runnerConfigs[selectedIndex].renameTargets;
	size_t count = std::min(sources.size(), targets.size());
	for (size_t i = 0; i < count; ++i) {
		if (!sources[i].empty() && !targets[i].empty()) {
			runnerConfigs[selectedIndex].renameMap[sources[i]] = targets[i];
		}
	}
}

static TraverseOptions BuildCurrentConfigFromControls(int selectedIndex) {
	TraverseOptions config;
	if (selectedIndex >= 0 && selectedIndex < static_cast<int>(runnerConfigs.size())) {
		config = runnerConfigs[selectedIndex];
	}

	wchar_t buffer[4096];

	int typeIndex = static_cast<int>(SendMessage(g_typeComboBox, CB_GETCURSEL, 0, 0));
	switch (typeIndex) {
	case 0: config.type = L"";
		break;
	case 1: config.type = L"uwp";
		break;
	case 2: config.type = L"regedit";
		break;
	case 3: config.type = L"path";
		break;
	default: config.type = L"";
		break;
	}

	GetWindowTextW(g_commandEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	config.command = buffer;

	GetWindowTextW(g_folderEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	if (typeIndex != 0 && wcscmp(buffer, L"default") == 0) {
		config.folder = L"";
	} else {
		config.folder = buffer;
	}

	GetWindowTextW(g_excludeWordsEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	config.excludeWords = StringToVectorAndLower(buffer);

	GetWindowTextW(g_excludesEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	config.excludeNames = StringToVector(buffer);

	GetWindowTextW(g_renameSourcesEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	config.renameSources = StringToVector(buffer);

	GetWindowTextW(g_renameTargetsEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	config.renameTargets = StringToVector(buffer);

	config.renameMap.clear();
	const auto& sources = config.renameSources;
	const auto& targets = config.renameTargets;
	size_t count = std::min(sources.size(), targets.size());
	for (size_t i = 0; i < count; ++i) {
		if (!sources[i].empty() && !targets[i].empty()) {
			config.renameMap[sources[i]] = targets[i];
		}
	}

	return config;
}


static void UpdateIndexFileListFromConfig(TraverseOptions& config) {
	allFileItems.clear();
	if (config.type == L"folder" || config.type.empty()) {
		if (g_host->GetSettingsMap().at("pref_use_everything_sdk_index").boolValue) {
			g_host->TraverseFilesForEverythingSDK(config.folder, config,
												[&](const std::wstring& name, const std::wstring& fullPath,
													const std::wstring& parent, const std::wstring& ext) {
													FileInfo fileInfo;
													fileInfo.file_path = fullPath;
													fileInfo.label = name;
													allFileItems.push_back(fileInfo);
												});
		} else {
			TraverseFiles(config.folder, config, EXE_FOLDER_PATH2,
						[&](const std::wstring& name, const std::wstring& fullPath,
							const std::wstring& parent, const std::wstring& ext) {
							FileInfo fileInfo;
							fileInfo.file_path = fullPath;
							fileInfo.label = name;
							allFileItems.push_back(fileInfo);
						});
		}
	} else if (config.type == L"uwp") {
		LoadUwpApps([&](const std::wstring& name,
						const std::wstring& fullPath,
						const std::wstring uwpCommandS,
						const HBITMAP hBitmap
				) {
						FileInfo fileInfo;
						fileInfo.file_path = fullPath;
						fileInfo.label = name;
						allFileItems.push_back(fileInfo);
					}, config);
	} else if (config.type == L"regedit") {
		TraverseRegistryApps([&](const std::wstring& name,
								const std::wstring& fullPath,
								const std::wstring& parent) {
			FileInfo fileInfo;
			fileInfo.file_path = fullPath;
			fileInfo.label = name;
			allFileItems.push_back(fileInfo);
		}, config);
	} else if (config.type == L"path") {
		TraversePATHExecutables2([&](const std::wstring& name,
									const std::wstring& fullPath,
									const std::wstring& parent,
									const std::wstring& ext) {
			FileInfo fileInfo;
			fileInfo.file_path = fullPath;
			fileInfo.label = name;
			allFileItems.push_back(fileInfo);
		}, config, EXE_FOLDER_PATH2);
	}

	UpdateFilteredFileList();
}

static void UpdateIndexFileList(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= (int)runnerConfigs.size()) {
		allFileItems.clear();
		UpdateFilteredFileList();
		return;
	}

	UpdateIndexFileListFromConfig(runnerConfigs[selectedIndex]);
}

// 重新索引当前选中的配置
static void ReindexCurrentConfig(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(runnerConfigs.size())) {
		return;
	}
	TraverseOptions currentConfig = BuildCurrentConfigFromControls(selectedIndex);
	runnerConfigs[selectedIndex] = currentConfig;
	UpdateIndexFileListFromConfig(currentConfig);
}

// 排除文件项
static void ExcludeFileItem(int itemIndex) {
	if (itemIndex < 0 || itemIndex >= static_cast<int>(allFileItems.size())) {
		return;
	}

	const FileInfo& fileInfo = allFileItems[itemIndex];
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
	allFileItems.erase(allFileItems.begin() + itemIndex);

	// 刷新过滤后的列表视图
	UpdateFilteredFileList();
}

// 重命名文件项
static void RenameFileItem(int itemIndex) {
	if (itemIndex < 0 || itemIndex >= static_cast<int>(allFileItems.size())) {
		return;
	}
	if (index_last_selected < 0 || index_last_selected >= static_cast<int>(runnerConfigs.size())) {
		return;
	}

	const FileInfo& fileInfo = allFileItems[itemIndex];
	std::wstring originalName;
	if (runnerConfigs[index_last_selected].type == L"uwp") {
		originalName = fileInfo.label;
	} else {
		originalName = fileInfo.file_path.substr(fileInfo.file_path.find_last_of(L"\\/") + 1);
		// 移除文件扩展名
		size_t dotPos = originalName.find_last_of(L'.');
		if (dotPos != std::wstring::npos) {
			originalName = originalName.substr(0, dotPos);
		}
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

	// 添加到配置集合
	runnerConfigs.push_back(newConfig);

	// 添加新项到列表视图
	int newIndex = (int)runnerConfigs.size() - 1;
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
	allFileItems.clear();
	UpdateFilteredFileList();

	// 更新配置显示
	index_last_selected = newIndex;
	UpdateConfigDisplayText(newIndex);

	// 刷新视图
	InvalidateRect(g_folderListView, nullptr, TRUE);
}

// 删除配置项
static void DeleteConfigItem(int itemIndex) {
	if (itemIndex < 0 || itemIndex >= static_cast<int>(runnerConfigs.size())) {
		return;
	}

	// 删除配置项
	runnerConfigs.erase(runnerConfigs.begin() + itemIndex);

	// 从列表视图中删除项
	ListView_DeleteItem(g_folderListView, itemIndex);

	// 清空列表选中项（如果还有项的话，会在后面重新选择）

	// 清空配置显示和文件列表
	UpdateConfigDisplayText(-1);
	allFileItems.clear();
	UpdateFilteredFileList();

	// 重置选中索引
	index_last_selected = -1;

	// 如果还有配置项，选中第一个
	if (!runnerConfigs.empty()) {
		int selectIndex = (itemIndex >= (int)runnerConfigs.size()) ? (int)runnerConfigs.size() - 1 : itemIndex;
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
	case WM_CREATE:
		{
			// 解析配置文件
			runnerConfigs = ParseRunnerConfig();

			// 创建右键菜单
			fileItemContextMenu = CreatePopupMenu();
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_EXCLUDE_ITEM, L"排除该项");
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_RENAME_ITEM, L"重命名该项");
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_CHECK_SHORTCUTS, L"检测快捷方式有效性");

			// 创建文件夹列表右键菜单
			folderContextMenu = CreatePopupMenu();
			AppendMenuW(folderContextMenu, MF_STRING, IDM_NEW_CONFIG, L"新建项");
			AppendMenuW(folderContextMenu, MF_STRING, IDM_DELETE_CONFIG, L"删除该项");

			// 创建保存按钮（在左侧区域上方）
			g_saveButton = CreateWindowExW(0, L"BUTTON", L"保存配置",
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											1, 1, leftPanelWidth, 30, hwnd, (HMENU)1002, GetModuleHandle(nullptr),
											nullptr);

			// 图标显示切换按钮（位于保存配置按钮下方，尺寸一致）
			g_toggleIconButton = CreateWindowExW(
				0, L"BUTTON", L"隐藏图标",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				1, 33, leftPanelWidth, 30,
				hwnd, (HMENU)IDC_TOGGLE_FILE_ICON, GetModuleHandle(nullptr), nullptr
			);

			// 创建左侧文件夹列表（向下移动给保存按钮留空间）
			g_folderListView = CreateWindowExW(0, WC_LISTVIEW, L"",
												WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
												1, 66, leftPanelWidth, 365, hwnd, nullptr, GetModuleHandle(nullptr),
												nullptr);
			ListView_SetExtendedListViewStyle(g_folderListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			// 设置文件夹列表的列
			LVCOLUMNW col = {};
			col.mask = LVCF_TEXT | LVCF_WIDTH;
			col.pszText = const_cast<LPWSTR>(L"索引配置");
			col.cx = 180;
			ListView_InsertColumn(g_folderListView, 0, &col);
			folderItemTexts.clear();

			// 添加文件夹到列表
			for (size_t i = 0; i < runnerConfigs.size(); ++i) {
				LVITEMW item = {};
				item.mask = LVIF_TEXT;
				item.iItem = static_cast<int>(i);
				item.iSubItem = 0;
				std::wstring command = runnerConfigs[i].command;
				if (runnerConfigs[i].type == L"folder" || runnerConfigs[i].type.empty()) {
					folderItemTexts.push_back(runnerConfigs[i].command);
				} else {
					folderItemTexts.push_back(L"( " + MyToUpper(runnerConfigs[i].type) + L" )");
				}
				item.pszText = const_cast<LPWSTR>(folderItemTexts[i].c_str());

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
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
											centerPanelX, editY, editWidth - 85, centerPanelEditHeight, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			editY += centerPanelEditHeight + 2;
			// Type 标签和下拉框
			CreateWindowExW(0, L"STATIC", L"类型:",
							WS_CHILD | WS_VISIBLE,
							centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
							GetModuleHandle(nullptr), nullptr);
			editY += centerPanelLabelHeight;
			g_typeComboBox = CreateWindowExW(0, L"COMBOBOX", L"",
											WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
											centerPanelX, editY, editWidth - 85, 100, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);

			// 添加下拉框选项
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"文件夹");
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"UWP 应用");
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"注册表应用");
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"%PATH% 环境变量");
			SendMessageW(g_typeComboBox, CB_SETCURSEL, 0, 0);
			editY += centerPanelEditHeight + 2;

			// Folder 标签和编辑框
			CreateWindowExW(0, L"STATIC", L"路径:",
							WS_CHILD | WS_VISIBLE,
							centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
							GetModuleHandle(nullptr), nullptr);
			editY += centerPanelLabelHeight;

			g_folderEdit = CreateWindowExW(0, L"EDIT", L"",
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
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
			editSyncHelper.Attach(g_renameSourcesEdit, g_renameTargetsEdit);
			// 索引按钮
			g_indexButton = CreateWindowExW(0, L"BUTTON", L"测试索引",
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											centerPanelX + 100, 0, 100, 32, hwnd, (HMENU)1001,
											GetModuleHandle(nullptr),
											nullptr);

			// 创建右侧文件列表
			g_fileFilterEdit = CreateWindowExW(
				0, L"EDIT", L"",
				WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
				530, 6, 180, 24,
				hwnd, (HMENU)IDC_FILE_FILTER_EDIT, GetModuleHandle(nullptr), nullptr
			);

			g_fileFilterClearButton = CreateWindowExW(
				0, L"BUTTON", L"x",
				WS_CHILD | BS_OWNERDRAW,
				714, 6, 24, 24,
				hwnd, (HMENU)IDC_FILE_FILTER_CLEAR, GetModuleHandle(nullptr), nullptr
			);
			ShowWindow(g_fileFilterClearButton, SW_HIDE);

			g_fileListView = CreateWindowExW(0, WC_LISTVIEW, L"",
											WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL |
											LVS_OWNERDATA,
											530, 34, 200, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
			ListView_SetExtendedListViewStyle(g_fileListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			// 绑定系统小图标列表到右侧文件列表
			SHFILEINFOW sfi = {};
			g_fileListSysImageList = reinterpret_cast<HIMAGELIST>(
				SHGetFileInfoW(
					L"C:\\",
					FILE_ATTRIBUTE_DIRECTORY,
					&sfi,
					sizeof(sfi),
					SHGFI_SYSICONINDEX | SHGFI_SMALLICON
				)
			);
			ListView_SetImageList(g_fileListView, g_fileListSysImageList, LVSIL_SMALL);
			UpdateFileListIconState();
			// 设置列表的列
			col.pszText = const_cast<LPWSTR>(L"文件名称");
			col.cx = 180;
			ListView_InsertColumn(g_fileListView, 0, &col);
			col.pszText = const_cast<LPWSTR>(L"文件路径");
			col.cx = 180;
			ListView_InsertColumn(g_fileListView, 1, &col);


			// 如果有配置，选择第一个
			if (!runnerConfigs.empty()) {
				ListView_SetItemState(g_folderListView, 0, LVIS_SELECTED, LVIS_SELECTED);
				index_last_selected = 0;
				UpdateConfigDisplayText(0);
				UpdateIndexFileList(0);
			}

			SHAutoComplete(g_commandEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_folderEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_excludeWordsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_excludesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_labelExcludesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_renameSourcesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_labelRenameSourcesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_labelRenameTargetsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_renameTargetsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_fileFilterEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);

			break;
		}

	case WM_COMMAND:
		{
			if (HIWORD(wParam) == CBN_SELCHANGE && (HWND)lParam == g_typeComboBox) {
				// 类型下拉框选择变化，根据类型设置folder编辑框状态
				int typeIndex = static_cast<int>(SendMessage(g_typeComboBox, CB_GETCURSEL, 0, 0));
				if (typeIndex == 0) {
					// folder
					// 启用folder编辑框，恢复之前保存的folder值
					EnableWindow(g_folderEdit, TRUE);
					EnableWindow(g_commandEdit, TRUE);
					if (index_last_selected >= 0) {
						SetWindowTextW(g_folderEdit, runnerConfigs[index_last_selected].folder.c_str());
						SetWindowTextW(g_commandEdit, runnerConfigs[index_last_selected].command.c_str());
					}
				} else {
					// uwp, regedit, path
					// 禁用folder编辑框，显示"default"
					EnableWindow(g_folderEdit, FALSE);
					EnableWindow(g_commandEdit, FALSE);
					SetWindowTextW(g_folderEdit, L"default");
					SetWindowTextW(g_commandEdit, L"default");
				}

				// 保存当前配置项
				if (index_last_selected >= 0) {
					SaveCurrentConfigItem(index_last_selected);
				}
			} else if (HIWORD(wParam) == EN_KILLFOCUS) {
				if ((HWND)lParam == g_commandEdit) {
					SyncCurrentConfigNameToLeftList();
				} else if ((HWND)lParam == g_folderEdit) {
					NormalizeFolderEditPathOnKillFocus();
				}
			} else if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_fileFilterEdit) {
				UpdateFilteredFileList();
			} else if (LOWORD(wParam) == 1001) {
				// 重新索引按钮
				// 检查编辑框是否为空
				if (IsEditControlsEmpty(std::vector<HWND>{g_commandEdit, g_folderEdit})) {
					TipEditEmpty();
					break;
				}
				int selectedIndex = ListView_GetNextItem(g_folderListView, -1, LVNI_SELECTED);
				if (selectedIndex >= 0) {
					ReindexCurrentConfig(selectedIndex);
				}
			} else if (LOWORD(wParam) == 1002) {
				// 保存按钮
				// 检查编辑框是否为空
				if (IsEditControlsEmpty(std::vector<HWND>{g_commandEdit, g_folderEdit})) {
					TipEditEmpty();
					break;
				}
				SaveCurrentConfigItem(index_last_selected);
				SaveConfigToFile(runnerConfigs);
				DestroyWindow(g_indexManagerHwnd);
				return 0;
			} else if (LOWORD(wParam) == IDM_EXCLUDE_ITEM) {
				// 排除该项
				if (rightClickedItemIndex >= 0) {
					ExcludeFileItem(rightClickedItemIndex);
					rightClickedItemIndex = -1;
				}
			} else if (LOWORD(wParam) == IDM_RENAME_ITEM) {
				// 重命名该项
				if (rightClickedItemIndex >= 0) {
					RenameFileItem(rightClickedItemIndex);
					rightClickedItemIndex = -1;
				}
			} else if (LOWORD(wParam) == IDM_CHECK_SHORTCUTS) {
				// 检测快捷方式有效性
				CheckShortcutValidity(g_indexManagerHwnd, allFileItems);
			} else if (LOWORD(wParam) == IDM_NEW_CONFIG) {
				// 新建配置项
				NewConfigItem();
			} else if (LOWORD(wParam) == IDM_DELETE_CONFIG) {
				// 删除配置项
				if (rightClickedConfigItemIndex >= 0) {
					DeleteConfigItem(rightClickedConfigItemIndex);
					rightClickedConfigItemIndex = -1;
				}
			} else if (LOWORD(wParam) == IDC_FILE_FILTER_CLEAR) {
				ClearFileFilter();
			} else if (LOWORD(wParam) == IDC_TOGGLE_FILE_ICON) {
				g_showFileIcons = !g_showFileIcons;
				UpdateFileListIconState();
			}
			break;
		}

	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;

			if (pnmh->hwndFrom == g_folderListView) {
				switch (pnmh->code) {
				case LVN_ITEMCHANGING:
					{
						LPNMLISTVIEW p = (LPNMLISTVIEW)lParam;

						// 只在状态变化且“选中位”发生变化时处理
						if ((p->uChanged & LVIF_STATE) && ((p->uOldState ^ p->uNewState) & LVIS_SELECTED)) {
							const bool goingToSelect = (p->uNewState & LVIS_SELECTED) != 0;
							const bool goingToDeselect = (p->uOldState & LVIS_SELECTED) != 0 && !goingToSelect;

							// 是否要阻止——当已有选中项且编辑框为空时，不允许切换/清空
							if (index_last_selected >= 0 &&
								IsEditControlsEmpty(std::vector<HWND>{g_commandEdit, g_folderEdit})) {
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
				case LVN_ITEMCHANGED:
					{
						// 选中变化
						LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
						if (pnmv->uNewState & LVIS_SELECTED) {
							// 当切换文件夹选择的时候保存当前配置
							if (index_last_selected >= 0 && index_last_selected != pnmv->iItem) {
								SaveCurrentConfigItem(index_last_selected);
								// 显式清除旧选中项的高亮状态
								ListView_SetItemState(g_folderListView, index_last_selected, 0, LVIS_SELECTED);
							}
							if (pnmv->iItem != index_last_selected) {
								index_last_selected = pnmv->iItem;
								UpdateConfigDisplayText(pnmv->iItem);
								UpdateIndexFileList(pnmv->iItem);
							}
						}
					}
					break;
				case NM_RCLICK:
					{
						// 文件夹列表右键点击
						LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)lParam;
						rightClickedConfigItemIndex = pnmia->iItem;

						// 获取鼠标位置
						POINT pt;
						GetCursorPos(&pt);

						// 显示右键菜单
						TrackPopupMenu(folderContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
					}
					break;

				default: break;
				}
			}
			if (pnmh->hwndFrom == g_fileListView && pnmh->code == LVN_GETDISPINFO) {
				NMLVDISPINFOW* plvdi = (NMLVDISPINFOW*)lParam;
				if (plvdi->item.iItem >= 0 && plvdi->item.iItem < (int)filteredFileItemIndices.size()) {
					const auto& item = allFileItems[filteredFileItemIndices[plvdi->item.iItem]];

					if ((plvdi->item.mask & LVIF_TEXT) && plvdi->item.pszText) {
						if (plvdi->item.iSubItem == 0) {
							// 第二列：文件名称
							lstrcpynW(plvdi->item.pszText, item.label.c_str(), plvdi->item.cchTextMax);
						} else if (plvdi->item.iSubItem == 1) {
							// 第三列：文件路径
							lstrcpynW(plvdi->item.pszText, item.file_path.c_str(), plvdi->item.cchTextMax);
						}
					}
					if (g_showFileIcons &&
						(plvdi->item.mask & LVIF_IMAGE) &&
						plvdi->item.iSubItem == 0) {
						plvdi->item.iImage = GetSysImageIndex(item.file_path);
					}
				}
			} else if (pnmh->hwndFrom == g_fileListView && pnmh->code == NM_RCLICK) {
				// 文件列表右键点击
				LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)lParam;
				rightClickedItemIndex = -1;
				if (pnmia->iItem >= 0 && pnmia->iItem < static_cast<int>(filteredFileItemIndices.size())) {
					rightClickedItemIndex = filteredFileItemIndices[pnmia->iItem];
				}

				if (rightClickedItemIndex >= 0 && rightClickedItemIndex < static_cast<int>(allFileItems.size())) {
					// 获取鼠标位置
					POINT pt;
					GetCursorPos(&pt);

					// 显示右键菜单
					TrackPopupMenu(fileItemContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
				}
			}


			break;
		}

	case WM_DRAWITEM:
		{
			const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
			if (wParam == IDC_FILE_FILTER_CLEAR && dis && dis->hwndItem == g_fileFilterClearButton) {
				HBRUSH brush = CreateSolidBrush(RGB(220, 53, 69));
				FillRect(dis->hDC, &dis->rcItem, brush);
				DeleteObject(brush);

				SetBkMode(dis->hDC, TRANSPARENT);
				SetTextColor(dis->hDC, RGB(255, 255, 255));
				DrawTextW(dis->hDC, L"x", -1, const_cast<RECT*>(&dis->rcItem),
						DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				if (dis->itemState & ODS_FOCUS) {
					DrawFocusRect(dis->hDC, &dis->rcItem);
				}
				return TRUE;
			}
			break;
		}

	case WM_SIZE:
		{
			// 处理窗口大小变化
			RECT rect;
			GetClientRect(hwnd, &rect);
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;
			int tempY = sizeChangePosY;
			int threeRectHeight = ((height - sizeChangePosY - (centerPanelLabelHeight * 2) - 4) / 3);
			// 重新调整控件大小
			int centerPanelWidth = static_cast<int>((width - leftPanelWidth) * 0.4) - 5;
			SetWindowPos(g_typeComboBox, nullptr, -1, -1, centerPanelWidth, centerPanelEditHeight,
						SWP_NOZORDER | SWP_NOMOVE);
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
			SetWindowPos(g_toggleIconButton, nullptr, 1, 33, leftPanelWidth, 32, SWP_NOZORDER);
			// 调整文件夹列表位置（留出保存按钮的空间）
			SetWindowPos(g_folderListView, nullptr, 1, 66, leftPanelWidth, height - 67, SWP_NOZORDER);
			int rightPanelX = static_cast<int>((width - leftPanelWidth) * 0.4 + leftPanelWidth) + 1;
			int rightPanelWidth = static_cast<int>((width - leftPanelWidth) * 0.6);
			int filterBarY = 6;
			int filterBarHeight = 24;
			int clearBtnWidth = 24;
			int margin = 6;
			int filterEditWidth = std::max(80, rightPanelWidth - clearBtnWidth - margin * 3);

			SetWindowPos(g_fileFilterEdit, nullptr, rightPanelX + margin, filterBarY,
						filterEditWidth, filterBarHeight, SWP_NOZORDER);
			SetWindowPos(g_fileFilterClearButton, nullptr, rightPanelX + margin * 2 + filterEditWidth, filterBarY,
						clearBtnWidth, filterBarHeight, SWP_NOZORDER);
			SetWindowPos(g_fileListView, nullptr, rightPanelX,
						filterBarY + filterBarHeight + margin,
						rightPanelWidth, height - (filterBarY + filterBarHeight + margin) - 1,
						SWP_NOZORDER);
			// 中间编辑区域保持固定布局，不需要调整
			break;
		}

	case WM_CLOSE:
		{
			int ret = MessageBoxW(hwnd, L"是否保存？", L"是否保存？", MB_OKCANCEL | MB_ICONQUESTION);
			if (ret == IDOK) {
				SendMessage(g_indexManagerHwnd, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED), 0);
			} else {
				DestroyWindow(g_indexManagerHwnd);
			}
			return 0;
		}

	case WM_DESTROY:
		{
			if (fileItemContextMenu) {
				DestroyMenu(fileItemContextMenu);
				fileItemContextMenu = nullptr;
			}
			if (folderContextMenu) {
				DestroyMenu(folderContextMenu);
				folderContextMenu = nullptr;
			}
			g_indexManagerHwnd = nullptr;
			g_folderListView = nullptr;
			g_fileListView = nullptr;
			g_fileFilterEdit = nullptr;
			g_fileFilterClearButton = nullptr;
			g_typeComboBox = nullptr;
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
			g_toggleIconButton = nullptr;
			allFileItems.clear();
			filteredFileItemIndices.clear();
			runnerConfigs.clear();
			index_last_selected = -1; // 重置选中索引状态
			rightClickedItemIndex = -1;
			rightClickedConfigItemIndex = -1;
			break;
		}

	default: return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static HBITMAP gBmpMainIcon = nullptr;

// 注册索引管理器窗口类
static void RegisterIndexManagerClass() {
	static bool registered = false;
	if (registered) return;

	if (gBmpMainIcon == nullptr) {
		gBmpMainIcon = LoadShell32IconProperAsBitmap(4, 16, 16);
	}

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = IndexManagerWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = L"IndexManagerWndClass";
	wc.hIcon = BitmapToIcon(gBmpMainIcon);

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
