#pragma once
#include "plugins/BaseAction.hpp"
#include "OneNotePluginData.hpp"

class OneNoteAction final : public BaseAction {
public:
	OneNoteAction() {
		pluginId = m_pluginId;
	}

	std::wstring pageId;         // OneNote 页面 ID
	std::wstring pageName;       // 页面名称
	std::wstring sectionName;    // 分区名称
	std::wstring notebookName;   // 笔记本名称
	std::wstring hyperlink;      // 页面超链接
	std::wstring pageContent;    // 页面文本内容

	// 图标
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

	~OneNoteAction() override {
	}
};
