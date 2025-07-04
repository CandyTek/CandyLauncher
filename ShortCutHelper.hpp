#pragma once
//
// Created by Administrator on 2025/7/4.
//

#ifndef SHORTCUTHELPER_H
#define SHORTCUTHELPER_H

#endif //SHORTCUTHELPER_H

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <shellapi.h>
#include <tchar.h>
#include <thread>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// 创建快捷方式到Startup文件夹
static bool CreateStartupShortcut(const std::wstring& exePath, const std::wstring& shortcutName) {
    wchar_t startupPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath))) {
        return false;
    }

    std::wstring shortcutPath = std::wstring(startupPath) + L"\\" + shortcutName + L".lnk";

    // 初始化 COM
    CoInitialize(nullptr);
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

