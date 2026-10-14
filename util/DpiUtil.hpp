#pragma once

// DPI 适配：
// - 进程本身保持 DPI 不感知（manifest 不声明 dpiAware），Win7/8.1 以及尚未适配的插件窗口仍由系统整体拉伸。
// - 主窗口在支持的系统（Win10 1703+）上以 Per-Monitor V2 上下文创建，自己按所在显示器的 DPI 缩放皮肤：
//   所有尺寸在皮肤加载时按比例换算并烘焙进预乘 DIB，每帧绘制仍然只是 BitBlt，没有任何逐帧缩放开销。
// - 其余窗口（设置、皮肤编辑器、重命名对话框、索引管理器等插件窗口）用“简单方案”自行适配，见下方
//   “次级窗口的简单 DPI 方案”一节：创建时进入 Per-Monitor V2 作用域，布局常量和字体按 Scale(v, dpi) 换算，
//   WM_DPICHANGED 里按建议矩形移动窗口并等比缩放子控件或整体重建控件。
// - 所有 Win10 的 DPI API 都通过 GetProcAddress 动态加载，exe 不直接导入它们，Win7 仍能启动。
//
// 混合模式下线程的 DPI 上下文决定了坐标空间：系统在调用某个窗口的窗口过程时会临时切到该窗口的上下文，
// 所以主窗口消息里拿到的是物理像素坐标，而从 wWinMain、钩子回调或其他窗口里操作主窗口时必须显式进入
// 主窗口的上下文（ThreadContextScope + ContextOfWindow），否则 SetWindowPos/GetWindowRect 得到的是虚拟化后的坐标。

#include <windows.h>
#include <commctrl.h>

#ifndef DPI_AWARENESS_CONTEXT_UNAWARE
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_UNAWARE ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((DPI_AWARENESS_CONTEXT)-3)
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace Dpi {

constexpr UINT kBaseDpi = 96;

struct Api {
	DPI_AWARENESS_CONTEXT (WINAPI* setThreadContext)(DPI_AWARENESS_CONTEXT) = nullptr;
	DPI_AWARENESS_CONTEXT (WINAPI* getThreadContext)() = nullptr;
	DPI_AWARENESS_CONTEXT (WINAPI* getWindowContext)(HWND) = nullptr;
	BOOL (WINAPI* areContextsEqual)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT) = nullptr;
	UINT (WINAPI* getDpiForWindow)(HWND) = nullptr;
	UINT (WINAPI* getDpiForSystem)() = nullptr;
	int (WINAPI* getSystemMetricsForDpi)(int, UINT) = nullptr;
	// shcore.dll，Win8.1+；第二个参数是 MONITOR_DPI_TYPE，0 = MDT_EFFECTIVE_DPI
	HRESULT (WINAPI* getDpiForMonitor)(HMONITOR, int, UINT*, UINT*) = nullptr;
};

// 只加载一次；shcore 加载后不释放
inline const Api& api() {
	static const Api instance = [] {
		Api a;
		if (const HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
			a.setThreadContext = reinterpret_cast<decltype(a.setThreadContext)>(
				GetProcAddress(user32, "SetThreadDpiAwarenessContext"));
			a.getThreadContext = reinterpret_cast<decltype(a.getThreadContext)>(
				GetProcAddress(user32, "GetThreadDpiAwarenessContext"));
			a.getWindowContext = reinterpret_cast<decltype(a.getWindowContext)>(
				GetProcAddress(user32, "GetWindowDpiAwarenessContext"));
			a.areContextsEqual = reinterpret_cast<decltype(a.areContextsEqual)>(
				GetProcAddress(user32, "AreDpiAwarenessContextsEqual"));
			a.getDpiForWindow = reinterpret_cast<decltype(a.getDpiForWindow)>(GetProcAddress(user32, "GetDpiForWindow"));
			a.getDpiForSystem = reinterpret_cast<decltype(a.getDpiForSystem)>(GetProcAddress(user32, "GetDpiForSystem"));
			a.getSystemMetricsForDpi = reinterpret_cast<decltype(a.getSystemMetricsForDpi)>(
				GetProcAddress(user32, "GetSystemMetricsForDpi"));
		}
		if (const HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
			a.getDpiForMonitor = reinterpret_cast<decltype(a.getDpiForMonitor)>(GetProcAddress(shcore, "GetDpiForMonitor"));
		}
		return a;
	}();
	return instance;
}

// 窗口是否能以 Per-Monitor V2 创建（Win10 1703+）
inline bool PerMonitorV2Supported() {
	return api().setThreadContext != nullptr && api().getDpiForWindow != nullptr && api().getWindowContext != nullptr;
}

// 线程 DPI 上下文作用域：构造时切换，析构时恢复。context 为 nullptr 或系统不支持时什么都不做
class ThreadContextScope {
public:
	explicit ThreadContextScope(const DPI_AWARENESS_CONTEXT context) {
		if (context != nullptr && api().setThreadContext != nullptr) {
			previous_ = api().setThreadContext(context);
			applied_ = previous_ != nullptr;
		}
	}

	~ThreadContextScope() {
		if (applied_) api().setThreadContext(previous_);
	}

	ThreadContextScope(const ThreadContextScope&) = delete;
	ThreadContextScope& operator=(const ThreadContextScope&) = delete;

	bool applied() const { return applied_; }

private:
	DPI_AWARENESS_CONTEXT previous_ = nullptr;
	bool applied_ = false;
};

// 创建主窗口和自行适配 DPI 的次级窗口时使用；系统不支持时返回 nullptr，作用域不生效，窗口保持不感知（系统拉伸）
inline DPI_AWARENESS_CONTEXT PerMonitorV2Context() {
	return PerMonitorV2Supported() ? DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 : nullptr;
}

// 调用尚未自行缩放的插件代码（弹菜单、创建插件自己的窗口）时使用：
// 从主窗口的消息里（线程处于主窗口的 Per-Monitor 上下文）调用它们时，必须切回不感知，否则在高 DPI 显示器上会显示得很小。
// 已经自行适配的插件窗口会在自己的创建函数里再进入 PerMonitorV2Context 作用域
inline DPI_AWARENESS_CONTEXT UnawareContext() {
	return api().setThreadContext != nullptr ? DPI_AWARENESS_CONTEXT_UNAWARE : nullptr;
}

// 某个窗口自己的上下文：在它的坐标空间里定位、测量它时进入这个作用域
inline DPI_AWARENESS_CONTEXT ContextOfWindow(const HWND hwnd) {
	return hwnd != nullptr && api().getWindowContext != nullptr ? api().getWindowContext(hwnd) : nullptr;
}

// 当前线程是否处于不感知上下文（无 API 的系统等同于不感知）
inline bool IsThreadUnaware() {
	if (api().getThreadContext == nullptr || api().areContextsEqual == nullptr) return true;
	return api().areContextsEqual(api().getThreadContext(), DPI_AWARENESS_CONTEXT_UNAWARE) != FALSE;
}

// 窗口的 DPI：Per-Monitor 窗口返回所在显示器的 DPI，不感知窗口返回 96
inline UINT WindowDpi(const HWND hwnd) {
	const UINT dpi = hwnd != nullptr && api().getDpiForWindow != nullptr ? api().getDpiForWindow(hwnd) : 0;
	return dpi != 0 ? dpi : kBaseDpi;
}

// 显示器的 DPI。注意返回值取决于调用线程的上下文：不感知线程得到的永远是 96，所以要在 Per-Monitor 作用域里调用
inline UINT MonitorDpi(const HMONITOR monitor) {
	UINT dpiX = 0, dpiY = 0;
	if (monitor != nullptr && api().getDpiForMonitor != nullptr &&
		SUCCEEDED(api().getDpiForMonitor(monitor, 0, &dpiX, &dpiY)) && dpiX != 0) {
		return dpiX;
	}
	return kBaseDpi;
}

inline int Scale(const int value, const UINT dpi) {
	return dpi == kBaseDpi ? value : MulDiv(value, static_cast<int>(dpi), static_cast<int>(kBaseDpi));
}

inline float ScaleF(const float value, const UINT dpi) {
	return dpi == kBaseDpi ? value : value * static_cast<float>(dpi) / static_cast<float>(kBaseDpi);
}

inline double Factor(const UINT dpi) {
	return static_cast<double>(dpi) / static_cast<double>(kBaseDpi);
}

// 把 fromDpi 下的像素值换算到 toDpi
inline int Rescale(const int value, const UINT fromDpi, const UINT toDpi) {
	return fromDpi == toDpi || fromDpi == 0 ? value : MulDiv(value, static_cast<int>(toDpi), static_cast<int>(fromDpi));
}

inline int SystemMetricForDpi(const int index, const UINT dpi) {
	if (api().getSystemMetricsForDpi != nullptr) return api().getSystemMetricsForDpi(index, dpi);
	return GetSystemMetrics(index);
}

// 把 child 客户区里的一个点（child 自己坐标空间的像素）换算成当前线程坐标空间里的屏幕坐标。
// 用于从主窗口（Per-Monitor）向不感知上下文里的插件传递点击位置：GetWindowRect 在两个上下文里各取一次，
// 按比例换算，不依赖系统对跨上下文 ClientToScreen 的转换规则
inline POINT ChildClientPointToThreadScreen(const HWND child, const POINT clientPt) {
	RECT own{};
	{
		ThreadContextScope scope(ContextOfWindow(child));
		GetWindowRect(child, &own);
	}
	RECT mine{};
	GetWindowRect(child, &mine);
	const int ownWidth = own.right - own.left;
	const int ownHeight = own.bottom - own.top;
	POINT result{mine.left + clientPt.x, mine.top + clientPt.y};
	if (ownWidth > 0 && ownHeight > 0) {
		result.x = mine.left + MulDiv(clientPt.x, mine.right - mine.left, ownWidth);
		result.y = mine.top + MulDiv(clientPt.y, mine.bottom - mine.top, ownHeight);
	}
	return result;
}

// ---------- 次级窗口的简单 DPI 方案 ----------
// 设置窗口、皮肤编辑器、重命名对话框和插件自己的窗口不像主窗口那样烘焙皮肤，而是：
// 1. 创建函数里进入 ThreadContextScope(PerMonitorV2Context())，用 PrimaryMonitorDpi() / MonitorDpiForWindow(parent)
//    得到目标显示器 DPI，窗口初始尺寸按 Scale(v, dpi) 换算（此时 GetSystemMetrics(SM_CXSCREEN) 等也是物理像素）；
// 2. WM_CREATE 里用 WindowDpi(hwnd) 得到窗口 DPI，所有布局常量、字体高度（CreateScaledFont）经 Scale 换算；
// 3. WM_DPICHANGED 里：先 ApplySuggestedRect 移到系统建议的矩形，再重建字体并 SetFontForChildren，
//    子控件位置、尺寸、ListView 列宽用 RescaleChildWindows 按新旧 DPI 等比缩放；布局依赖 WM_SIZE 的窗口会自动重排。
//    控件很多、布局由代码生成的窗口（设置窗口）直接销毁全部子控件按新 DPI 重建更简单。

// 主显示器的 DPI（当前线程须处于 Per-Monitor 作用域，否则得到 96）
inline UINT PrimaryMonitorDpi() {
	const POINT origin{0, 0};
	return MonitorDpi(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY));
}

// 窗口所在显示器的 DPI；hwnd 为空时取主显示器
inline UINT MonitorDpiForWindow(const HWND hwnd) {
	if (hwnd == nullptr || !IsWindow(hwnd)) return PrimaryMonitorDpi();
	return MonitorDpi(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

// WM_DPICHANGED 的 wParam 高位是新 DPI
inline UINT DpiFromWParam(const WPARAM wParam) {
	const UINT dpi = HIWORD(wParam);
	return dpi != 0 ? dpi : kBaseDpi;
}

// WM_DPICHANGED 的 lParam 是系统建议的新窗口矩形（物理像素）
inline void ApplySuggestedRect(const HWND hwnd, const LPARAM lParam) {
	const RECT* rect = reinterpret_cast<const RECT*>(lParam);
	if (rect == nullptr) return;
	SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
}

// 按 96 DPI 下的字体高度（CreateFontW 的 nHeight 语义，负数为字符高度）创建缩放后的字体
inline HFONT CreateScaledFont(const int height96, const int weight, const wchar_t* face, const UINT dpi) {
	return CreateFontW(Scale(height96, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE,
					DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
					CLEARTYPE_QUALITY, VARIABLE_PITCH, face);
}

// 给 parent 的所有直接子窗口发 WM_SETFONT
inline void SetFontForChildren(const HWND parent, const HFONT font) {
	for (HWND child = GetWindow(parent, GW_CHILD); child != nullptr; child = GetWindow(child, GW_HWNDNEXT)) {
		SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	}
}

// 把 parent 的所有直接子窗口按 newDpi/oldDpi 等比缩放位置和尺寸；ListView 的列宽也一起缩放。
// 子控件的位置以 parent 客户区左上角为原点换算
inline void RescaleChildWindows(const HWND parent, const UINT oldDpi, const UINT newDpi) {
	if (oldDpi == newDpi || oldDpi == 0) return;
	for (HWND child = GetWindow(parent, GW_CHILD); child != nullptr; child = GetWindow(child, GW_HWNDNEXT)) {
		RECT rect{};
		GetWindowRect(child, &rect);
		MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&rect), 2);
		SetWindowPos(child, nullptr,
					Rescale(rect.left, oldDpi, newDpi), Rescale(rect.top, oldDpi, newDpi),
					Rescale(rect.right - rect.left, oldDpi, newDpi), Rescale(rect.bottom - rect.top, oldDpi, newDpi),
					SWP_NOZORDER | SWP_NOACTIVATE);

		wchar_t className[32] = {};
		GetClassNameW(child, className, 32);
		if (_wcsicmp(className, WC_LISTVIEWW) == 0) {
			const HWND header = ListView_GetHeader(child);
			const int columns = header != nullptr ? Header_GetItemCount(header) : 0;
			for (int i = 0; i < columns; ++i) {
				ListView_SetColumnWidth(child, i, Rescale(ListView_GetColumnWidth(child, i), oldDpi, newDpi));
			}
		}
	}
}

} // namespace Dpi
