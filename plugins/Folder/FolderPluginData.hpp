//
// Created by Administrator on 2025/10/1.
//
#pragma once

#include <string>
#include <functional>

#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "model/TraverseOptions.hpp"
#include "util/BaseTools.hpp"
#include "util/FileUtil.hpp"


inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring RUNNER_CONFIG_PATH2 = EXE_FOLDER_PATH2 + L"\\plugins\\config_folder_plugin.json";

inline const candy::Host* m_host = nullptr;

// 索引管理器 / 自动化编辑器保存后在工作线程上重新索引并通知主程序
inline std::function<void()> g_refreshFolderPlugin = nullptr;

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
