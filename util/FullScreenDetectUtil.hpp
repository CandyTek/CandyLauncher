#pragma once

#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <string>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

// The shell reports exclusive Direct3D fullscreen, but borderless games usually
// need to be recognized from the foreground window instead.
static bool isExclusiveFullscreen() {
	QUERY_USER_NOTIFICATION_STATE state{};
	return SUCCEEDED(SHQueryUserNotificationState(&state))
		&& state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

static bool isFullscreenForegroundWindow(HWND hwnd, bool requireTopmost) {
	if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) return false;

	// A foreground child belongs to its root window. Do not use GA_ROOTOWNER:
	// owned popups can have an invisible or differently sized owner.
	hwnd = GetAncestor(hwnd, GA_ROOT);
	if (!hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;

	wchar_t className[128]{};
	if (GetClassNameW(hwnd, className, static_cast<int>(std::size(className)))) {
		if (lstrcmpW(className, L"Progman") == 0 ||
			lstrcmpW(className, L"WorkerW") == 0 ||
			lstrcmpW(className, L"Shell_TrayWnd") == 0) return false;
	}

	DWORD cloaked = 0;
	if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
		&& cloaked != 0) return false;

	const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
	const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	if ((style & WS_CHILD) || (exStyle & WS_EX_TOOLWINDOW)) return false;
	if (requireTopmost && !(exStyle & WS_EX_TOPMOST)) return false;

	MONITORINFO monitorInfo{};
	monitorInfo.cbSize = sizeof(monitorInfo);
	const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
	if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) return false;

	RECT windowRect{};
	if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
		&windowRect, sizeof(windowRect))) && !GetWindowRect(hwnd, &windowRect)) return false;

	const RECT& screen = monitorInfo.rcMonitor;
	constexpr int tolerance = 4;
	if (std::abs(windowRect.left - screen.left) > tolerance ||
		std::abs(windowRect.top - screen.top) > tolerance ||
		std::abs(windowRect.right - screen.right) > tolerance ||
		std::abs(windowRect.bottom - screen.bottom) > tolerance) return false;

	// A maximized normal window can cover the monitor when the taskbar auto hides.
	return !(style & WS_CAPTION) && !(style & WS_THICKFRAME);
}

static bool shouldShowInCurrentWindowMode(HWND hwnd) {
	return !isExclusiveFullscreen() && !isFullscreenForegroundWindow(hwnd, false);
}

static bool isLikelyFullscreenGame(HWND hwnd) {
	return isExclusiveFullscreen() || isFullscreenForegroundWindow(hwnd, true);
}

// Explicit rules cover windowed games, for which fullscreen detection cannot help.
static bool isForegroundProcessInList(HWND hwnd, const std::vector<std::string>& entries) {
	if (!hwnd || entries.empty()) return false;
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (!pid) return false;
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!process) return false;
	wchar_t path[32768]{};
	DWORD length = static_cast<DWORD>(std::size(path));
	const bool found = QueryFullProcessImageNameW(process, 0, path, &length) != 0;
	CloseHandle(process);
	if (!found) return false;

	std::wstring image(path, length);
	std::transform(image.begin(), image.end(), image.begin(), towlower);
	const std::wstring name = image.substr(image.find_last_of(L"\\/") + 1);
	for (const auto& entry : entries) {
		// Process names and paths are stored as UTF-8 in settings.json.
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			entry.c_str(), static_cast<int>(entry.size()), nullptr, 0);
		if (count <= 0) continue;
		std::wstring rule(count, L'\0');
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			entry.c_str(), static_cast<int>(entry.size()), rule.data(), count);
		const auto first = rule.find_first_not_of(L" \t\r\n\"");
		if (first == std::wstring::npos) continue;
		const auto last = rule.find_last_not_of(L" \t\r\n\"");
		rule = rule.substr(first, last - first + 1);
		std::replace(rule.begin(), rule.end(), L'/', L'\\');
		std::transform(rule.begin(), rule.end(), rule.begin(), towlower);
		if (rule == image || (rule.find(L'\\') == std::wstring::npos && rule == name))
			return true;
	}
	return false;
}
