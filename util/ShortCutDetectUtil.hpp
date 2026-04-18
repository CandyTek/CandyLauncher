#pragma once
#include <windows.h>
#include <string>
#include <sstream>
#include <ShlObj.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <fcntl.h>
#include <shlwapi.h>

#include "BaseTools.hpp"
// #include "../common/GlobalState.hpp"
#include <propkey.h>
#include <propvarutil.h>

struct ComInitGuard {
	ComInitGuard() {
		hr = CoInitialize(nullptr);
	}

	~ComInitGuard() {
		if (SUCCEEDED(hr)) CoUninitialize();
	}

	HRESULT hr;
};

static std::wstring ExpandEnvStrings(const std::wstring& s) {
    if (s.empty()) return s;

    DWORD need = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
    if (need == 0) return s;

    std::wstring out;
    out.resize(need);
    DWORD written = ExpandEnvironmentStringsW(s.c_str(), out.data(), need);
    if (written == 0) return s;

    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

static bool FileOrDirExists(const std::wstring& path) {
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
}

static bool LooksLikeClsidOrShellTarget(const std::wstring& s) {
    if (s.empty()) return false;

    // shell:AppsFolder / shell:downloads 等
    if (s.rfind(L"shell:", 0) == 0) return true;

    // ::{GUID}
    if (s.find(L"::{") != std::wstring::npos) return true;
    if (s.find(L"shell:::{") != std::wstring::npos) return true;

    return false;
}

static bool LooksLikeAppUserModelID(const std::wstring& s) {
    if (s.empty()) return false;

    // 典型格式：
    // 21814BlastApps.BlastSearch_pdn8zwjh47aq4!FluentSearch
    // 特征：有 '!'，前半段像包族名
    return s.find(L'!') != std::wstring::npos;
}

static bool IsExplorerAppsFolderShortcut(const std::wstring& path, const std::wstring& args) {
    std::wstring p = path;
    std::wstring a = args;

    for (auto& ch : p) ch = (wchar_t)towlower(ch);
    for (auto& ch : a) ch = (wchar_t)towlower(ch);

    // 常见：explorer.exe shell:AppsFolder\PackageFamilyName!App
    if (p.find(L"explorer.exe") != std::wstring::npos &&
        a.find(L"appsfolder") != std::wstring::npos) {
        return true;
    }

    if (a.find(L"shell:appsfolder") != std::wstring::npos) {
        return true;
    }

    return false;
}

static bool TryReadAppUserModelID(IShellLinkW* pShellLink, std::wstring& outAumid) {
    outAumid.clear();

    IPropertyStore* pStore = nullptr;
    HRESULT hr = pShellLink->QueryInterface(IID_PPV_ARGS(&pStore));
    if (FAILED(hr) || !pStore) return false;

    PROPVARIANT pv;
    PropVariantInit(&pv);

    hr = pStore->GetValue(PKEY_AppUserModel_ID, &pv);
    if (SUCCEEDED(hr) && pv.vt == VT_LPWSTR && pv.pwszVal && pv.pwszVal[0] != L'\0') {
        outAumid = pv.pwszVal;
        PropVariantClear(&pv);
        pStore->Release();
        return true;
    }

    PropVariantClear(&pv);
    pStore->Release();
    return false;
}

static bool IsUrlLike(const std::wstring& s) {
	if (s.empty()) return false;

	std::wstring lower = s;
	for (auto& c : lower) c = (wchar_t)towlower(c);

	return lower.rfind(L"http://", 0) == 0 ||
		   lower.rfind(L"https://", 0) == 0 ||
		   lower.rfind(L"ftp://", 0) == 0 ||
		   lower.rfind(L"ftps://", 0) == 0 ||
		   lower.rfind(L"file://", 0) == 0 ||
		   lower.rfind(L"mailto:", 0) == 0;
}

static std::wstring ReadLinkTargetParsingPath(IShellLinkW* pShellLink) {
	IPropertyStore* pps = nullptr;
	std::wstring result;

	if (SUCCEEDED(pShellLink->QueryInterface(IID_PPV_ARGS(&pps))) && pps) {
		PROPVARIANT pv;
		PropVariantInit(&pv);

		if (SUCCEEDED(pps->GetValue(PKEY_Link_TargetParsingPath, &pv))) {
			if (pv.vt == VT_LPWSTR && pv.pwszVal) {
				result = pv.pwszVal;
			}
			PropVariantClear(&pv);
		}
		pps->Release();
	}

	return result;
}

static bool IsShortcutInvalid(const std::wstring& shortcutPath) {
    ComInitGuard guard;
    if (FAILED(guard.hr)) return true;

    IShellLinkW* pShellLink = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pShellLink)
    );
    if (FAILED(hr) || !pShellLink) return true;

    IPersistFile* pPersistFile = nullptr;
    hr = pShellLink->QueryInterface(IID_PPV_ARGS(&pPersistFile));
    if (FAILED(hr) || !pPersistFile) {
        pShellLink->Release();
        return true;
    }

    hr = pPersistFile->Load(shortcutPath.c_str(), STGM_READ);
    pPersistFile->Release();
    if (FAILED(hr)) {
        pShellLink->Release();
        return true;
    }

    wchar_t rawPath[MAX_PATH * 4] = {0};
    WIN32_FIND_DATAW wfd = {};
    hr = pShellLink->GetPath(rawPath, ARRAYSIZE(rawPath), &wfd, SLGP_RAWPATH);

    std::wstring targetPath;
    if (SUCCEEDED(hr) && rawPath[0] != L'\0') {
        targetPath = ExpandEnvStrings(rawPath);
    }
	if (targetPath.empty()) {
		targetPath = ReadLinkTargetParsingPath(pShellLink);
	}

    wchar_t args[MAX_PATH * 4] = {0};
    hr = pShellLink->GetArguments(args, ARRAYSIZE(args));
    std::wstring arguments = SUCCEEDED(hr) ? args : L"";

	// URL 类型（直接视为有效）
	if (IsUrlLike(targetPath) || IsUrlLike(arguments)) {
		pShellLink->Release();
		return false;
	}
	
    // 1) 明确识别“特殊快捷方式”
    if (LooksLikeClsidOrShellTarget(targetPath) ||
        LooksLikeClsidOrShellTarget(arguments) ||
        LooksLikeAppUserModelID(targetPath) ||
        LooksLikeAppUserModelID(arguments) ||
        IsExplorerAppsFolderShortcut(targetPath, arguments)) {
        pShellLink->Release();
        return false;
    }

    std::wstring aumid;
    if (TryReadAppUserModelID(pShellLink, aumid) && !aumid.empty()) {
        pShellLink->Release();
        return false;
    }

    // 2) 普通文件快捷方式：路径非空，就按文件存在性判断
    if (!targetPath.empty()) {
        bool exists = FileOrDirExists(targetPath);
        pShellLink->Release();
        return !exists;
    }

    // 3) 路径为空但有 PIDL：尝试从 PIDL 解析目标
    PIDLIST_ABSOLUTE pidl = nullptr;
    hr = pShellLink->GetIDList(&pidl);
    if (SUCCEEDED(hr) && pidl != nullptr) {
        bool pidlValid = false;

        PWSTR pszParsing = nullptr;
        if (SUCCEEDED(SHGetNameFromIDList(pidl, SIGDN_DESKTOPABSOLUTEPARSING, &pszParsing)) && pszParsing) {
            std::wstring parsedName = pszParsing;
            CoTaskMemFree(pszParsing);

            if (IsUrlLike(parsedName) ||
                LooksLikeClsidOrShellTarget(parsedName) ||
                LooksLikeAppUserModelID(parsedName) ||
                FileOrDirExists(parsedName)) {
                pidlValid = true;
            }
        }

        CoTaskMemFree(pidl);
        pShellLink->Release();
        return !pidlValid;
    }

    pShellLink->Release();
    return true;
}
