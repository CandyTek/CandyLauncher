#pragma once
#include "plugins/BaseAction.hpp"
#include "EverNotePluginData.hpp"

class EverNoteAction final : public BaseAction {
public:
	EverNoteAction() {
		pluginId = m_pluginId;
	}

	std::wstring noteId;  // Evernote 笔记 GUID
	std::wstring notebookName;  // 笔记本名称
	std::wstring userId;  // 用户 ID (owner)
	std::wstring shardId;  // Shard ID

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

	~EverNoteAction() override {
	}
};
