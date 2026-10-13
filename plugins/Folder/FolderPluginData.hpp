//
// Created by Administrator on 2025/10/1.
//
#pragma once

#include <windows.h>
#include <string>
#include <functional>

#include "util/LogUtil.hpp"
#include "util/BackgroundWorkTracker.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "model/TraverseOptions.hpp"
#include "util/BaseTools.hpp"
#include "util/FileUtil.hpp"


inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring RUNNER_CONFIG_PATH2 = EXE_FOLDER_PATH2 + L"\\plugins\\config_folder_plugin.json";

inline const candy::Host* m_host = nullptr;

// 索引管理器 / 自动化编辑器保存后在工作线程上重新索引并通知主程序
inline std::function<void()> g_refreshFolderPlugin = nullptr;

// 本插件发起的全部后台线程（启动条目、自动化动作组、保存后刷新）都经这里启动，
// Shutdown 时统一取消并等待，避免 DLL 卸载后线程继续执行插件代码
inline BackgroundWorkTracker g_folderWork;

// 本插件 DLL 的模块句柄。窗口类的窗口过程编译在本 DLL 里，必须用它注册/注销，
// 而不是主程序的 GetModuleHandle(nullptr)，否则卸载后主程序进程里会残留指向已释放代码的窗口类
inline HINSTANCE FolderModuleHandle() {
	static const HINSTANCE instance = [] {
		HMODULE module = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							reinterpret_cast<LPCWSTR>(&FolderModuleHandle), &module);
		return static_cast<HINSTANCE>(module);
	}();
	return instance;
}

// Shutdown 正在销毁插件自己的窗口：WM_DESTROY 里不要再把主窗口拉到前台
inline bool g_folderWindowsClosing = false;

// 注册窗口类。同名类已存在说明上次卸载没注销干净，它指向的窗口过程可能已经失效，先注销再注册本次的
inline bool RegisterFolderWindowClass(WNDCLASSEXW& wc) {
	if (RegisterClassExW(&wc)) return true;
	if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
	UnregisterClassW(wc.lpszClassName, wc.hInstance);
	return RegisterClassExW(&wc) != 0;
}

// 把插件的 TraverseOptions 转成主程序 traverse_files 需要的参数，folder 由调用方指定（可能已展开为实际路径）
inline candy::TraverseOptions MakeHostTraverseOptions(const TraverseOptions& options, const std::wstring& folder) {
	candy::TraverseOptions result;
	result.folder = folder;
	result.extensions = options.extensions;
	result.excludeNames = options.excludeNames;
	result.excludeWords = options.excludeWords;
	result.renameSources = options.renameSources;
	result.renameTargets = options.renameTargets;
	result.recursive = options.recursive;
	result.indexFilesOnly = options.indexFilesOnly;
	return result;
}
