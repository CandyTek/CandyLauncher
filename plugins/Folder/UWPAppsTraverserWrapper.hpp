#pragma once


#include <windows.h>
#include <iostream>
#include <filesystem>
#include "../../model/TraverseOptions.hpp"
#include "../../util/MainTools.hpp"
#include "util/FileSystemTraverser.hpp"
#include "util/UWPAppsTraverser.hpp"

#include "plugins/Folder/FileAction.hpp"
#pragma comment(lib, "Psapi.lib")


// 废弃的，不生效
static void TraverseUWPApps(
	const TraverseOptions& options,
	std::vector<std::shared_ptr<FileAction>>& outActions) {
	TraverseUWPApps(options, [&](const std::wstring& name,
								const std::wstring& fullPath) {
		const auto action = std::make_shared<FileAction>(
			name, fullPath, false, L""
		);
		outActions.push_back(action);
	});
}


static void LoadUwpApps(
	std::vector<std::shared_ptr<BaseAction>>& outActions,
	const TraverseOptions& options) {
	LoadUwpApps([&](const std::wstring& name,
					const std::wstring& fullPath,
					const std::wstring& uwpCommandS,
					const HBITMAP hBitmap) {
		const auto action = std::make_shared<FileAction>(
			name, fullPath
		);
		action->iconBitmap = hBitmap;
		action->uwpSource = uwpCommandS;
		outActions.push_back(action);
	}, options);
}


static void traverseUwpApps(
	std::vector<std::shared_ptr<BaseAction>>& outActions, const TraverseOptions& options
) {
	// *** UWP应用程序加载逻辑 ***
	if (IsWindows8OrGreater())
		try {
			LoadUwpApps(outActions, options);
		} catch (const std::exception& e) {
			std::wcerr << L"Failed to load UWP apps: " << Utf8ToWString(e.what()) << std::endl;
		}
}
