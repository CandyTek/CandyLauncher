#pragma once

#include <thread>
#include "IndexManagerLogic.hpp"

static HHOOK g_indexMgrKeyHook = nullptr;

static LRESULT CALLBACK IndexMgrGetMsgHook(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION && wParam == PM_REMOVE && g_indexManagerHwnd) {
		MSG* pMsg = reinterpret_cast<MSG*>(lParam);
		if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'S' &&
			(GetKeyState(VK_CONTROL) & 0x8000)) {
			HWND h = pMsg->hwnd;
			while (h && h != g_indexManagerHwnd) h = GetParent(h);
			if (h == g_indexManagerHwnd) {
				SendMessageW(g_indexManagerHwnd, WM_KEYDOWN, 'S', pMsg->lParam);
				pMsg->message = WM_NULL;
			}
		}
	}
	return CallNextHookEx(g_indexMgrKeyHook, nCode, wParam, lParam);
}

static HFONT FONT_DEFAULT;

static void setFont(HWND hwnd) {
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)FONT_DEFAULT, TRUE);
}

static void initFont(HWND hwnd) {
	HDC hdc = GetDC(hwnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hwnd, hdc);

	int height = -MulDiv(9, dpi, 72);

	FONT_DEFAULT = CreateFontW(
		height, 0, 0, 0,
		FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, VARIABLE_PITCH,
		L"Microsoft YaHei UI"
	);
}

static void setCustomText(HWND hwnd) {
	setFont(hwnd);
}

static void setCustomEdit(HWND hwnd) {
	setFont(hwnd);
}

static void setCustomButton(HWND hwnd) {
	setFont(hwnd);
}

// 索引管理器窗口过程
static LRESULT CALLBACK IndexManagerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		{
			initFont(hwnd);

			// 解析配置文件
			runnerConfigs = ParseRunnerConfig();

			// 创建右侧索引列表右键菜单
			fileItemContextMenu = CreatePopupMenu();
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_EXCLUDE_ITEM, L"排除该项");
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_RENAME_ITEM, L"重命名该项");
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_CHECK_SHORTCUTS, L"检测快捷方式有效性");
			AppendMenuW(fileItemContextMenu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(fileItemContextMenu, MF_STRING, IDM_SHOW_EXCLUDED_ITEMS, L"查看已排除的项");

			// 创建左测配置列表右键菜单
			folderContextMenu = CreatePopupMenu();
			AppendMenuW(folderContextMenu, MF_STRING, IDM_NEW_CONFIG, L"新建项");
			AppendMenuW(folderContextMenu, MF_STRING, IDM_COPY_CONFIG, L"复制项");
			AppendMenuW(folderContextMenu, MF_STRING, IDM_DELETE_CONFIG, L"删除该项");

			// 创建保存按钮（在左侧区域上方）
			g_saveButton = CreateWindowExW(0, L"BUTTON", L"保存配置(Ctrl+S)",
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											1, 1, leftPanelWidth, 32, hwnd, (HMENU)1002, GetModuleHandle(nullptr),
											nullptr);
			setCustomButton(g_saveButton);

			// 创建图标显示切换按钮
			g_toggleIconButton = CreateWindowExW(
				0, L"BUTTON", L"隐藏图标",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				1, 33, leftPanelWidth, 32,
				hwnd, (HMENU)IDC_TOGGLE_FILE_ICON, GetModuleHandle(nullptr), nullptr
			);
			setCustomButton(g_toggleIconButton);
			// 创建索引按钮
			g_indexButton = CreateWindowExW(0, L"BUTTON", L"测试索引",
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											1, 65, leftPanelWidth, 32, hwnd, (HMENU)1001,
											GetModuleHandle(nullptr),
											nullptr);
			setCustomButton(g_indexButton);

			// 创建左侧文件夹列表（向下移动给保存按钮留空间）
			g_folderListView = CreateWindowExW(0, WC_LISTVIEW, L"",
												WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
												1, 97, leftPanelWidth, 365, hwnd, nullptr, GetModuleHandle(nullptr),
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
				std::wstring command = runnerConfigs[i].name;
				if (runnerConfigs[i].type == L"folder" || runnerConfigs[i].type.empty()) {
					folderItemTexts.push_back(runnerConfigs[i].name);
				} else {
					folderItemTexts.push_back(L"( " + MyToUpper(runnerConfigs[i].type) + L" )");
				}
				item.pszText = const_cast<LPWSTR>(folderItemTexts[i].c_str());

				ListView_InsertItem(g_folderListView, &item);
			}

			// 创建中间配置编辑区域
			int editY = 1;
			int editWidth = 300;
			int labelWidth = 120;
			int spacing = 35;


			// Command 标签和编辑框
			HWND labelName = CreateWindowExW(0, L"STATIC", L"名称:",
											WS_CHILD | WS_VISIBLE,
											centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			setCustomText(labelName);
			editY += centerPanelLabelHeight;
			g_nameEdit = CreateWindowExW(0, L"EDIT", L"",
										WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
										centerPanelX, editY, editWidth - 85, centerPanelEditHeight, hwnd, nullptr,
										GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_nameEdit);
			editY += centerPanelEditHeight + 2;
			// Type 标签和下拉框
			HWND labelType = CreateWindowExW(0, L"STATIC", L"类型:",
											WS_CHILD | WS_VISIBLE,
											centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			setCustomText(labelType);
			editY += centerPanelLabelHeight;
			g_typeComboBox = CreateWindowExW(0, L"COMBOBOX", L"",
											WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
											centerPanelX, editY, editWidth - 85, 100, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			setCustomText(g_typeComboBox);

			// 添加下拉框选项
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"文件夹");
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"UWP 应用");
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"注册表应用");
			SendMessageW(g_typeComboBox, CB_ADDSTRING, 0, (LPARAM)L"%PATH% 环境变量");
			SendMessageW(g_typeComboBox, CB_SETCURSEL, 0, 0);
			editY += centerPanelEditHeight + 2;

			// Folder 标签和编辑框
			HWND labelPath = CreateWindowExW(0, L"STATIC", L"路径:",
											WS_CHILD | WS_VISIBLE,
											centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			setCustomText(labelPath);
			editY += centerPanelLabelHeight;

			g_folderEditY = editY;
			g_folderEdit = CreateWindowExW(0, L"EDIT", L"",
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
											centerPanelX, editY, editWidth - 85 - 57, centerPanelEditHeight, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_folderEdit);
			g_browseFolderButton = CreateWindowExW(0, L"BUTTON", L"浏览",
													WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
													centerPanelX + editWidth - 85 - 55, editY, 55, centerPanelEditHeight,
													hwnd, (HMENU)IDC_BROWSE_FOLDER, GetModuleHandle(nullptr), nullptr);
			setCustomButton(g_browseFolderButton);
			editY += centerPanelEditHeight + 2;
			// "仅索引文件" 复选框
			g_checkboxIndexFilesOnly = CreateWindowExW(
				0,
				L"BUTTON",
				L"仅索引文件",
				WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
				centerPanelX, editY, 120, centerPanelLabelHeight,
				hwnd,
				(HMENU)IDC_CHECKBOX_INDEX_FILES_ONLY,
				GetModuleHandle(nullptr),
				nullptr
			);
			SendMessageW(g_checkboxIndexFilesOnly, BM_SETCHECK, BST_CHECKED, 0);
			setCustomText(g_checkboxIndexFilesOnly);

			// "递归索引" 复选框
			g_checkboxRecursiveIndex = CreateWindowExW(
				0,
				L"BUTTON",
				L"递归索引",
				WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
				centerPanelX + 124, editY, 120, centerPanelLabelHeight,
				hwnd,
				(HMENU)IDC_CHECKBOX_RECURSIVE_INDEX,
				GetModuleHandle(nullptr),
				nullptr
			);
			SendMessageW(g_checkboxRecursiveIndex, BM_SETCHECK, BST_CHECKED, 0);
			setCustomText(g_checkboxRecursiveIndex);

			editY += centerPanelEditHeight + 2;

			g_sizeChangePosY = editY;

			// Exclude Words 标签和 扩展名标签
			HWND labelExcludeWord = CreateWindowExW(0, L"STATIC", L"排除词汇:",
													WS_CHILD | WS_VISIBLE,
													centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd, nullptr,
													GetModuleHandle(nullptr), nullptr);
			setCustomText(labelExcludeWord); // 以下控件使用动态宽高，WM_SIZE里调整
			g_labelExts = CreateWindowExW(0, L"STATIC", L"扩展名:",
										WS_CHILD | WS_VISIBLE,
										centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
										nullptr, GetModuleHandle(nullptr), nullptr);
			setCustomText(g_labelExts);
			editY += centerPanelLabelHeight;
			// Exclude Words编辑框和扩展名编辑框
			g_excludeWordsEdit = CreateWindowExW(0, L"EDIT", L"",
												WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
												centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
												GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_excludeWordsEdit);
			g_extsEdit = CreateWindowExW(0, L"EDIT", L"",
										WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL |
										ES_AUTOHSCROLL,
										centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
										GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_extsEdit);
			editY += 70;

			// Excludes 标签和编辑框
			g_labelExcludes = CreateWindowExW(0, L"STATIC", L"排除名称:",
											WS_CHILD | WS_VISIBLE,
											centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
											nullptr, GetModuleHandle(nullptr), nullptr);
			setCustomText(g_labelExcludes);
			g_excludesEdit = CreateWindowExW(0, L"EDIT", L"",
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL,
											centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
											GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_excludesEdit);
			editY += 70;

			// Rename Sources 标签和编辑框
			g_labelRenameSourcesEdit = CreateWindowExW(0, L"STATIC", L"原索引名称:",
														WS_CHILD | WS_VISIBLE,
														centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
														nullptr, GetModuleHandle(nullptr), nullptr);
			setCustomText(g_labelRenameSourcesEdit);
			g_renameSourcesEdit = CreateWindowExW(0, L"EDIT", L"",
												WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL |
												ES_AUTOHSCROLL,
												centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
												GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_renameSourcesEdit);
			editY += 70;

			// Rename Targets 标签和编辑框
			g_labelRenameTargetsEdit = CreateWindowExW(0, L"STATIC", L"重命名:",
														WS_CHILD | WS_VISIBLE,
														centerPanelX, editY, labelWidth, centerPanelLabelHeight, hwnd,
														nullptr, GetModuleHandle(nullptr), nullptr);
			setCustomText(g_labelRenameTargetsEdit);
			g_renameTargetsEdit = CreateWindowExW(0, L"EDIT", L"",
												WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_VSCROLL |
												ES_AUTOHSCROLL,
												centerPanelX, editY, editWidth - 85, 60, hwnd, nullptr,
												GetModuleHandle(nullptr), nullptr);
			setCustomEdit(g_renameTargetsEdit);

			editY += 80;
			editSyncHelper.Attach(g_renameSourcesEdit, g_renameTargetsEdit);

			// 创建右侧文件列表
			g_fileFilterEdit = CreateWindowExW(
				0, L"EDIT", L"",
				WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
				530, 6, 180, 24,
				hwnd, (HMENU)IDC_FILE_FILTER_EDIT, GetModuleHandle(nullptr), nullptr
			);
			setCustomEdit(g_fileFilterEdit);

			g_fileFilterClearButton = CreateWindowExW(
				0, L"BUTTON", L"x",
				WS_CHILD | BS_OWNERDRAW|WS_VISIBLE,
				714, 6, 24, 24,
				hwnd, (HMENU)IDC_FILE_FILTER_CLEAR, GetModuleHandle(nullptr), nullptr
			);
			setCustomButton(g_saveButton);

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

			SHAutoComplete(g_nameEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_folderEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_excludeWordsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_excludesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_labelExcludes, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_renameSourcesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_labelRenameSourcesEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_labelRenameTargetsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_extsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_renameTargetsEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
			SHAutoComplete(g_fileFilterEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);

			break;
		}

	case WM_COMMAND:
		{
			if (HIWORD(wParam) == EN_CHANGE && !g_suppressDirty) {
				g_configDirty = true;
			}
			if (HIWORD(wParam) == BN_CLICKED &&
				(LOWORD(wParam) == IDC_CHECKBOX_INDEX_FILES_ONLY || LOWORD(wParam) == IDC_CHECKBOX_RECURSIVE_INDEX)) {
				g_configDirty = true;
			}
			if (HIWORD(wParam) == CBN_SELCHANGE && (HWND)lParam == g_typeComboBox) {
				g_configDirty = true;
				// 类型下拉框选择变化，根据类型设置folder编辑框状态
				int typeIndex = static_cast<int>(SendMessage(g_typeComboBox, CB_GETCURSEL, 0, 0));
				if (typeIndex == 0) {
					// folder
					// 启用folder编辑框，恢复之前保存的folder值
					EnableWindow(g_folderEdit, TRUE);
					EnableWindow(g_nameEdit, TRUE);
					EnableWindow(g_checkboxIndexFilesOnly, TRUE);
					EnableWindow(g_checkboxRecursiveIndex, TRUE);
					if (index_last_selected >= 0) {
						SetWindowTextW(g_folderEdit, runnerConfigs[index_last_selected].folder.c_str());
						SetWindowTextW(g_nameEdit, runnerConfigs[index_last_selected].name.c_str());
					}
				} else {
					// uwp, regedit, path
					// 禁用folder编辑框，显示"default"
					EnableWindow(g_folderEdit, FALSE);
					EnableWindow(g_nameEdit, FALSE);
					EnableWindow(g_checkboxIndexFilesOnly, FALSE);
					EnableWindow(g_checkboxRecursiveIndex, FALSE);
					SetWindowTextW(g_folderEdit, L"default");
					SetWindowTextW(g_nameEdit, L"default");
				}

				// 保存当前配置项
				if (index_last_selected >= 0) {
					SaveCurrentConfigItem(index_last_selected);
				}
			} else if (HIWORD(wParam) == EN_KILLFOCUS) {
				if ((HWND)lParam == g_nameEdit) {
					SyncCurrentConfigNameToLeftList();
				} else if ((HWND)lParam == g_folderEdit) {
					NormalizeFolderEditPathOnKillFocus();
				}
			} else if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_fileFilterEdit) {
				UpdateFilteredFileList();
			} else if (LOWORD(wParam) == 1001) {
				// 重新索引按钮
				// 检查编辑框是否为空
				if (IsEditControlsEmpty(std::vector<HWND>{g_nameEdit, g_folderEdit})) {
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
				if (IsEditControlsEmpty(std::vector<HWND>{g_nameEdit, g_folderEdit})) {
					TipEditEmpty();
					break;
				}
				SaveCurrentConfigItem(index_last_selected);
				SaveConfigToFile(runnerConfigs);
				g_configDirty = false;
				DestroyWindow(g_indexManagerHwnd);
				if (g_refreshFolderPlugin) {
					auto cb = g_refreshFolderPlugin;
					std::thread([cb]() { cb(); }).detach();
				}
				return 0;
			} else if (LOWORD(wParam) == IDM_EXCLUDE_ITEM) {
				// 排除该项
				if (rightClickedItemIndex >= 0) {
					ExcludeFileItem(rightClickedItemIndex);
					rightClickedItemIndex = -1;
					g_configDirty = true;
				}
			} else if (LOWORD(wParam) == IDM_RENAME_ITEM) {
				// 重命名该项
				if (rightClickedItemIndex >= 0) {
					RenameFileItem(rightClickedItemIndex);
					rightClickedItemIndex = -1;
					g_configDirty = true;
				}
			} else if (LOWORD(wParam) == IDM_CHECK_SHORTCUTS) {
				// 检测快捷方式有效性
				CheckShortcutValidity(g_indexManagerHwnd, allFileItems);
			} else if (LOWORD(wParam) == IDM_SHOW_EXCLUDED_ITEMS) {
				if (index_last_selected >= 0) {
					TraverseOptions currentConfig = BuildCurrentConfigFromControls(index_last_selected);
					ShowExcludedItemsWindow(g_indexManagerHwnd, currentConfig);
				}
			} else if (LOWORD(wParam) == IDM_NEW_CONFIG) {
				// 新建配置项
				NewConfigItem();
				g_configDirty = true;
			} else if (LOWORD(wParam) == IDM_COPY_CONFIG) {
				// 复制配置项
				if (rightClickedConfigItemIndex >= 0 && rightClickedConfigItemIndex < static_cast<int>(runnerConfigs.size())) {
					TraverseOptions copied = runnerConfigs[rightClickedConfigItemIndex];
					copied.name += L"(2)";
					runnerConfigs.push_back(copied);
					int newIndex = static_cast<int>(runnerConfigs.size()) - 1;
					std::wstring label = (copied.type == L"folder" || copied.type.empty())
											? copied.name
											: L"( " + MyToUpper(copied.type) + L" )";
					folderItemTexts.push_back(label);
					LVITEMW item = {};
					item.mask = LVIF_TEXT;
					item.iItem = newIndex;
					item.iSubItem = 0;
					item.pszText = const_cast<LPWSTR>(folderItemTexts[newIndex].c_str());
					ListView_InsertItem(g_folderListView, &item);
					ListView_SetItemState(g_folderListView, newIndex, LVIS_SELECTED, LVIS_SELECTED);
					ListView_EnsureVisible(g_folderListView, newIndex, FALSE);
					index_last_selected = newIndex;
					UpdateConfigDisplayText(newIndex);
					allFileItems.clear();
					UpdateFilteredFileList();
					rightClickedConfigItemIndex = -1;
				}
			} else if (LOWORD(wParam) == IDM_DELETE_CONFIG) {
				// 删除配置项
				if (rightClickedConfigItemIndex >= 0) {
					DeleteConfigItem(rightClickedConfigItemIndex);
					rightClickedConfigItemIndex = -1;
					g_configDirty = true;
				}
			} else if (LOWORD(wParam) == IDC_BROWSE_FOLDER) {
				IFileOpenDialog* pfd = nullptr;
				if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
					DWORD options = 0;
					pfd->GetOptions(&options);
					pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
					if (SUCCEEDED(pfd->Show(hwnd))) {
						IShellItem* psi = nullptr;
						if (SUCCEEDED(pfd->GetResult(&psi))) {
							PWSTR path = nullptr;
							if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
								SetWindowTextW(g_folderEdit, path);
								CoTaskMemFree(path);
							}
							psi->Release();
						}
					}
					pfd->Release();
				}
			} else if (LOWORD(wParam) == IDC_FILE_FILTER_CLEAR) {
				ClearFileFilter();
			} else if (LOWORD(wParam) == IDC_TOGGLE_FILE_ICON) {
				g_showFileIcons = !g_showFileIcons;
				UpdateFileListIconState();
			}
			break;
		}
	case WM_KEYDOWN:
		{
			if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'S') {
				// 模拟点击保存按钮
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED), 0);
				return 0;
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

						// 只在状态变化且"选中位"发生变化时处理
						if ((p->uChanged & LVIF_STATE) && ((p->uOldState ^ p->uNewState) & LVIS_SELECTED)) {
							const bool goingToSelect = (p->uNewState & LVIS_SELECTED) != 0;
							const bool goingToDeselect = (p->uOldState & LVIS_SELECTED) != 0 && !goingToSelect;

							// 是否要阻止——当已有选中项且编辑框为空时，不允许切换/清空
							if (index_last_selected >= 0 &&
								IsEditControlsEmpty(std::vector<HWND>{g_nameEdit, g_folderEdit})) {
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

				UINT itemMenuState = (rightClickedItemIndex >= 0 &&
										rightClickedItemIndex < static_cast<int>(allFileItems.size()))
										? MF_ENABLED
										: MF_GRAYED;
				EnableMenuItem(fileItemContextMenu, IDM_EXCLUDE_ITEM, MF_BYCOMMAND | itemMenuState);
				EnableMenuItem(fileItemContextMenu, IDM_RENAME_ITEM, MF_BYCOMMAND | itemMenuState);
				EnableMenuItem(fileItemContextMenu, IDM_SHOW_EXCLUDED_ITEMS,
								MF_BYCOMMAND | (index_last_selected >= 0 ? MF_ENABLED : MF_GRAYED));

				// 获取鼠标位置
				POINT pt;
				GetCursorPos(&pt);

				// 显示右键菜单
				TrackPopupMenu(fileItemContextMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
			}


			break;
		}

	case WM_DRAWITEM:
		{
			const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
			if (wParam == IDC_FILE_FILTER_CLEAR && dis && dis->hwndItem == g_fileFilterClearButton) {
				const bool enabled = IsWindowEnabled(g_fileFilterClearButton) != FALSE;

				COLORREF bgColor   = enabled ? RGB(220, 53, 69)   : RGB(200, 200, 200);
				COLORREF textColor = enabled ? RGB(255, 255, 255) : RGB(245, 245, 245);

				HBRUSH brush = CreateSolidBrush(bgColor);
				FillRect(dis->hDC, &dis->rcItem, brush);
				DeleteObject(brush);

				SetBkMode(dis->hDC, TRANSPARENT);
				SetTextColor(dis->hDC, textColor);
				DrawTextW(dis->hDC, L"x", -1, const_cast<RECT*>(&dis->rcItem),
						  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				if ((dis->itemState & ODS_FOCUS) && enabled) {
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
			int tempY = g_sizeChangePosY;
			// 用于窗口中间区域，动态调整三层编辑框高度
			int threeRectHeight = (height - g_sizeChangePosY - (centerPanelLabelHeight * 3) - 4) / 3;
			// 窗口中间区域的宽度
			int centerPanelWidth = static_cast<int>((width - leftPanelWidth) * 0.4) - 5;
			// 重新调整控件大小
			SetWindowPos(g_typeComboBox, nullptr, -1, -1, centerPanelWidth, centerPanelEditHeight,
						SWP_NOZORDER | SWP_NOMOVE);
			SetWindowPos(g_nameEdit, nullptr, -1, -1, centerPanelWidth, centerPanelEditHeight,
						SWP_NOZORDER | SWP_NOMOVE);
			SetWindowPos(g_folderEdit, nullptr, -1, -1, centerPanelWidth - 57, centerPanelEditHeight,
						SWP_NOZORDER | SWP_NOMOVE);
			SetWindowPos(g_browseFolderButton, nullptr, centerPanelX + centerPanelWidth - 55, g_folderEditY - 1,
						55, centerPanelEditHeight + 2, SWP_NOZORDER);
			SetWindowPos(g_excludeWordsEdit, nullptr, -1, -1, centerPanelWidth / 2 - 1, threeRectHeight,
						SWP_NOZORDER | SWP_NOMOVE);
			SetWindowPos(g_extsEdit, nullptr, centerPanelX + centerPanelWidth / 2, tempY + centerPanelLabelHeight, centerPanelWidth / 2,
						threeRectHeight,
						SWP_NOZORDER);

			SetWindowPos(g_labelExts, nullptr, centerPanelX + centerPanelWidth / 2, tempY, -1, -1, SWP_NOZORDER | SWP_NOSIZE);
			tempY += threeRectHeight + 2 + centerPanelLabelHeight;
			SetWindowPos(g_labelExcludes, nullptr, centerPanelX, tempY, -1, -1, SWP_NOZORDER | SWP_NOSIZE);
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

			// 调整文件夹列表位置（留出保存按钮的空间）
			SetWindowPos(g_folderListView, nullptr, -1, -1, leftPanelWidth, height - 98, SWP_NOMOVE | SWP_NOZORDER);
			int rightPanelX = static_cast<int>((width - leftPanelWidth) * 0.4 + leftPanelWidth) + 1;
			int rightPanelWidth = static_cast<int>((width - leftPanelWidth) * 0.6);
			int filterBarY = 6;
			int filterBarHeight = 24;
			int clearBtnWidth = 24;
			int margin = 6;
			int filterEditWidth = std::max(80, rightPanelWidth/3 );

			SetWindowPos(g_fileFilterEdit, nullptr, width-clearBtnWidth-margin- filterEditWidth, filterBarY,
						filterEditWidth, filterBarHeight, SWP_NOZORDER);
			SetWindowPos(g_fileFilterClearButton, nullptr, width-clearBtnWidth-1, filterBarY,
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
			if (g_configDirty) {
				int ret = MessageBoxW(hwnd, L"是否保存？", L"是否保存？", MB_OKCANCEL | MB_ICONQUESTION);
				if (ret == IDOK) {
					SendMessage(g_indexManagerHwnd, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED), 0);
				} else {
					DestroyWindow(g_indexManagerHwnd);
				}
			} else {
				DestroyWindow(g_indexManagerHwnd);
			}
			return 0;
		}

	case WM_DESTROY:
		{
			HWND hOwner = GetWindow(hwnd, GW_OWNER);
			if (hOwner) {
				ShowWindow(hOwner, SW_SHOW);
				SetForegroundWindow(hOwner);
			}
			if (fileItemContextMenu) {
				DestroyMenu(fileItemContextMenu);
				fileItemContextMenu = nullptr;
			}
			if (folderContextMenu) {
				DestroyMenu(folderContextMenu);
				folderContextMenu = nullptr;
			}
			if (g_excludedItemsHwnd) {
				DestroyWindow(g_excludedItemsHwnd);
			}
			g_indexManagerHwnd = nullptr;
			g_folderListView = nullptr;
			g_fileListView = nullptr;
			g_fileFilterEdit = nullptr;
			g_fileFilterClearButton = nullptr;
			g_typeComboBox = nullptr;
			g_nameEdit = nullptr;
			g_folderEdit = nullptr;
			g_excludeWordsEdit = nullptr;
			g_excludesEdit = nullptr;
			g_labelExcludes = nullptr;
			g_renameSourcesEdit = nullptr;
			g_labelRenameSourcesEdit = nullptr;
			g_labelRenameTargetsEdit = nullptr;
			g_renameTargetsEdit = nullptr;
			g_indexButton = nullptr;
			g_saveButton = nullptr;
			g_toggleIconButton = nullptr;
			g_browseFolderButton = nullptr;
			g_checkboxIndexFilesOnly = nullptr;
			g_checkboxRecursiveIndex = nullptr;
			allFileItems.clear();
			filteredFileItemIndices.clear();
			runnerConfigs.clear();
			index_last_selected = -1; // 重置选中索引状态
			rightClickedItemIndex = -1;
			rightClickedConfigItemIndex = -1;
			g_configDirty = false;
			g_suppressDirty = false;
			if (g_indexMgrKeyHook) {
				UnhookWindowsHookEx(g_indexMgrKeyHook);
				g_indexMgrKeyHook = nullptr;
			}
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
		g_indexMgrKeyHook = SetWindowsHookExW(WH_GETMESSAGE, IndexMgrGetMsgHook, nullptr, GetCurrentThreadId());
		ShowWindow(g_indexManagerHwnd, SW_SHOW);
		UpdateWindow(g_indexManagerHwnd);
	}
}
