#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <iostream>

// Helper: read a registry value (supports REG_EXPAND_SZ expansion)
// Returns empty string on failure.
static std::wstring ReadRegStringValue(HKEY hive, const std::wstring& subKey, const std::wstring& valueName, REGSAM viewFlag)
{
    HKEY hKey = nullptr;
    LONG res = RegOpenKeyExW(hive, subKey.c_str(), 0, KEY_READ | viewFlag, &hKey);
    if (res != ERROR_SUCCESS) return L"";

    DWORD type = 0;
    DWORD cb = 0;
    res = RegQueryValueExW(hKey, valueName.empty() ? nullptr : valueName.c_str(), nullptr, &type, nullptr, &cb);
    if (res != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return L"";
    }

    std::wstring buf;
    buf.resize(cb / sizeof(wchar_t)); // cb is bytes, include terminating NUL
    res = RegQueryValueExW(hKey, valueName.empty() ? nullptr : valueName.c_str(), nullptr, nullptr,
                          reinterpret_cast<LPBYTE>(&buf[0]), &cb);
    RegCloseKey(hKey);
    if (res != ERROR_SUCCESS) return L"";

    // remove trailing garbage / resize to actual string length
    size_t len = wcslen(buf.c_str());
    buf.resize(len);

    if (type == REG_EXPAND_SZ) {
        // expand environment strings
        DWORD needed = ExpandEnvironmentStringsW(buf.c_str(), nullptr, 0);
        if (needed) {
            std::wstring expanded;
            expanded.resize(needed);
            ExpandEnvironmentStringsW(buf.c_str(), &expanded[0], needed);
            // ExpandEnvironmentStrings returns length including null
            expanded.resize(wcslen(expanded.c_str()));
            return expanded;
        }
    }

    return buf;
}

static bool FileExistsAndIsFile(const std::wstring& path)
{
    if (path.empty()) return false;
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void AddIfFile(std::set<std::wstring>& setOut, const std::wstring& path)
{
    if (path.empty()) return;
    if (FileExistsAndIsFile(path)) setOut.insert(path);
}

// Try to append filename to dir safely
static std::wstring JoinPath(const std::wstring& dir, const std::wstring& file)
{
    if (dir.empty()) return file;
    std::wstring d = dir;
    if (d.back() != L'\\' && d.back() != L'/') d.push_back(L'\\');
    d += file;
    return d;
}

inline std::wstring FindOneNotePathEnhanced()
{
    std::set<std::wstring> found; // de-dupe
    const std::wstring exeName = L"ONENOTE.EXE";

    // 1) App Paths (HKCU/HKLM) with 64/32 views
    REGSAM views[] = { KEY_WOW64_64KEY, KEY_WOW64_32KEY, 0 };
    HKEY hives[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    const std::wstring appPathsSub = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\onenote.exe";

    for (REGSAM view : views) {
        for (HKEY hive : hives) {
            std::wstring val = ReadRegStringValue(hive, appPathsSub, L"", view);
            if (!val.empty()) AddIfFile(found, val);
        }
    }

    // If found in App Paths, return the first one
    if (!found.empty()) return *found.begin();

    // 2) Click-to-Run: HKLM\SOFTWARE\Microsoft\Office\ClickToRun\Configuration\ClientFolder
    // check both registry views
    for (REGSAM view : views) {
        std::wstring clientFolder = ReadRegStringValue(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Office\\ClickToRun\\Configuration", L"ClientFolder", view);
        if (!clientFolder.empty()) {
            // candidate patterns to try
            std::vector<std::wstring> patterns = {
                JoinPath(clientFolder, exeName),
                JoinPath(JoinPath(clientFolder, L"root\\Office16"), exeName),
                JoinPath(JoinPath(clientFolder, L"Office16"), exeName),
                JoinPath(JoinPath(clientFolder, L"root\\Office15"), exeName),
                JoinPath(JoinPath(clientFolder, L"Office15"), exeName)
            };
            for (auto &p : patterns) AddIfFile(found, p);
        }
    }

    if (!found.empty()) return *found.begin();

    // 3) Office InstallRoot registry keys (16.0 and 15.0), plus possible OneNote-specific InstallRoot
    const int vers[] = { 16, 15 }; // 16 -> Office2016/2019/365, 15 -> Office2013
    for (int v : vers) {
        // Common InstallRoot
        std::wstring commonPathKey = L"SOFTWARE\\Microsoft\\Office\\" + std::to_wstring(v) + L".0\\Common\\InstallRoot";
        for (REGSAM view : views) {
            std::wstring installPath = ReadRegStringValue(HKEY_LOCAL_MACHINE, commonPathKey, L"Path", view);
            if (!installPath.empty()) {
                // typical install root might point to "C:\Program Files\Microsoft Office\Office16\" already
                // try both direct and with exe appended
                AddIfFile(found, JoinPath(installPath, exeName));
                AddIfFile(found, JoinPath(JoinPath(installPath, L"root\\Office" + std::to_wstring(v)), exeName));
                AddIfFile(found, JoinPath(JoinPath(installPath, L"Office" + std::to_wstring(v)), exeName));
            }
        }

        // OneNote-specific InstallRoot (sometimes)
        std::wstring oneNoteKey = L"SOFTWARE\\Microsoft\\Office\\" + std::to_wstring(v) + L".0\\OneNote\\InstallRoot";
        for (REGSAM view : views) {
            std::wstring onenoteInstall = ReadRegStringValue(HKEY_LOCAL_MACHINE, oneNoteKey, L"Path", view);
            if (!onenoteInstall.empty()) {
                AddIfFile(found, JoinPath(onenoteInstall, exeName));
            }
        }
    }

    if (!found.empty()) return *found.begin();

    // 4) Common default file system locations (Program Files / Program Files (x86))
    // Try lots of typical paths used by MSI and Click-to-Run installs
    std::vector<std::wstring> programRoots;
    // read environment for ProgramFiles and ProgramFiles(x86)
    wchar_t buf[4096] = {0};
    if (GetEnvironmentVariableW(L"ProgramFiles", buf, _countof(buf))) programRoots.emplace_back(buf);
    if (GetEnvironmentVariableW(L"ProgramFiles(x86)", buf, _countof(buf))) programRoots.emplace_back(buf);
    // also try hard-coded fallbacks
    programRoots.emplace_back(L"C:\\Program Files");
    programRoots.emplace_back(L"C:\\Program Files (x86)");

    // candidate relative paths
    std::vector<std::wstring> relPaths = {
        L"Microsoft Office\\Office15",
        L"Microsoft Office\\Office16",
        L"Microsoft Office\\root\\Office16",
        L"Microsoft Office 15\\Office15",
        L"Microsoft Office 16\\Office16",
        L"Microsoft Office\\Office14" // just in case (older)
    };

    for (auto &root : programRoots) {
        for (auto &rel : relPaths) {
            std::wstring full = JoinPath(root, rel);
            AddIfFile(found, JoinPath(full, exeName));
        }
    }

    if (!found.empty()) return *found.begin();

    // 5) last resort: search PATH environment for onenote.exe (fast)
    if (GetEnvironmentVariableW(L"PATH", buf, _countof(buf))) {
        std::wstring pathEnv(buf);
        size_t pos = 0;
        while (pos < pathEnv.size()) {
            size_t next = pathEnv.find(L';', pos);
            if (next == std::wstring::npos) next = pathEnv.size();
            std::wstring chunk = pathEnv.substr(pos, next - pos);
            if (!chunk.empty()) AddIfFile(found, JoinPath(chunk, exeName));
            pos = next + 1;
        }
    }

    if (!found.empty()) return *found.begin();

    // not found
    return L"";
}

