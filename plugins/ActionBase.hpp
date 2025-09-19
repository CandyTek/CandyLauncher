#pragma once
#include <string>
#include <ShlObj.h>

class ActionBase
{
public:
	// 用于编辑框匹配的文本
	std::wstring matchText;
	// 图标，只要是文件就可以
	std::uint16_t pluginId = 0;
	
	virtual std::wstring getTitle() = 0;

	virtual std::wstring getSubTitle() = 0;

	virtual std::wstring getIconFilePath() = 0;

	virtual int getIconFilePathIndex() = 0;

	virtual HBITMAP getIconBitmap() = 0;
	
	virtual ~ActionBase() = default;
	
};
