#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib") // Link against the shell library
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")

#include "ListedRunnerPlugin.h"
#include "DataKeeper.hpp"
#include "TraverseFilesHelper.hpp"
#include "MainTools.hpp"

#include <shobjidl.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>
#include <objbase.h>
#include <combaseapi.h> // for CreateStreamOnHGlobal
#include <sstream> // for CreateStreamOnHGlobal
#include <codecvt>
#include <objidl.h>
#include <gdiplus.h>

#include <filesystem>
#include <propkey.h>  // For property keys (optional but good practice)
#include <shlwapi.h>       // For Path* functions (optional)
#include <propvarutil.h>   // Optional for PROPVARIANT helpers
#include <VersionHelpers.h>  // 添加这个头文件

// 不能少
#include <shellapi.h>
#include <Everything.h>

#ifndef ESI_ALLITEMS
#define ESI_ALLITEMS 0x00000040
#endif


static std::unordered_map<std::string, std::function<void()>> callbackFunctions;

ListedRunnerPlugin::ListedRunnerPlugin() {
}

ListedRunnerPlugin::ListedRunnerPlugin(
		const std::unordered_map<std::string, std::function<void()>> &callbackFunction1) {
	callbackFunctions = callbackFunction1;
	LoadConfiguration();
}

const std::vector<std::shared_ptr<RunCommandAction>> &ListedRunnerPlugin::GetActions() const {
	return actions;
}


void ListedRunnerPlugin::LoadConfiguration() {
	MethodTimerStart();
	bool isUwpAdded = false;
	bool isPathAdded = false;
	bool isRegeditAdded = false;

	actions.clear();
	actions.push_back(std::make_shared<ActionNormal>(L"刷新列表", L"刷新运行配置", EXE_FOLDER_PATH + L"\\refresh.ico",
													 callbackFunctions["refreshList"]));
	actions.push_back(std::make_shared<ActionNormal>(L"退出软件", L"退出软件", EXE_FOLDER_PATH + L"\\refresh.ico",
													 callbackFunctions["quit"]));
	actions.push_back(std::make_shared<ActionNormal>(L"重启软件", L"重启软件", EXE_FOLDER_PATH + L"\\refresh.ico",
													 callbackFunctions["restart"]));
	actions.push_back(
			std::make_shared<ActionNormal>(L"软件设置", L"打开本软件设置界面", EXE_FOLDER_PATH + L"\\refresh.ico",
										   callbackFunctions["settings"]));
	std::vector<TraverseOptions> runnerConfigs = ParseRunnerConfig();
	for (TraverseOptions traverseOptions1: runnerConfigs) {
		if (traverseOptions1.type.empty()) {
			if (g_settings_map["pref_use_everything_sdk_index"].boolValue) {
				::TraverseFilesForEverythingSDK(traverseOptions1.folder, traverseOptions1, [&](const std::wstring &name,
																							   const std::wstring &fullPath,
																							   const std::wstring &parent,
																							   const std::wstring &ext) {
					const auto action = std::make_shared<RunCommandAction>(
							name, fullPath, false, true, parent
					);
					actions.push_back(action);
				});
			} else {
				::TraverseFiles(traverseOptions1.folder, traverseOptions1, [&](const std::wstring &name,
																			   const std::wstring &fullPath,
																			   const std::wstring &parent,
																			   const std::wstring &ext) {
					const auto action = std::make_shared<RunCommandAction>(
							name, fullPath, false, true, parent
					);
					actions.push_back(action);
				});

			}
		} else if (traverseOptions1.type == L"uwp" && !isUwpAdded &&
				   g_settings_map["pref_indexed_uwp_apps_enable"].boolValue) {
			TraverseUwpApps(actions, traverseOptions1);
			isUwpAdded = true;
		} else if (traverseOptions1.type == L"path" && !isPathAdded &&
				   g_settings_map["pref_indexed_envpath_apps"].boolValue) {
			TraversePATHExecutables2(actions, traverseOptions1);
			isPathAdded = true;
		} else if (traverseOptions1.type == L"regedit" && !isRegeditAdded &&
				   g_settings_map["pref_indexed_regedit_apps"].boolValue) {
			TraverseRegistryApps(actions, traverseOptions1);
			isRegeditAdded = true;
		}

	}

	TraverseOptions emptyTraverseOptions;
	TraverseOptions defaultOptions = CreateDefaultPathTraverseOptions();
	if (!isUwpAdded && g_settings_map["pref_indexed_uwp_apps_enable"].boolValue) {
		TraverseUwpApps(actions, emptyTraverseOptions);
	} else if (!isPathAdded && g_settings_map["pref_indexed_envpath_apps"].boolValue) {
		// TODO: 解决这个崩溃问题
		TraversePATHExecutables2(actions, defaultOptions);
	} else if (!isRegeditAdded && g_settings_map["pref_indexed_regedit_apps"].boolValue) {
		TraverseRegistryApps(actions, emptyTraverseOptions);
	}

	MethodTimerEnd(L"loadlist");
}


std::vector<std::shared_ptr<RunCommandAction>> ListedRunnerPlugin::Search(const std::wstring &query) const {
	std::vector<std::shared_ptr<RunCommandAction>> results;
	const std::vector<std::wstring> words = SplitWords(query);

	for (const std::shared_ptr<RunCommandAction> &action: actions) {
		bool match = true;
		for (const auto &word: words) {
			if (action->GetTitle().find(word) == std::wstring::npos) {
				match = false;
				break;
			}
		}
		if (match) {
			results.push_back(action);
			if (results.size() >= 7) break;
		}
	}

	return results;
}

std::vector<std::wstring> ListedRunnerPlugin::SplitWords(const std::wstring &input) {
	std::vector<std::wstring> result;
	size_t start = 0, end = 0;
	while ((end = input.find(L' ', start)) != std::wstring::npos) {
		if (end > start)
			result.push_back(input.substr(start, end - start));
		start = end + 1;
	}
	if (start < input.size())
		result.push_back(input.substr(start));
	return result;
}

HICON ListedRunnerPlugin::GetFileIcon(const std::wstring &filePath, const bool largeIcon) {
	SHFILEINFOW sfi = {0};;
	UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
	flags |= largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON;

	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
	return sfi.hIcon;
}

static HBITMAP IconToBitmap(HICON hIcon) {
	if (!hIcon) return nullptr;

	ICONINFO iconInfo;
	GetIconInfo(hIcon, &iconInfo);

	return iconInfo.hbmColor;
}

HBITMAP ListedRunnerPlugin::GetIconFromPath(const std::wstring &path) {
	std::wstring actualPath = path;

	if (MyEndsWith(path, L".lnk")) {
		actualPath = GetShortcutTarget(path);
	}

	HICON hIcon = GetFileIcon(actualPath, true);
	HBITMAP hBitmap = IconToBitmap(hIcon);
	DestroyIcon(hIcon);
	return hBitmap;
}

void ListedRunnerPlugin::WriteDefaultConfig(const std::wstring &path) {
	using json = nlohmann::json;

	json jArray = json::array();
	jArray.push_back({
							 {"Command",            "ProgramData开始菜单"},
							 {"FolderPath",         R"(C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs)"},
							 {"IsSearchSubFolder",  true},
							 {"ExcludeNameWordArr", {"uninstall", "帮助", "help"}}
					 });

	// std::ofstream out(path);

	std::ofstream out;
	out.open(path.c_str()); // ✅ 使用 c_str() 得到 const wchar_t*

	out << jArray.dump(4); // Pretty-print JSON
	out.close();
}

// Helper: 获取 PNG 编码器 CLSID
static int GetEncoderClsid(const WCHAR *format, CLSID *pClsid) {
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;

	Gdiplus::ImageCodecInfo *pImageCodecInfo = static_cast<Gdiplus::ImageCodecInfo *>(malloc(size));
	if (!pImageCodecInfo) return -1;

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT i = 0; i < num; ++i) {
		if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[i].Clsid;
			free(pImageCodecInfo);
			return i;
		}
	}

	free(pImageCodecInfo);
	return -1;
}

static std::vector<BYTE> HBitmapToByteArray(HBITMAP hBitmap) {
	std::vector<BYTE> buffer;
	if (!hBitmap) return buffer;

	Gdiplus::Bitmap bmp(hBitmap, nullptr);
	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);

	IStream *pStream = nullptr;
	CreateStreamOnHGlobal(nullptr, TRUE, &pStream);

	bmp.Save(pStream, &pngClsid, nullptr);

	STATSTG stat;
	pStream->Stat(&stat, STATFLAG_NONAME);
	const ULONG size = static_cast<ULONG>(stat.cbSize.QuadPart);

	buffer.resize(size);
	const LARGE_INTEGER liZero = {};
	pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);
	ULONG read = 0;
	pStream->Read(buffer.data(), size, &read);
	pStream->Release();

	return buffer;
}


void ListedRunnerPlugin::TraverseUwpApps(
		std::vector<std::shared_ptr<RunCommandAction>> &outActions, const TraverseOptions &options
) {
	// *** UWP应用程序加载逻辑 ***
	if (IsWindows8OrGreater())
		try {
			LoadUwpApps(outActions, options);
		}
		catch (const std::exception &e) {
			std::wcerr << L"Failed to load UWP apps: " << Utf8ToWString(e.what()) << std::endl;
		}

}

