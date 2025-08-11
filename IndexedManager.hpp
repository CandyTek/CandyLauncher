#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <commctrl.h> // For ListView controls
#include "json.hpp"
#include "BaseTools.hpp" // For utf8_to_wide

// 索引管理器窗口相关定义
static HWND g_indexManagerHwnd = nullptr;
static HWND g_folderListView = nullptr;
static HWND g_configTextBox = nullptr;
static HWND g_shortcutListView = nullptr;

struct RunnerConfig {
	std::string command;
	std::string folder;
	std::vector<std::string> exclude_words;
	std::vector<std::string> excludes;
	std::vector<std::string> rename_sources;
	std::vector<std::string> rename_targets;
};

static std::vector<RunnerConfig> g_runnerConfigs;

// 解析 runner.json 文件
static std::vector<RunnerConfig> ParseRunnerConfig() {
	std::vector<RunnerConfig> configs;

	try {
		std::ifstream file("runner.json");
		if (!file.is_open()) {
			// 如果文件不存在，返回空配置
			return configs;
		}

		nlohmann::json j;
		file >> j;

		for (const auto &item: j) {
			RunnerConfig config;
			config.command = item.value("command", "");
			config.folder = item.value("folder", "");

			if (item.contains("exclude_words")) {
				config.exclude_words = item["exclude_words"];
			}
			if (item.contains("excludes")) {
				config.excludes = item["excludes"];
			}
			if (item.contains("rename_sources")) {
				config.rename_sources = item["rename_sources"];
			}
			if (item.contains("rename_targets")) {
				config.rename_targets = item["rename_targets"];
			}

			configs.push_back(config);
		}
	}
	catch (const std::exception &e) {
		MessageBoxA(nullptr, ("Failed to parse runner.json: " + std::string(e.what())).c_str(),
					"Error", MB_OK | MB_ICONERROR);
	}

	return configs;
}

// 获取指定文件夹中的所有快捷方式
static std::vector<std::wstring> GetShortcutsInFolder(const std::string &folderPath) {
	std::vector<std::wstring> shortcuts;

	std::wstring wFolderPath = utf8_to_wide(folderPath);
	std::wstring searchPattern = wFolderPath + L"\\*.lnk";

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				std::wstring fileName = findData.cFileName;
				// 移除 .lnk 扩展名
				if (fileName.length() > 4 && fileName.substr(fileName.length() - 4) == L".lnk") {
					fileName = fileName.substr(0, fileName.length() - 4);
				}
				shortcuts.push_back(fileName);
			}
		} while (FindNextFileW(hFind, &findData));

		FindClose(hFind);
	}

	return shortcuts;
}

// 更新配置显示区域
static void UpdateConfigDisplay(int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(g_runnerConfigs.size())) {
		SetWindowTextA(g_configTextBox, "");
		return;
	}

	const RunnerConfig &config = g_runnerConfigs[selectedIndex];

	std::string configText = "Command: " + config.command + "\r\n\r\n";
	configText += "Folder: " + config.folder + "\r\n\r\n";

	configText += "Exclude Words:\r\n";
	for (const auto &word: config.exclude_words) {
		configText += "  " + word + "\r\n";
	}
	configText += "\r\n";

	configText += "Excludes:\r\n";
	for (const auto &exclude: config.excludes) {
		configText += "  " + exclude + "\r\n";
	}
	configText += "\r\n";

	configText += "Rename Sources:\r\n";
	for (const auto &source: config.rename_sources) {
		configText += "  " + source + "\r\n";
	}
	configText += "\r\n";

	configText += "Rename Targets:\r\n";
	for (const auto &target: config.rename_targets) {
		configText += "  " + target + "\r\n";
	}

	SetWindowTextA(g_configTextBox, configText.c_str());
}

// 更新快捷方式列表
static void UpdateShortcutList(int selectedIndex) {
	// 清空列表
	ListView_DeleteAllItems(g_shortcutListView);

	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(g_runnerConfigs.size())) {
		return;
	}

	const RunnerConfig &config = g_runnerConfigs[selectedIndex];
	std::vector<std::wstring> shortcuts = GetShortcutsInFolder(config.folder);

	// 添加快捷方式到列表视图
	for (size_t i = 0; i < shortcuts.size(); ++i) {
		LVITEMW item = {};
		item.mask = LVIF_TEXT;
		item.iItem = static_cast<int>(i);
		item.iSubItem = 0;
		item.pszText = const_cast<LPWSTR>(shortcuts[i].c_str());
		ListView_InsertItem(g_shortcutListView, &item);
	}
}

// 索引管理器窗口过程
static LRESULT CALLBACK IndexManagerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			// 解析配置文件
			g_runnerConfigs = ParseRunnerConfig();

			// 创建左侧文件夹列表
			g_folderListView = CreateWindowExW(0, WC_LISTVIEW, L"",
											   WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
											   10, 10, 200, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

			// 设置文件夹列表的列
			LVCOLUMNW col = {};
			col.mask = LVCF_TEXT | LVCF_WIDTH;
			col.pszText = const_cast<LPWSTR>(L"Folders");
			col.cx = 180;
			ListView_InsertColumn(g_folderListView, 0, &col);

			// 添加文件夹到列表
			for (size_t i = 0; i < g_runnerConfigs.size(); ++i) {
				LVITEMW item = {};
				item.mask = LVIF_TEXT;
				item.iItem = static_cast<int>(i);
				item.iSubItem = 0;
				std::wstring command = utf8_to_wide(g_runnerConfigs[i].command);
				item.pszText = const_cast<LPWSTR>(command.c_str());
				ListView_InsertItem(g_folderListView, &item);
			}

			// 创建中间配置显示区域
			g_configTextBox = CreateWindowExW(0, L"EDIT", L"",
											  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY |
											  WS_VSCROLL,
											  220, 10, 300, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

			// 创建右侧快捷方式列表
			g_shortcutListView = CreateWindowExW(0, WC_LISTVIEW, L"",
												 WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
												 530, 10, 200, 400, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

			// 设置快捷方式列表的列
			col.pszText = const_cast<LPWSTR>(L"Shortcuts");
			col.cx = 180;
			ListView_InsertColumn(g_shortcutListView, 0, &col);

			// 如果有配置，选择第一个
			if (!g_runnerConfigs.empty()) {
				ListView_SetItemState(g_folderListView, 0, LVIS_SELECTED, LVIS_SELECTED);
				UpdateConfigDisplay(0);
				UpdateShortcutList(0);
			}

			break;
		}

		case WM_NOTIFY: {
			LPNMHDR pnmh = (LPNMHDR) lParam;
			if (pnmh->hwndFrom == g_folderListView && pnmh->code == LVN_ITEMCHANGED) {
				LPNMLISTVIEW pnmv = (LPNMLISTVIEW) lParam;
				if (pnmv->uNewState & LVIS_SELECTED) {
					UpdateConfigDisplay(pnmv->iItem);
					UpdateShortcutList(pnmv->iItem);
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

			// 重新调整控件大小
			SetWindowPos(g_folderListView, nullptr, 10, 10, 200, height - 20, SWP_NOZORDER);
			SetWindowPos(g_configTextBox, nullptr, 220, 10, width - 450, height - 20, SWP_NOZORDER);
			SetWindowPos(g_shortcutListView, nullptr, width - 220, 10, 200, height - 20, SWP_NOZORDER);
			break;
		}

		case WM_CLOSE: {
			ShowWindow(hwnd, SW_HIDE);
			return 0;
		}

		case WM_DESTROY: {
			g_indexManagerHwnd = nullptr;
			g_folderListView = nullptr;
			g_configTextBox = nullptr;
			g_shortcutListView = nullptr;
			break;
		}

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
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
	int windowWidth = 750;
	int windowHeight = 450;
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
