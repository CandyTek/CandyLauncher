#pragma once

#include "BaseAction.hpp"
#include "../common/GlobalState.hpp"
#include "../util/BitmapUtil.hpp"

using CallbackFunction = std::function<void()>;

class BaseLaunchAction : public BaseAction {
public:
	constexpr static int LOADTYPE_APP_RES = 0;
	constexpr static int LOADTYPE_SHELL32 = 1;


	std::wstring title;
	std::wstring subTitle;
	std::wstring iconFilePath;
	int iconFilePathIndex = -1;
	HBITMAP iconBitmap = nullptr;
	CallbackFunction callback1{};

	// 删除默认构造函数
	BaseLaunchAction() = delete;

	explicit BaseLaunchAction(
		const std::wstring& title2,
		const std::wstring& subTitle2,
		const std::wstring& icon,
		CallbackFunction callback
	): BaseAction(), callback1(std::move(callback)) {
		title = title2;
		subTitle = subTitle2;
		iconFilePath = icon;
	}

	explicit BaseLaunchAction(
		const std::wstring& title2,
		const std::wstring& subTitle2,
		const HBITMAP iconBitmap2,
		const int index,
		CallbackFunction callback
	): BaseAction(), callback1(std::move(callback)) {
		iconBitmap = iconBitmap2;
		title = title2;
		subTitle = subTitle2;
	}

	// 运行命令
	void Invoke() const {
		if (callback1) callback1();
	}

	std::wstring& getTitle() override {
		return title;
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
		return iconBitmap;
	}

	~BaseLaunchAction() override {
		if (iconBitmap) {
			DeleteObject(iconBitmap);
			iconBitmap = nullptr;
		}
	}
};
