#pragma once

// 主程序侧启动 CandyUwpHelper.exe 并读取其输出（util/UwpHelperProtocol.hpp）。
// 被 UWP 索引（util/UWPAppsTraverser.hpp）和控制面板 / Windows 设置索引
// （plugins/Folder/SystemSettingsIndex.hpp）共用；助手不存在、超时或输出不合法时返回 false，由调用方回退到进程内枚举。

#include <windows.h>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "LogUtil.hpp"
#include "UwpHelperProtocol.hpp"

// 定位助手程序：先看本模块所在目录，再看上一级（插件 DLL 在 <app>\plugins\，助手与主程序同目录）
static std::wstring FindUwpHelperExe() {
	HMODULE module = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						reinterpret_cast<LPCWSTR>(&FindUwpHelperExe), &module);
	wchar_t modulePath[MAX_PATH]{};
	if (GetModuleFileNameW(module, modulePath, MAX_PATH) == 0) return L"";
	std::filesystem::path dir = std::filesystem::path(modulePath).parent_path();
	for (int level = 0; level < 2; ++level) {
		const std::filesystem::path candidate = dir / UwpHelper::kExeName;
		std::error_code ec;
		if (std::filesystem::is_regular_file(candidate, ec)) return candidate.wstring();
		if (!dir.has_parent_path() || dir.parent_path() == dir) break;
		dir = dir.parent_path();
	}
	return L"";
}

// 启动助手（mode 见 UwpHelper::kMode*）并读完它的 stdout。超时则结束进程并返回 false；退出码非 0 或输出解析失败也返回 false
static bool RunUwpHelper(const std::wstring& exePath, const wchar_t* mode, std::vector<UwpHelper::Item>& items,
						const DWORD timeoutMs) {
	SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
	HANDLE readEnd = nullptr, writeEnd = nullptr;
	if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) return false;
	// 读端不能被子进程继承，否则子进程退出后管道不会到 EOF
	SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = writeEnd;
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdError = INVALID_HANDLE_VALUE;
	PROCESS_INFORMATION pi{};
	std::wstring commandLine = L"\"" + exePath + L"\" " + mode;
	const BOOL created = CreateProcessW(exePath.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
										CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
	// 父进程必须关掉自己的写端，否则读端永远等不到 EOF
	CloseHandle(writeEnd);
	if (!created) {
		Loge(L"UwpHelper", L"CreateProcess failed for ", exePath, L" mode=", mode, L" error=", GetLastError());
		CloseHandle(readEnd);
		return false;
	}

	// 在独立线程里读到 EOF；主线程等待进程退出并负责超时时终止它（终止后管道断开，读线程随之返回）
	std::vector<uint8_t> output;
	std::thread reader([&]() {
		std::vector<uint8_t> chunk(64 * 1024);
		DWORD read = 0;
		while (ReadFile(readEnd, chunk.data(), static_cast<DWORD>(chunk.size()), &read, nullptr) && read > 0) {
			output.insert(output.end(), chunk.begin(), chunk.begin() + read);
		}
	});
	const DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
	if (waitResult != WAIT_OBJECT_0) {
		Loge(L"UwpHelper", L"mode=", mode, L" timed out after ", timeoutMs, L"ms, terminating");
		TerminateProcess(pi.hProcess, 1);
		WaitForSingleObject(pi.hProcess, 5000);
	}
	reader.join();
	DWORD exitCode = 1;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(readEnd);

	if (waitResult != WAIT_OBJECT_0 || exitCode != 0) {
		Loge(L"UwpHelper", L"mode=", mode, L" exited with code ", exitCode, L", output bytes=", output.size());
		return false;
	}
	if (!UwpHelper::Deserialize(output.data(), output.size(), items)) {
		Loge(L"UwpHelper", L"mode=", mode, L" output could not be parsed, bytes=", output.size());
		return false;
	}
	return true;
}
