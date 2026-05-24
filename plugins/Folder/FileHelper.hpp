#pragma once

#include <shtypes.h>

#include "model/TraverseOptions.hpp"
#include "util/StringUtil.hpp"

#include <windows.h>
#include <string>
#include <ShlObj.h>
#include <fstream>



static std::wstring GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
	PWSTR path = nullptr;
	HRESULT hr = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &path);

	if (FAILED(hr) || !path) {
		return L"";
	}

	std::wstring result(path);
	CoTaskMemFree(path);
	return result;
}

static std::wstring GetCurrentFolderPath(TraverseOptions& traverse_options) {
	const std::wstring folder = MyToLower(traverse_options.folder);

	static const std::unordered_map<std::wstring, KNOWNFOLDERID> folder_map = {
		{L"desktop", FOLDERID_Desktop},
		{L"downloads", FOLDERID_Downloads},
		{L"documents", FOLDERID_Documents},
		{L"pictures", FOLDERID_Pictures},
		{L"music", FOLDERID_Music},
		{L"videos", FOLDERID_Videos},
		{L"favorites", FOLDERID_Favorites},
		{L"searches", FOLDERID_SavedSearches},
		{L"links", FOLDERID_Links}
	};

	if (const auto it = folder_map.find(folder); it != folder_map.end()) {
		return GetKnownFolderPath(it->second);
	}
	return traverse_options.folder;
}

