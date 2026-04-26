#pragma once
#include <string>
#include <ShlObj.h>
#include <wrl/client.h>

#include "../../util/BaseTools.hpp"
#include "../../util/ImmersiveAppViewTraverser.hpp"
#include "RunningAppPluginData.hpp"
#include "../../util/MainTools.hpp"


class RunningAppAction final : public BaseAction {
	std::wstring iconFilePath;

public:
	enum class ActivateType {
		WindowHandle,
		ApplicationView
	};

	RunningAppAction() {
		pluginId = m_pluginId;
	}


	// 图标，只要是文件就可以
	std::wstring filePath;
	std::wstring title;
	std::wstring subTitle;
	std::wstring runningAppHwnd;
	int iconFilePathIndex = -1;
	ActivateType activateType = ActivateType::WindowHandle;
	Microsoft::WRL::ComPtr<IUnknown> applicationView;
	bool isModernApplicationView = false;

	std::wstring& getTitle() override {
		return title;
	}
	
	std::wstring& getFilePath() {
		return filePath;
	}

	std::wstring& getSubTitle() override {
		return subTitle;
	}

	std::wstring& getIconFilePath() override {
		return iconFilePath;
	}

	int getIconFilePathIndex() override {
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
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


	~RunningAppAction() override {
	}
};
