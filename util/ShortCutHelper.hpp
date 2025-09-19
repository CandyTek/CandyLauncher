#pragma once
//
// Created by Administrator on 2025/7/4.
//

#ifndef SHORTCUTHELPER_H
#define SHORTCUTHELPER_H
#include <shlwapi.h>

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


static const wchar_t* kLinkName = L"CandyLauncher 索引文件夹.lnk";
static const wchar_t* kLinkDesc = L"CandyLauncher 索引文件夹";

static bool InstallSendToEntry(const std::wstring& exePath)
{
    HRESULT hr = CoInitialize(nullptr);
    bool needUninit = SUCCEEDED(hr);

    PWSTR sendto = nullptr;
    std::wstring linkPath;
    bool ok = false;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
        linkPath.assign(sendto);
        if (!linkPath.empty() && linkPath.back() != L'\\') linkPath += L'\\';
        linkPath += kLinkName;

        IShellLinkW* psl = nullptr;
        hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&psl));
        if (SUCCEEDED(hr)) {
            psl->SetPath(exePath.c_str());          // 目标：CreateShortcut.exe
            psl->SetDescription(kLinkDesc);
            // 这里不需要设置 "%1"；SendTo 会自动把所选文件路径附加为参数
            // 如需自定义前缀参数，可： psl->SetArguments(L"--mode index");

            IPersistFile* ppf = nullptr;
            hr = psl->QueryInterface(IID_PPV_ARGS(&ppf));
            if (SUCCEEDED(hr)) {
                hr = ppf->Save(linkPath.c_str(), TRUE);
                ppf->Release();
                ok = SUCCEEDED(hr);
            }
            psl->Release();
        }
        CoTaskMemFree(sendto);
    }

    if (needUninit) CoUninitialize();
    return ok;
}

static bool DeleteSendToEntry(const std::wstring& shortcutName)
{
    HRESULT hr = CoInitialize(nullptr);
    bool needUninit = SUCCEEDED(hr);

    bool ok = false;
    PWSTR sendto = nullptr;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
        std::wstring linkPath(sendto);
        if (!linkPath.empty() && linkPath.back() != L'\\')
            linkPath += L'\\';
        linkPath += shortcutName; // 例如 L"CandyLauncher 索引文件夹.lnk"

        // 尝试删除文件
        if (DeleteFileW(linkPath.c_str())) {
            ok = true;
        } else {
            // 如果不存在也视为成功
            ok = (GetLastError() == ERROR_FILE_NOT_FOUND);
        }

        CoTaskMemFree(sendto);
    }

    if (needUninit)
        CoUninitialize();

    return ok;
}
