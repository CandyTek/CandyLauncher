#pragma once

#include <windows.h>

#include <psapi.h>
#pragma comment(lib, "Psapi.lib")


/**
 * 检索系统运行中的窗口
 * @tparam Callback 
 * @param callback 
 */
template <typename Callback>
static void TraverseRunningWindows(
	Callback&& callback
) {
	auto enumProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
		if (!IsWindowVisible(hwnd)) return TRUE;

		DWORD pid;
		GetWindowThreadProcessId(hwnd, &pid);

		wchar_t title[256];
		GetWindowTextW(hwnd, title, sizeof(title) / sizeof(wchar_t));
		if (wcslen(title) == 0) return TRUE; // 跳过无标题窗口

		// 获取进程名
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		std::wstring procName;
		if (hProcess) {
			wchar_t exeName[MAX_PATH];
			if (GetModuleFileNameExW(hProcess, nullptr, exeName, MAX_PATH)) {
				procName = exeName;
			}
			CloseHandle(hProcess);
		}

		// 执行跳转窗口命令 run the window
		// 构造一个“命令”字符串，这里用 activate:<hwnd>
		// wchar_t buf[64];
		// swprintf(buf, 64, L"%p", hwnd);
		std::wstring command = L"";
		// std::wstring command = L"activate:";
		// command += buf;

		// 调用 callback
		auto& cb = *reinterpret_cast<Callback*>(lParam);
		cb(
			std::wstring(title), // 窗口标题作为逻辑名
			procName, // 所属进程路径
			std::to_wstring((uintptr_t)hwnd), // 窗口句柄字符串
			command // 激活窗口命令
		);

		return TRUE;
	};

	EnumWindows(enumProc, reinterpret_cast<LPARAM>(&callback));
}
