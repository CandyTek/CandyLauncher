#pragma once
#include "BookmarkPluginData.hpp"

class BookmarkAction final : public BaseAction {
	public:
	BookmarkAction() {
		pluginId = m_pluginId;
	}
	std::wstring url;

	
	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;

	std::wstring& getTitle() override
	{
		return title;
	}

	std::wstring& getSubTitle() override
	{
		return subTitle;
	}

	std::wstring& getIconFilePath() override
	{
		return iconFilePath;
	}

	int getIconFilePathIndex() override
	{
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override
	{
		return nullptr;
	}

	~BookmarkAction() override
	{
	}
};
