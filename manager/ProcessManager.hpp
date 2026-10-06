#pragma once

#include <cstdint>
#include <windows.h>
// #include "common/framework.h"
#include <string>
#include <shlguid.h>
#include <filesystem>

// #include "common/AppController.hpp"
// #include "manager/EditManager.hpp"

static bool g_restartRequested = false;

static UINT g_WM_SHOW_EXISTING_INSTANCE = 0;
static HANDLE g_instanceMutex = nullptr;

static std::wstring GetOwnExecutablePath() {
	std::wstring path(MAX_PATH, L'\0');
	for (;;) {
		const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (length == 0) return L"";
		if (length < path.size()) {
			path.resize(length);
			return path;
		}
		if (path.size() >= 32768) return L"";
		path.resize(path.size() * 2);
	}
}

static std::wstring GetInstanceMutexName(std::wstring path) {
	// Windows 文件路径不区分大小写；用完整路径区分不同目录下的副本。
	CharLowerBuffW(path.data(), static_cast<DWORD>(path.size()));
	uint64_t hash = 14695981039346656037ull;
	for (const wchar_t ch : path) {
		hash ^= static_cast<uint16_t>(ch);
		hash *= 1099511628211ull;
	}
	return L"Local\\CandyLauncher_" + std::to_wstring(hash);
}

struct ExistingInstanceSearch {
	const std::wstring& executablePath;
	HWND window = nullptr;
	DWORD processId = 0;
};

static BOOL CALLBACK FindExistingInstanceWindow(HWND window, LPARAM parameter) {
	auto& search = *reinterpret_cast<ExistingInstanceSearch*>(parameter);
	wchar_t className[64]{};
	if (GetClassNameW(window, className, 64) == 0 || wcscmp(className, L"CandyLauncherClass") != 0) return TRUE;

	DWORD processId = 0;
	GetWindowThreadProcessId(window, &processId);
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!process) return TRUE;
	std::wstring path(32768, L'\0');
	DWORD length = static_cast<DWORD>(path.size());
	const bool found = QueryFullProcessImageNameW(process, 0, path.data(), &length)
		&& CompareStringOrdinal(path.data(), length, search.executablePath.data(),
			static_cast<int>(search.executablePath.size()), TRUE) == CSTR_EQUAL;
	CloseHandle(process);
	if (!found) return TRUE;

	search.window = window;
	search.processId = processId;
	return FALSE;
}

static void ShowExistingInstance(const std::wstring& executablePath) {
	// 第二个进程可能在第一个进程创建窗口之前启动，稍等窗口完成注册。
	for (int attempt = 0; attempt < 100; ++attempt) {
		ExistingInstanceSearch search{executablePath};
		EnumWindows(FindExistingInstanceWindow, reinterpret_cast<LPARAM>(&search));
		if (search.window) {
			AllowSetForegroundWindow(search.processId);
			PostMessageW(search.window, g_WM_SHOW_EXISTING_INSTANCE, 0, 0);
			return;
		}
		Sleep(50);
	}
}
