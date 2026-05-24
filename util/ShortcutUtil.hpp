#pragma once

#include <shlwapi.h>

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include "util/StringUtil.hpp"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")


inline std::wstring GetShortcutTarget(const std::wstring& lnkPath) {
	CoInitialize(nullptr);

	std::wstring target;

	IShellLink* psl = nullptr;
	if (SUCCEEDED(
		CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink,
			reinterpret_cast<void **>(&psl)
		))) {
		IPersistFile* ppf = nullptr;
		if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf)))) {
			if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
				WCHAR szPath[MAX_PATH];
				WIN32_FIND_DATA wfd = {0};
				if (SUCCEEDED(psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_UNCPRIORITY))) {
					target = szPath;
				}
			}
			ppf->Release();
		}
		psl->Release();
	}

	CoUninitialize();
	return target;
}

inline std::wstring SaveGetShortcutTarget(const std::wstring& lnkPath) {
	DWORD attr = GetFileAttributesW(lnkPath.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
	} else {
		if (EndsWith(lnkPath, L".lnk")) {
			std::wstring actualPath = GetShortcutTarget(lnkPath);
			return actualPath;
		}
	}
	return L"";
}

inline std::wstring SaveGetShortcutTargetAndReturn(const std::wstring& lnkPath) {
	DWORD attr = GetFileAttributesW(lnkPath.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
	} else {
		if (EndsWith(lnkPath, L".lnk")) {
			std::wstring actualPath = GetShortcutTarget(lnkPath);
			return actualPath;
		}
	}
	return lnkPath;
}


// 创建快捷方式到Startup文件夹
static bool CreateStartupShortcut(const std::wstring& exePath, const std::wstring& shortcutName) {
	wchar_t startupPath[MAX_PATH];
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) {
		return false;
	}

	std::wstring shortcutPath = std::wstring(startupPath) + L"\\" + shortcutName + L".lnk";

	// 初始化 COM
	HRESULT _ignore = CoInitialize(nullptr);
	IShellLinkW* pShellLink = nullptr;
	IPersistFile* pPersistFile = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
								IID_IShellLinkW, (LPVOID*)&pShellLink);
	if (SUCCEEDED(hr)) {
		pShellLink->SetPath(exePath.c_str());
		pShellLink->SetDescription(L"My App Auto Start");
		hr = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
		if (SUCCEEDED(hr)) {
			hr = pPersistFile->Save(shortcutPath.c_str(), TRUE);
			pPersistFile->Release();
		}
		pShellLink->Release();
	}

	CoUninitialize();
	return SUCCEEDED(hr);
}

static bool DeleteStartupShortcut(const std::wstring& shortcutName) {
	wchar_t startupPath[MAX_PATH];
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) {
		return false;
	}
	std::wstring shortcutPath = std::wstring(startupPath) + L"\\" + shortcutName + L".lnk";
	return DeleteFileW(shortcutPath.c_str()) != 0;
}

static bool DetectDeleteFile(const std::wstring& path) {
	return DeleteFileW(path.c_str()) != 0;
}


// 检查是否存在启动项快捷方式
static bool IsStartupShortcutExists(const std::wstring& shortcutName) {
	wchar_t startupPath[MAX_PATH];
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) {
		return false;
	}

	std::wstring shortcutPath = std::wstring(startupPath) + L"\\" + shortcutName + L".lnk";
	return PathFileExistsW(shortcutPath.c_str());
}
