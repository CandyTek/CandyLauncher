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
#define IDM_SHOW_EXCLUDED_ITEMS 2006
#define IDM_COPY_CONFIG 2007
#define IDC_FILE_FILTER_EDIT 3001
#define IDC_FILE_FILTER_CLEAR 3002
#define IDC_TOGGLE_FILE_ICON 3003
#define IDC_BROWSE_FOLDER 3004
#define IDC_CHECKBOX_INDEX_FILES_ONLY 3005
#define IDC_CHECKBOX_RECURSIVE_INDEX 3006
static HMENU fileItemContextMenu = nullptr;
static HMENU folderContextMenu = nullptr;

// 配置编辑控件
static HWND g_typeComboBox = nullptr;
static HWND g_nameEdit = nullptr;
static HWND g_folderEdit = nullptr;
static HWND g_excludeWordsEdit = nullptr;
static HWND g_excludesEdit = nullptr;
static HWND g_labelExcludes = nullptr;
static HWND g_renameSourcesEdit = nullptr;
static HWND g_labelRenameSourcesEdit = nullptr;
static HWND g_labelRenameTargetsEdit = nullptr;
static HWND g_renameTargetsEdit = nullptr;
static HWND g_labelExts = nullptr;
static HWND g_extsEdit = nullptr;
static HWND g_indexButton = nullptr;
static HWND g_saveButton = nullptr;
static HWND g_toggleIconButton = nullptr;
static HWND g_browseFolderButton = nullptr;
static HWND g_checkboxIndexFilesOnly = nullptr;
static HWND g_checkboxRecursiveIndex = nullptr;
static int g_folderEditY = 0;
static int leftPanelWidth = 170;
static int centerPanelEditHeight = 25;
// static bool g_showFileIcons = true;
static bool g_showFileIcons = false;

static int centerPanelX = leftPanelWidth + 4;
static int centerPanelLabelHeight = 20;

static int g_sizeChangePosY = 0;

static std::vector<TraverseOptions> runnerConfigs;
static std::vector<FileInfo> allFileItems;
static std::vector<int> filteredFileItemIndices;
static HWND g_excludedItemsHwnd = nullptr;
static HWND g_excludedItemsListView = nullptr;
static std::vector<FileInfo> excludedFileItems;
// 有一个类成员变量来存储所有文本
static std::vector<std::wstring> folderItemTexts;
static int index_last_selected = -1;
static EditScrollSync editSyncHelper;
static int rightClickedItemIndex = -1;
static int rightClickedConfigItemIndex = -1;
static bool g_configDirty = false;
static bool g_suppressDirty = false;

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

