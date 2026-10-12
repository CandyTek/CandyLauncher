#pragma once
#include "OneNotePluginData.hpp"

class OneNoteAction final : public candy::ActionBase {
public:
	std::wstring pageId;         // OneNote 页面 ID
	std::wstring pageName;       // 页面名称
	std::wstring sectionName;    // 分区名称
	std::wstring notebookName;   // 笔记本名称
	std::wstring hyperlink;      // 页面超链接
	std::wstring pageContent;    // 页面文本内容
};
