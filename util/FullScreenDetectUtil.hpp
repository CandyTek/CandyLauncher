#include <windows.h>
#include <dwmapi.h>
#include <string>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

static bool isShellLikeWindow(HWND hwnd) {
    if (!hwnd) return true;

    char className[256] = {};
    if (GetClassNameA(hwnd, className, sizeof(className)) <= 0) {
        return false;
    }

    const std::string cls(className);
    return cls == "Progman" ||
           cls == "WorkerW" ||
           cls == "Shell_TrayWnd";
}

static bool isCloakedWindow(HWND hwnd) {
    BOOL cloaked = FALSE;
    const HRESULT hr = DwmGetWindowAttribute(
        hwnd,
        DWMWA_CLOAKED,
        &cloaked,
        sizeof(cloaked)
    );
    return SUCCEEDED(hr) && cloaked;
}

static RECT getBestWindowRect(HWND hwnd) {
    RECT rc = {};
    // 优先取 DWM 可见边界
    if (SUCCEEDED(DwmGetWindowAttribute(
            hwnd,
            DWMWA_EXTENDED_FRAME_BOUNDS,
            &rc,
            sizeof(rc)))) {
        return rc;
    }

    // 回退到 GetWindowRect
    GetWindowRect(hwnd, &rc);
    return rc;
}

static bool rectNearlyEquals(const RECT& a, const RECT& b, int tolerance = 2) {
    return std::abs(a.left   - b.left)   <= tolerance &&
           std::abs(a.top    - b.top)    <= tolerance &&
           std::abs(a.right  - b.right)  <= tolerance &&
           std::abs(a.bottom - b.bottom) <= tolerance;
}


static bool isLikelyBorderlessStyle(LONG_PTR style, LONG_PTR exStyle) {
    // 无标题栏 / 无 thickframe / 无常规边框，更像 borderless fullscreen
    const bool noCaption    = (style & WS_CAPTION) == 0;
    const bool noThickFrame = (style & WS_THICKFRAME) == 0;
    const bool noBorder     = (style & WS_BORDER) == 0;
    const bool noDlgFrame   = (style & WS_DLGFRAME) == 0;
    const bool toolWindow   = (exStyle & WS_EX_TOOLWINDOW) != 0;

    // tool window 常见于浮动窗/辅助窗，不当作目标全屏窗口
    if (toolWindow) return false;

    // 放宽一些，不要求所有条件都满足
    int score = 0;
    if (noCaption)    ++score;
    if (noThickFrame) ++score;
    if (noBorder)     ++score;
    if (noDlgFrame)   ++score;

    return score >= 3;
}

static bool shouldShowInCurrentWindowMode(HWND hwnd) {
	if (hwnd == nullptr) {
		return true;
	}
	// 获取窗口样式
	const LONG style = GetWindowLong(hwnd, GWL_STYLE);
	const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (style & WS_CAPTION || style & WS_THICKFRAME || exStyle & WS_EX_WINDOWEDGE) {
		// 如果有这些样式，基本可以确定不是全屏
		return true;
	}

	// 获取窗口所在屏幕
	HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);
	RECT screenRect = monitorInfo.rcMonitor;

	// 获取窗口位置
	RECT windowRect;
	// 获取失败，默认窗口模式
	if (!GetWindowRect(hwnd, &windowRect)) return true;

	// 检查是否为无边框全屏
	if ((style & WS_BORDER) == 0 &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom) {
		// 检查是否是桌面（类名为 "Progman"）
		char className[256];
		GetClassNameA(hwnd, className, sizeof(className));
		if (std::string(className) == "Progman") {
			return true;
		}
		return false;
	}

	// 检查是否全屏（隐藏任务栏等）
	if ((style & WS_CAPTION) == 0 &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom) {
		return false;
	}

	return true;
}

[[deprecated(L"对游戏判断不太准确")]]
static bool shouldShowInCurrentWindowTopmostMode(HWND hwnd) {
	if (hwnd == nullptr) {
		return true;
	}
	// 获取窗口样式
	const LONG style = GetWindowLong(hwnd, GWL_STYLE);
	const LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (style & WS_CAPTION || style & WS_THICKFRAME || exStyle & WS_EX_WINDOWEDGE) {
		// 如果有这些样式，基本可以确定不是全屏
		return true;
	}

	// 获取窗口所在屏幕
	HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO monitorInfo = {};
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);
	RECT screenRect = monitorInfo.rcMonitor;

	// 获取窗口位置
	RECT windowRect;
	// 获取失败，默认窗口模式
	if (!GetWindowRect(hwnd, &windowRect)) return true;

	// 检查是否为无边框全屏
	if (exStyle & WS_EX_TOPMOST &&
		windowRect.left == screenRect.left &&
		windowRect.top == screenRect.top &&
		windowRect.right == screenRect.right &&
		windowRect.bottom == screenRect.bottom) {
		return false;
	}

	return true;
}

static bool LooksBorderlessFullscreenStyle(LONG_PTR style, LONG_PTR exStyle) {
    if ((style & WS_CHILD) != 0) {
        return false;
    }
    if ((exStyle & WS_EX_TOOLWINDOW) != 0) {
        return false;
    }

    int score = 0;
    if ((style & WS_CAPTION) == 0)    ++score;
    if ((style & WS_THICKFRAME) == 0) ++score;
    if ((style & WS_BORDER) == 0)     ++score;
    if ((style & WS_DLGFRAME) == 0)   ++score;

    // 3/4 即可，别太苛刻
    return score >= 3;
}

static std::wstring GetProcessImagePathFromHwnd(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return {};
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return {};
    }

    wchar_t buf[1024] = {};
    DWORD len = static_cast<DWORD>(std::size(buf));
    std::wstring result;
    if (QueryFullProcessImageNameW(hProcess, 0, buf, &len)) {
        result.assign(buf, len);
    }
    CloseHandle(hProcess);
    return result;
}

static std::wstring ToLowerCopy(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return s;
}

static bool IsKnownNonGameProcess(const std::wstring& imagePath) {
    if (imagePath.empty()) return false;

    const std::wstring p = ToLowerCopy(imagePath);

    // 这里只放少量高频误报项，别放太多
    return p.find(L"\\explorer.exe") != std::wstring::npos ||
           p.find(L"\\applicationframehost.exe") != std::wstring::npos ||
           p.find(L"\\dwm.exe") != std::wstring::npos ||
           p.find(L"\\searchhost.exe") != std::wstring::npos;
}

static bool isForegroundFullscreenGameLike(HWND hwnd) {
    // 第一层：系统状态
    QUERY_USER_NOTIFICATION_STATE st{};
    if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
        if (st == QUNS_RUNNING_D3D_FULL_SCREEN) {
            return true;
        }
        // QUNS_BUSY 继续走窗口启发式
    }

    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return false;
    }

    // 统一到根拥有者，避免吃到 owned popup/临时窗
    hwnd = GetAncestor(hwnd, GA_ROOTOWNER);
    if (!hwnd) {
        return false;
    }

    if (isShellLikeWindow(hwnd) || isCloakedWindow(hwnd)) {
        return false;
    }

    const LONG_PTR style   = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    if ((style & WS_MINIMIZE) != 0) {
        return false;
    }

    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!hMon) {
        return false;
    }

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        return false;
    }

    const RECT monitorRect = mi.rcMonitor;
    const RECT windowRect  = getBestWindowRect(hwnd);

    // 没铺满监视器，直接不是全屏游戏态
    if (!rectNearlyEquals(windowRect, monitorRect, 2)) {
        return false;
    }

    // 轻量排除一些高频非游戏进程
    const std::wstring imagePath = GetProcessImagePathFromHwnd(hwnd);
    if (IsKnownNonGameProcess(imagePath)) {
        return false;
    }

    // topmost 是强信号，但不是必要条件
    const bool isTopmost = (exStyle & WS_EX_TOPMOST) != 0;
    const bool borderlessLike = LooksBorderlessFullscreenStyle(style, exStyle);

    // 判定策略：
    // 1) 独占全屏已在前面返回 true
    // 2) 这里命中“铺满屏幕 + topmost”
    // 3) 或者“铺满屏幕 + 很像无边框全屏窗口”
    if (isTopmost) {
        return true;
    }

    if (borderlessLike) {
        return true;
    }

    return false;
}

#pragma comment(lib, "psapi.lib")

struct ForegroundProcessMemoryInfo {
	DWORD pid = 0;
	SIZE_T workingSetBytes = 0;   // 当前工作集
	SIZE_T privateUsageBytes = 0; // Private Bytes / Commit Charge
};

static std::optional<ForegroundProcessMemoryInfo> GetForegroundProcessMemoryInfo() {
	HWND hwnd = GetForegroundWindow();
	if (!hwnd) {
		return std::nullopt;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == 0) {
		return std::nullopt;
	}

	// 对 GetProcessMemoryInfo，通常 QUERY_LIMITED_INFORMATION 不够稳；
	// 更常见是 PROCESS_QUERY_INFORMATION | PROCESS_VM_READ。
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!hProcess) {
		return std::nullopt;
	}

	PROCESS_MEMORY_COUNTERS_EX pmc{};
	pmc.cb = sizeof(pmc);

	if (!GetProcessMemoryInfo(
			hProcess,
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
			sizeof(pmc))) {
		CloseHandle(hProcess);
		return std::nullopt;
			}

	CloseHandle(hProcess);

	ForegroundProcessMemoryInfo info;
	info.pid = pid;
	info.workingSetBytes = pmc.WorkingSetSize;
	info.privateUsageBytes = pmc.PrivateUsage;
	return info;
}

static bool IsForegroundProcessUsingLargeMemory(SIZE_T thresholdMB = 1536) {
	auto info = GetForegroundProcessMemoryInfo();
	if (!info) {
		return false;
	}

	const SIZE_T thresholdBytes = thresholdMB * 1024ull * 1024ull;
	return info->privateUsageBytes >= thresholdBytes;
}

static bool isLikelyFullscreenGame(HWND hwnd) {
	if (!isForegroundFullscreenGameLike(hwnd)) {
		return false;
	}

	auto mem = GetForegroundProcessMemoryInfo();
	if (!mem) {
		// 拿不到内存时，仍然保留前台全屏判定
		return true;
	}

	// 内存高，进一步增强“像游戏”的置信度
	return mem->privateUsageBytes >= 1024ull * 1024ull * 1024ull; // 1 GB
}
