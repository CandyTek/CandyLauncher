#pragma once
#include <string>
#include <cwchar>
#include <ShlObj.h>
#include <wrl/client.h>

#include "../../util/BaseTools.hpp"
#include "../../util/ImmersiveAppViewTraverser.hpp"
#include "../../util/LogUtil.hpp"
#include "RunningAppPluginData.hpp"
#include "../../util/MainTools.hpp"


class RunningAppAction final : public candy::ActionBase {
public:
	enum class ActivateType {
		WindowHandle,
		ApplicationView
	};

	// 图标，只要是文件就可以（iconPath 由主程序延迟解析系统图标）
	std::wstring filePath;
	// 窗口句柄的十进制字符串（Edge 标签页为缩略图窗口句柄）
	std::wstring runningAppHwnd;
	ActivateType activateType = ActivateType::WindowHandle;
	// 由工作线程的 STA 创建，只能在工作线程上调用和释放
	Microsoft::WRL::ComPtr<IUnknown> applicationView;
	bool isModernApplicationView = false;

	std::wstring& getFilePath() {
		return filePath;
	}

	// 解析失败返回 nullptr，不抛异常
	HWND GetHwnd() const {
		if (runningAppHwnd.empty()) return nullptr;
		wchar_t* end = nullptr;
		const unsigned long long value = wcstoull(runningAppHwnd.c_str(), &end, 10);
		if (!end || *end != L'\0') return nullptr;
		return reinterpret_cast<HWND>(static_cast<uintptr_t>(value));
	}

	// ApplicationView 类型必须在创建该 COM 指针的线程（插件工作线程）上调用
	void Invoke() const {
		if (activateType == ActivateType::ApplicationView && applicationView) {
			const HRESULT hr = SwitchToApplicationView(applicationView.Get(), isModernApplicationView);
			if (FAILED(hr)) Loge(L"RunningApp", L"SwitchToApplicationView failed hr=", hr, L" title=", title);
			return;
		}
		const HWND hwnd = GetHwnd();
		if (!hwnd || !IsWindow(hwnd)) {
			Logw(L"RunningApp", L"Window no longer exists: ", title);
			return;
		}
		showCurrectWindowSimple(hwnd);
	}
};
