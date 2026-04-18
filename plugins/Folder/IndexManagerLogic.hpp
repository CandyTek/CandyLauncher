#pragma once

#include "IndexManagerState.hpp"

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

static void UpdateFileFilterClearButtonState()
{
	if (!g_fileFilterEdit || !g_fileFilterClearButton) {
		return;
	}

	int len = GetWindowTextLengthW(g_fileFilterEdit);

	// 始终显示按钮
	ShowWindow(g_fileFilterClearButton, SW_SHOW);

	// 有内容时可点击；无内容时灰掉
	EnableWindow(g_fileFilterClearButton, len > 0);
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

	UpdateFileFilterClearButtonState();
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

static void CollectIndexFileListFromConfig(TraverseOptions config, std::vector<FileInfo>& outputItems) {
	outputItems.clear();
	if (config.type == L"folder" || config.type.empty()) {
		if (g_host->GetSettingsMap().at("pref_use_everything_sdk_index").boolValue) {
			g_host->TraverseFilesForEverythingSDK(config.folder, config,
												[&](const std::wstring& name, const std::wstring& fullPath,
													const std::wstring& parent, const std::wstring& ext) {
													FileInfo fileInfo;
													fileInfo.file_path = fullPath;
													fileInfo.label = name;
													outputItems.push_back(fileInfo);
												});
		} else {
			TraverseFiles(config.folder, config, EXE_FOLDER_PATH2,
						[&](const std::wstring& name, const std::wstring& fullPath,
							const std::wstring& parent, const std::wstring& ext) {
							FileInfo fileInfo;
							fileInfo.file_path = fullPath;
							fileInfo.label = name;
							outputItems.push_back(fileInfo);
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
						outputItems.push_back(fileInfo);
					}, config);
	} else if (config.type == L"regedit") {
		TraverseRegistryApps([&](const std::wstring& name,
								const std::wstring& fullPath,
								const std::wstring& parent) {
			FileInfo fileInfo;
			fileInfo.file_path = fullPath;
			fileInfo.label = name;
			outputItems.push_back(fileInfo);
		}, config);
	} else if (config.type == L"path") {
		TraversePATHExecutables2([&](const std::wstring& name,
									const std::wstring& fullPath,
									const std::wstring& parent,
									const std::wstring& ext) {
			FileInfo fileInfo;
			fileInfo.file_path = fullPath;
			fileInfo.label = name;
			outputItems.push_back(fileInfo);
		}, config, EXE_FOLDER_PATH2);
	}
}

static void CollectExcludedFileItemsFromConfig(const TraverseOptions& config, std::vector<FileInfo>& outputItems) {
	outputItems.clear();

	TraverseOptions rawConfig = config;
	rawConfig.excludeNames.clear();
	rawConfig.excludeWords.clear();
	rawConfig.renameSources.clear();
	rawConfig.renameTargets.clear();
	rawConfig.renameMap.clear();

	std::vector<FileInfo> rawItems;
	CollectIndexFileListFromConfig(rawConfig, rawItems);
	for (const auto& item : rawItems) {
		if (shouldExclude(config, item.file_path.filename().wstring())) {
			outputItems.push_back(item);
		}
	}
}

static LRESULT CALLBACK ExcludedItemsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		{
			g_excludedItemsListView = CreateWindowExW(0, WC_LISTVIEW, L"",
													WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
													0, 0, 700, 420, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
			ListView_SetExtendedListViewStyle(g_excludedItemsListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

			LVCOLUMNW col = {};
			col.mask = LVCF_TEXT | LVCF_WIDTH;
			col.pszText = const_cast<LPWSTR>(L"文件名称");
			col.cx = 220;
			ListView_InsertColumn(g_excludedItemsListView, 0, &col);
			col.pszText = const_cast<LPWSTR>(L"文件路径");
			col.cx = 460;
			ListView_InsertColumn(g_excludedItemsListView, 1, &col);

			for (int i = 0; i < static_cast<int>(excludedFileItems.size()); ++i) {
				LVITEMW item = {};
				item.mask = LVIF_TEXT;
				item.iItem = i;
				item.iSubItem = 0;
				item.pszText = const_cast<LPWSTR>(excludedFileItems[i].label.c_str());
				ListView_InsertItem(g_excludedItemsListView, &item);
				ListView_SetItemText(g_excludedItemsListView, i, 1,
									const_cast<LPWSTR>(excludedFileItems[i].file_path.c_str()));
			}

			ListView_SetColumnWidth(g_excludedItemsListView, 0, LVSCW_AUTOSIZE_USEHEADER);
			ListView_SetColumnWidth(g_excludedItemsListView, 1, LVSCW_AUTOSIZE_USEHEADER);
			break;
		}
	case WM_SIZE:
		{
			if (g_excludedItemsListView) {
				RECT rect;
				GetClientRect(hwnd, &rect);
				SetWindowPos(g_excludedItemsListView, nullptr, 0, 0,
							rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
			}
			break;
		}
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		g_excludedItemsHwnd = nullptr;
		g_excludedItemsListView = nullptr;
		excludedFileItems.clear();
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static void RegisterExcludedItemsWindowClass() {
	static bool registered = false;
	if (registered) return;

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = ExcludedItemsWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = L"ExcludedItemsWndClass";
	RegisterClassExW(&wc);
	registered = true;
}

static void ShowExcludedItemsWindow(HWND parent, const TraverseOptions& config) {
	CollectExcludedFileItemsFromConfig(config, excludedFileItems);

	if (g_excludedItemsHwnd != nullptr) {
		DestroyWindow(g_excludedItemsHwnd);
	}

	RegisterExcludedItemsWindowClass();

	RECT parentRect = {};
	GetWindowRect(parent, &parentRect);
	int windowWidth = 760;
	int windowHeight = 460;
	int x = parentRect.left + ((parentRect.right - parentRect.left) - windowWidth) / 2;
	int y = parentRect.top + ((parentRect.bottom - parentRect.top) - windowHeight) / 2;

	std::wstring title = L"查看已排除的项 (" + std::to_wstring(excludedFileItems.size()) + L")";
	g_excludedItemsHwnd = CreateWindowExW(
		0,
		L"ExcludedItemsWndClass",
		title.c_str(),
		WS_OVERLAPPEDWINDOW,
		x, y, windowWidth, windowHeight,
		parent,
		nullptr,
		GetModuleHandle(nullptr),
		nullptr
	);

	if (g_excludedItemsHwnd) {
		ShowWindow(g_excludedItemsHwnd, SW_SHOW);
		UpdateWindow(g_excludedItemsHwnd);
	}
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
	if (!g_nameEdit || !g_folderListView) {
		return;
	}

	wchar_t buffer[4096];
	GetWindowTextW(g_nameEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	runnerConfigs[index_last_selected].name = buffer;

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
	g_suppressDirty = true;
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(runnerConfigs.size())) {
		SendMessage(g_typeComboBox, CB_SETCURSEL, 0, 0);
		SetWindowTextW(g_nameEdit, L"");
		SetWindowTextW(g_folderEdit, L"");
		SetWindowTextW(g_excludeWordsEdit, L"");
		SetWindowTextW(g_excludesEdit, L"");
		SetWindowTextW(g_renameSourcesEdit, L"");
		SetWindowTextW(g_renameTargetsEdit, L"");
		SetWindowTextW(g_extsEdit, L"");
		// 默认启用所有控件
		EnableWindow(g_folderEdit, TRUE);
		SendMessageW(g_checkboxIndexFilesOnly, BM_SETCHECK, BST_CHECKED, 0);
		SendMessageW(g_checkboxRecursiveIndex, BM_SETCHECK, BST_CHECKED, 0);
		g_suppressDirty = false;
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
	bool isFolderType = (config.type == L"folder" || config.type.empty());
	if (isFolderType) {
		// folder类型：启用编辑框，显示实际路径
		EnableWindow(g_folderEdit, TRUE);
		SetWindowTextW(g_folderEdit, config.folder.c_str());
		EnableWindow(g_nameEdit, TRUE);
		SetWindowTextW(g_nameEdit, config.name.c_str());
		EnableWindow(g_checkboxIndexFilesOnly, TRUE);
		EnableWindow(g_checkboxRecursiveIndex, TRUE);
	} else {
		// uwp/regedit/path类型：禁用编辑框，显示"default"
		EnableWindow(g_folderEdit, FALSE);
		SetWindowTextW(g_folderEdit, L"default");
		EnableWindow(g_nameEdit, FALSE);
		SetWindowTextW(g_nameEdit, L"default");
		EnableWindow(g_checkboxIndexFilesOnly, FALSE);
		EnableWindow(g_checkboxRecursiveIndex, FALSE);
	}

	SendMessageW(g_checkboxIndexFilesOnly, BM_SETCHECK, config.indexFilesOnly ? BST_CHECKED : BST_UNCHECKED, 0);
	SendMessageW(g_checkboxRecursiveIndex, BM_SETCHECK, config.recursive ? BST_CHECKED : BST_UNCHECKED, 0);

	SetWindowTextW(g_excludeWordsEdit, VectorToString(config.excludeWords).c_str());
	SetWindowTextW(g_excludesEdit, VectorToString(config.excludeNames).c_str());
	SetWindowTextW(g_renameSourcesEdit, VectorToString(config.renameSources).c_str());
	SetWindowTextW(g_renameTargetsEdit, VectorToString(config.renameTargets).c_str());
	SetWindowTextW(g_extsEdit, VectorToString(config.extensions).c_str());
	g_suppressDirty = false;
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

	GetWindowTextW(g_nameEdit, buffer, sizeof(buffer));
	runnerConfigs[selectedIndex].name = buffer;

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

	GetWindowTextW(g_extsEdit, buffer, sizeof(buffer));
	{
		auto rawExts = StringToVector(std::wstring(buffer));
		std::vector<std::wstring> normalizedExts;
		for (auto ext : rawExts) {
			if (!ext.empty() && ext[0] != L'.') ext.insert(ext.begin(), L'.');
			normalizedExts.push_back(ext);
		}
		runnerConfigs[selectedIndex].extensions = normalizedExts;
	}

	runnerConfigs[selectedIndex].indexFilesOnly =
		(SendMessageW(g_checkboxIndexFilesOnly, BM_GETCHECK, 0, 0) == BST_CHECKED);
	runnerConfigs[selectedIndex].recursive =
		(SendMessageW(g_checkboxRecursiveIndex, BM_GETCHECK, 0, 0) == BST_CHECKED);

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

	GetWindowTextW(g_nameEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	config.name = buffer;

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

	GetWindowTextW(g_extsEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
	{
		auto rawExts = StringToVector(std::wstring(buffer));
		std::vector<std::wstring> normalizedExts;
		for (auto ext : rawExts) {
			if (!ext.empty() && ext[0] != L'.') ext.insert(ext.begin(), L'.');
			normalizedExts.push_back(ext);
		}
		config.extensions = normalizedExts;
	}

	config.indexFilesOnly =
		(SendMessageW(g_checkboxIndexFilesOnly, BM_GETCHECK, 0, 0) == BST_CHECKED);
	config.recursive =
		(SendMessageW(g_checkboxRecursiveIndex, BM_GETCHECK, 0, 0) == BST_CHECKED);

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
	CollectIndexFileListFromConfig(config, allFileItems);
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
	originalName = fileInfo.label;

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
	newConfig.name = L"new";

	// 添加到配置集合
	runnerConfigs.push_back(newConfig);

	// 添加新项到列表视图
	int newIndex = (int)runnerConfigs.size() - 1;
	LVITEMW item = {};
	item.mask = LVIF_TEXT;
	item.iItem = newIndex;
	item.iSubItem = 0;
	item.pszText = const_cast<LPWSTR>(newConfig.name.c_str());
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


