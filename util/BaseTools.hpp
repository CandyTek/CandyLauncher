// 此类放置程序所有通用基础方法，本hpp不要引用其他抽象文件，只引用通用工具

#pragma once

#include <algorithm>
#include <chrono>
#include <windows.h>
#include <string>
#include <sstream>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <ShlGuid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "../model/TraverseOptions.hpp"
#include "NinePatchImage.hpp"
#include <array>

#include "LogUtil.hpp"
#include "StringUtil.hpp"


static std::wstring GetClipboardText() {
	// 尝试打开剪贴板
	if (!OpenClipboard(nullptr)) {
		return nullptr;
	}

	// 获取 Unicode 文本格式的剪贴板数据
	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == nullptr) {
		CloseClipboard();
		return nullptr;
	}

	// 锁定内存句柄以获取实际数据指针
	LPCWSTR pszText = static_cast<LPCWSTR>(GlobalLock(hData));
	if (pszText == nullptr) {
		CloseClipboard();
		return nullptr;
	}

	// 复制数据到 std::wstring
	std::wstring text(pszText);

	// 解锁全局内存
	GlobalUnlock(hData);

	// 关闭剪贴板
	CloseClipboard();

	return text;
}

inline bool CopyTextToClipboard(HWND hWnd, const std::wstring& text) {
	if (!OpenClipboard(hWnd)) return false;
	if (!EmptyClipboard()) {
		CloseClipboard();
		return false;
	}

	size_t bytes = (text.size() + 1) * sizeof(wchar_t);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (!hMem) {
		CloseClipboard();
		return false;
	}

	void* p = GlobalLock(hMem);
	memcpy(p, text.c_str(), bytes);
	GlobalUnlock(hMem);
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard(); // hMem 的释放权交给剪贴板
	return true;
}

// 辅助函数：执行命令行并获取其标准输出
static std::string ExecuteCommandAndGetOutput(const std::wstring& command) {
	HANDLE hChildStd_OUT_Rd = NULL;
	HANDLE hChildStd_OUT_Wr = NULL;
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// 创建一个管道用于子进程的 STDOUT
	if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &sa, 0)) {
		throw std::runtime_error("StdoutRd CreatePipe failed");
	}
	if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
		throw std::runtime_error("Stdout SetHandleInformation failed");
	}

	PROCESS_INFORMATION piProcInfo;
	STARTUPINFOW siStartInfo;
	BOOL bSuccess = FALSE;

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
	siStartInfo.cb = sizeof(STARTUPINFOW);
	siStartInfo.hStdError = hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = hChildStd_OUT_Wr;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	std::wstring cmd = command;
	std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
	cmdline.push_back(L'\0');

	bSuccess = CreateProcessW(NULL,
							cmdline.data(), // command line 
							NULL, // process security attributes 
							NULL, // primary thread security attributes 
							TRUE, // handles are inherited 
							CREATE_NO_WINDOW, // creation flags: 不显示命令行窗口
							NULL, // use parent's environment 
							NULL, // use parent's starting directory 
							&siStartInfo, // STARTUPINFO pointer 
							&piProcInfo); // receives PROCESS_INFORMATION 

	if (!bSuccess) {
		CloseHandle(hChildStd_OUT_Rd);
		CloseHandle(hChildStd_OUT_Wr);
		throw std::runtime_error("CreateProcess failed");
	}

	// 必须关闭写管道句柄，否则 ReadFile 会一直阻塞等待
	CloseHandle(hChildStd_OUT_Wr);

	std::string result;
	DWORD dwRead;
	std::array<char, 4096> buffer;

	// 从管道读取子进程的输出
	for (;;) {
		bSuccess = ReadFile(hChildStd_OUT_Rd, buffer.data(), static_cast<DWORD>(buffer.size()), &dwRead, NULL);
		if (!bSuccess || dwRead == 0) break;
		result.append(buffer.data(), dwRead);
	}

	// 清理
	CloseHandle(hChildStd_OUT_Rd);
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	return result;
}

static std::wstring ExpandEnvironmentVariables(const std::wstring& path, const std::wstring& exeFolderPath) {
	if (path.empty()) return path;

	std::wstring processedPath = path;

	// Handle paths starting with backslash by prepending
	if (path.size() > 0 && path[0] == L'\\') {
		processedPath = exeFolderPath + path;
	}

	DWORD size = ExpandEnvironmentStringsW(processedPath.c_str(), nullptr, 0);
	if (size == 0) return processedPath;

	std::wstring expanded(size - 1, 0);
	DWORD result = ExpandEnvironmentStringsW(processedPath.c_str(), &expanded[0], size);
	if (result == 0) return processedPath;

	return expanded;
}

// 检查编辑框是否为空
static bool IsEditControlsEmpty(const std::vector<HWND>& edits) {
	wchar_t buffer[4096];

	for (HWND hEdit : edits) {
		if (!hEdit) continue; // 防止传入空句柄
		GetWindowTextW(hEdit, buffer, sizeof(buffer) / sizeof(wchar_t));
		if (wcslen(buffer) == 0) {
			return true;
		}
	}

	return false;
}

// 从最小化恢复
inline void RestoreWindowIfMinimized(HWND hWnd) {
	if (IsIconic(hWnd)) {
		ShowWindow(hWnd, SW_RESTORE);
	}
}

// 显示指定窗口
inline void showCurrectWindowSimple(HWND hwnd) {
	RestoreWindowIfMinimized(hwnd);
	SetForegroundWindow(hwnd);
}

inline bool IsWindows8OrGreater() {
	OSVERSIONINFOEXW osvi = {sizeof(osvi)};
	osvi.dwMajorVersion = 6;
	osvi.dwMinorVersion = 2; // Windows 8 是 6.2

	DWORDLONG const dwlConditionMask =
		VerSetConditionMask(
			VerSetConditionMask(
				0, VER_MAJORVERSION, VER_GREATER_EQUAL),
			VER_MINORVERSION, VER_GREATER_EQUAL);

	return VerifyVersionInfoW(
		&osvi,
		VER_MAJORVERSION | VER_MINORVERSION,
		dwlConditionMask
	) != FALSE;
}
