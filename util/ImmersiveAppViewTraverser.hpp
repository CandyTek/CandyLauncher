#pragma once

#include <windows.h>
#include <inspectable.h>

#include <objectarray.h>
#include <psapi.h>
#include <servprov.h>
#include <wrl/client.h>

#include <cwctype>
#include <functional>
#include <string>

#pragma comment(lib, "Psapi.lib")

enum APPLICATION_VIEW_CLOAK_TYPE {
	AVCT_NONE = 0,
	AVCT_DEFAULT = 1,
	AVCT_VIRTUAL_DESKTOP = 2
};

enum APPLICATION_VIEW_COMPATIBILITY_POLICY {
	AVCP_NONE = 0,
	AVCP_SMALL_SCREEN = 1,
	AVCP_TABLET_SMALL_SCREEN = 2,
	AVCP_VERY_SMALL_SCREEN = 3,
	AVCP_HIGH_SCALE_FACTOR = 4
};

struct IAsyncCallback;
struct IImmersiveApplication;
struct IImmersiveMonitor;
struct IApplicationViewPosition;
struct IApplicationViewOperation;
struct IShellPositionerPriority;
struct IApplicationViewChangeListener;

struct IApplicationViewLegacy : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE SetFocus() = 0;
	virtual HRESULT STDMETHODCALLTYPE SwitchTo() = 0;
	virtual HRESULT STDMETHODCALLTYPE TryInvokeBack(IAsyncCallback*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetThumbnailWindow(HWND*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetMonitor(IImmersiveMonitor**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetVisibility(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCloak(APPLICATION_VIEW_CLOAK_TYPE, int) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPosition(REFIID, void**) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPosition(IApplicationViewPosition*) = 0;
	virtual HRESULT STDMETHODCALLTYPE InsertAfterWindow(HWND) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetExtendedFramePosition(RECT*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAppUserModelId(PWSTR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetAppUserModelId(PCWSTR) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsEqualByAppUserModelId(PCWSTR, int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewState(UINT*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetViewState(UINT) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetNeediness(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetLastActivationTimestamp(ULONGLONG*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetLastActivationTimestamp(ULONGLONG) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetVirtualDesktopId(GUID*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetVirtualDesktopId(REFGUID) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetShowInSwitchers(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetShowInSwitchers(int) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetScaleFactor(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE CanReceiveInput(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCompatibilityPolicyType(APPLICATION_VIEW_COMPATIBILITY_POLICY*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCompatibilityPolicyType(APPLICATION_VIEW_COMPATIBILITY_POLICY) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPositionPriority(IShellPositionerPriority**) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPositionPriority(IShellPositionerPriority*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSizeConstraints(IImmersiveMonitor*, SIZE*, SIZE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSizeConstraintsForDpi(UINT, SIZE*, SIZE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetSizeConstraintsForDpi(const UINT*, const SIZE*, const SIZE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE QuerySizeConstraintsFromApp() = 0;
	virtual HRESULT STDMETHODCALLTYPE OnMinSizePreferencesUpdated(HWND) = 0;
	virtual HRESULT STDMETHODCALLTYPE ApplyOperation(IApplicationViewOperation*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsTray(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsInHighZOrderBand(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsSplashScreenPresented(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Flash() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetRootSwitchableOwner(IApplicationViewLegacy**) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumerateOwnershipTree(IObjectArray**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetEnterpriseId(PWSTR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetEnterpriseChromePreference(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsMirrored(BOOL*) = 0;
};

struct IApplicationViewModern : public IInspectable {
	virtual HRESULT STDMETHODCALLTYPE SetFocus() = 0;
	virtual HRESULT STDMETHODCALLTYPE SwitchTo() = 0;
	virtual HRESULT STDMETHODCALLTYPE TryInvokeBack(IAsyncCallback*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetThumbnailWindow(HWND*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetMonitor(IImmersiveMonitor**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetVisibility(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCloak(APPLICATION_VIEW_CLOAK_TYPE, int) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPosition(REFIID, void**) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPosition(IApplicationViewPosition*) = 0;
	virtual HRESULT STDMETHODCALLTYPE InsertAfterWindow(HWND) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetExtendedFramePosition(RECT*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAppUserModelId(PWSTR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetAppUserModelId(PCWSTR) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsEqualByAppUserModelId(PCWSTR, int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewState(UINT*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetViewState(UINT) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetNeediness(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetLastActivationTimestamp(ULONGLONG*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetLastActivationTimestamp(ULONGLONG) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetVirtualDesktopId(GUID*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetVirtualDesktopId(REFGUID) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetShowInSwitchers(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetShowInSwitchers(int) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetScaleFactor(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE CanReceiveInput(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCompatibilityPolicyType(APPLICATION_VIEW_COMPATIBILITY_POLICY*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCompatibilityPolicyType(APPLICATION_VIEW_COMPATIBILITY_POLICY) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSizeConstraints(IImmersiveMonitor*, SIZE*, SIZE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSizeConstraintsForDpi(UINT, SIZE*, SIZE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetSizeConstraintsForDpi(const UINT*, const SIZE*, const SIZE*) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnMinSizePreferencesUpdated(HWND) = 0;
	virtual HRESULT STDMETHODCALLTYPE ApplyOperation(IApplicationViewOperation*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsTray(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsInHighZOrderBand(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsSplashScreenPresented(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Flash() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetRootSwitchableOwner(IApplicationViewModern**) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumerateOwnershipTree(IObjectArray**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetEnterpriseId(PWSTR*) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsMirrored(BOOL*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown1(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown2(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown3(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown4(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown5(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown6(int) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown7() = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown8(int*) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown9(int) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown10(int, int) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown11(int) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unknown12(SIZE*) = 0;
};

struct IApplicationViewCollection : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetViews(IObjectArray**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewsByZOrder(IObjectArray**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewsByAppUserModelId(PCWSTR, IObjectArray**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewForHwnd(HWND, IUnknown**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewForApplication(IImmersiveApplication*, IUnknown**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewForAppUserModelId(PCWSTR, IUnknown**) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetViewInFocus(IUnknown**) = 0;
	virtual HRESULT STDMETHODCALLTYPE RefreshCollection() = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterForApplicationViewChanges(IApplicationViewChangeListener*, DWORD*) = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterForApplicationViewPositionChanges(IApplicationViewChangeListener*, DWORD*) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnregisterForApplicationViewChanges(DWORD) = 0;
};

struct ImmersiveAppViewInfo {
	std::wstring title;
	std::wstring processPath;
	std::wstring appUserModelId;
	HWND thumbnailHwnd = nullptr;
	Microsoft::WRL::ComPtr<IUnknown> view;
	bool isModernView = false;
};

inline DWORD GetWindowsBuildNumber()
{
	using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
	const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll) return 0;

	const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
	if (!rtlGetVersion) return 0;

	RTL_OSVERSIONINFOW osInfo = {};
	osInfo.dwOSVersionInfoSize = sizeof(osInfo);
	if (rtlGetVersion(&osInfo) != 0) return 0;
	return osInfo.dwBuildNumber;
}

inline bool UseModernApplicationViewApi()
{
	const DWORD build = GetWindowsBuildNumber();
	return build > 17134;
}

inline GUID GetApplicationViewCollectionGuid()
{
	if (UseModernApplicationViewApi()) {
		return { 0x1841C6D7, 0x4F9D, 0x42C0, { 0xAF, 0x41, 0x87, 0x47, 0x53, 0x8F, 0x10, 0xE5 } };
	}
	return { 0x2C08ADF0, 0xA386, 0x4B35, { 0x92, 0x50, 0x0F, 0xE1, 0x83, 0x47, 0x6F, 0xCC } };
}

inline GUID GetApplicationViewGuid()
{
	if (UseModernApplicationViewApi()) {
		return { 0x372E1D3B, 0x38D3, 0x42E4, { 0xA1, 0x5B, 0x8A, 0xB2, 0xB1, 0x78, 0xF5, 0x13 } };
	}
	return { 0x9AC0B5C8, 0x1484, 0x4C5B, { 0x95, 0x33, 0x41, 0x34, 0xA0, 0xF9, 0x7C, 0xEA } };
}

inline std::wstring GetProcessPathFromWindowHandle(HWND hwnd) {
	if (!hwnd) return L"";

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == 0) return L"";

	std::wstring processPath;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!hProcess) return processPath;

	wchar_t exeName[MAX_PATH] = {};
	if (GetModuleFileNameExW(hProcess, nullptr, exeName, MAX_PATH)) {
		processPath = exeName;
	}
	CloseHandle(hProcess);
	return processPath;
}

inline std::wstring GetWindowTitleFromHandle(HWND hwnd) {
	if (!hwnd) return L"";
	const int len = GetWindowTextLengthW(hwnd);
	if (len <= 0) return L"";

	std::wstring title(static_cast<size_t>(len) + 1, L'\0');
	GetWindowTextW(hwnd, title.data(), len + 1);
	title.resize(static_cast<size_t>(len));
	return title;
}

inline std::wstring ToLowerCopy(std::wstring value) {
	for (wchar_t& ch : value) {
		ch = static_cast<wchar_t>(towlower(ch));
	}
	return value;
}

inline std::wstring GetFileNamePart(const std::wstring& path) {
	const size_t pos = path.find_last_of(L"\\/");
	return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

inline bool IsEdgeProcessPath(const std::wstring& processPath) {
	return ToLowerCopy(GetFileNamePart(processPath)) == L"msedge.exe";
}

inline HRESULT GetApplicationViewThumbnailWindow(IUnknown* view, bool isModernView, HWND* hwnd)
{
	if (!view || !hwnd) return E_POINTER;
	if (isModernView) {
		return reinterpret_cast<IApplicationViewModern*>(view)->GetThumbnailWindow(hwnd);
	}
	return reinterpret_cast<IApplicationViewLegacy*>(view)->GetThumbnailWindow(hwnd);
}

inline HRESULT GetApplicationViewShowInSwitchers(IUnknown* view, bool isModernView, int* showInSwitchers)
{
	if (!view || !showInSwitchers) return E_POINTER;
	if (isModernView) {
		return reinterpret_cast<IApplicationViewModern*>(view)->GetShowInSwitchers(showInSwitchers);
	}
	return reinterpret_cast<IApplicationViewLegacy*>(view)->GetShowInSwitchers(showInSwitchers);
}

inline HRESULT GetApplicationViewAppUserModelId(IUnknown* view, bool isModernView, PWSTR* appId)
{
	if (!view || !appId) return E_POINTER;
	if (isModernView) {
		return reinterpret_cast<IApplicationViewModern*>(view)->GetAppUserModelId(appId);
	}
	return reinterpret_cast<IApplicationViewLegacy*>(view)->GetAppUserModelId(appId);
}

inline HRESULT SwitchToApplicationView(IUnknown* view, bool isModernView)
{
	if (!view) return E_POINTER;
	if (isModernView) {
		return reinterpret_cast<IApplicationViewModern*>(view)->SwitchTo();
	}
	return reinterpret_cast<IApplicationViewLegacy*>(view)->SwitchTo();
}

template <typename Callback>
static void TraverseImmersiveApplicationViews(Callback&& callback) {
	const CLSID clsidImmersiveShell = {0xC2F03A33, 0x21F5, 0x47FA, {0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39}};
	const GUID applicationViewCollectionGuid = GetApplicationViewCollectionGuid();
	const GUID applicationViewGuid = GetApplicationViewGuid();
	const bool isModernView = UseModernApplicationViewApi();

	const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	const bool shouldUninitialize = (initHr == S_OK || initHr == S_FALSE);
	if (FAILED(initHr)) {
		return;
	}

	Microsoft::WRL::ComPtr<IServiceProvider> serviceProvider;
	HRESULT hr = CoCreateInstance(
		clsidImmersiveShell,
		nullptr,
		CLSCTX_LOCAL_SERVER,
		IID_PPV_ARGS(&serviceProvider));
	if (FAILED(hr)) {
		if (shouldUninitialize) {
			CoUninitialize();
		}
		return;
	}

	Microsoft::WRL::ComPtr<IApplicationViewCollection> viewCollection;
	hr = serviceProvider->QueryService(
		applicationViewCollectionGuid,
		applicationViewCollectionGuid,
		reinterpret_cast<void**>(viewCollection.GetAddressOf()));
	if (FAILED(hr) || !viewCollection) {
		if (shouldUninitialize) {
			CoUninitialize();
		}
		return;
	}

	Microsoft::WRL::ComPtr<IObjectArray> views;
	hr = viewCollection->GetViews(&views);
	if (FAILED(hr) || !views) {
		if (shouldUninitialize) {
			CoUninitialize();
		}
		return;
	}

	UINT count = 0;
	if (SUCCEEDED(views->GetCount(&count))) {
		for (UINT i = 0; i < count; ++i) {
			Microsoft::WRL::ComPtr<IUnknown> view;
			if (FAILED(views->GetAt(i, applicationViewGuid, reinterpret_cast<void**>(view.GetAddressOf()))) || !view) {
				continue;
			}

			int showInSwitchers = 0;
			if (FAILED(GetApplicationViewShowInSwitchers(view.Get(), isModernView, &showInSwitchers)) || !showInSwitchers) {
				continue;
			}

			HWND thumbnailHwnd = nullptr;
			if (FAILED(GetApplicationViewThumbnailWindow(view.Get(), isModernView, &thumbnailHwnd)) || !thumbnailHwnd) {
				continue;
			}

			ImmersiveAppViewInfo info;
			info.thumbnailHwnd = thumbnailHwnd;
			info.title = GetWindowTitleFromHandle(thumbnailHwnd);
			info.processPath = GetProcessPathFromWindowHandle(thumbnailHwnd);
			info.view = view;
			info.isModernView = isModernView;

			PWSTR appId = nullptr;
			if (SUCCEEDED(GetApplicationViewAppUserModelId(view.Get(), isModernView, &appId)) && appId) {
				info.appUserModelId = appId;
				CoTaskMemFree(appId);
			}

			callback(info);
		}
	}

	if (shouldUninitialize) {
		CoUninitialize();
	}
}
