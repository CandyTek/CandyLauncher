#pragma once
#include <string>
#include <ShlObj.h>

class BaseAction {
public:
	// 此文本用于编辑框搜索匹配
	std::wstring matchText;
	std::uint16_t pluginId = 65535;

	virtual std::wstring& getTitle() = 0;

	virtual std::wstring& getSubTitle() = 0;

	virtual std::wstring& getIconFilePath() =0;

	virtual int getIconFilePathIndex() =0;

	virtual HBITMAP getIconBitmap() =0;

	virtual ~BaseAction() = default;
};
