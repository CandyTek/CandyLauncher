#pragma once
#include <string>
#include <ShlObj.h>
#include <wrl/client.h>

#include "../../util/BaseTools.hpp"
#include "../../util/ImmersiveAppViewTraverser.hpp"
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
	std::wstring runningAppHwnd;
	ActivateType activateType = ActivateType::WindowHandle;
	Microsoft::WRL::ComPtr<IUnknown> applicationView;
	bool isModernApplicationView = false;

	std::wstring& getFilePath() {
		return filePath;
	}

	void Invoke() const {
		try {
			if (activateType == ActivateType::ApplicationView && applicationView) {
				SwitchToApplicationView(applicationView.Get(), isModernApplicationView);
				return;
			}
			showCurrectWindowSimple(reinterpret_cast<HWND>((static_cast<uintptr_t>(std::stoull(runningAppHwnd)))));
		} catch (...) {
		}
	}
};
