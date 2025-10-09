#pragma once
#include "plugins/BaseAction.hpp"
#include "RegistryPluginData.hpp"

class RegistryAction final : public BaseAction {
public:
	RegistryAction() {
		pluginId = m_pluginId;
	}

	std::wstring keyPath;        // 注册表完整路径
	std::wstring valueName;      // 值名称（如果有）
	std::wstring valueData;      // 值数据（如果有）
	std::wstring valueType;      // 值类型（REG_SZ, REG_DWORD 等）
	bool hasException = false;   // 是否有访问异常
	std::wstring exceptionMsg;   // 异常信息
	int subKeyCount = 0;         // 子键数量
	int valueCount = 0;          // 值数量

	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;

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
		return nullptr;
	}

	~RegistryAction() override {
	}
};
