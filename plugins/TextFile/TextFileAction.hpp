#pragma once
#include "plugins/BaseAction.hpp"
#include "TextFilePluginData.hpp"

class TextFileAction final : public BaseAction {
public:
	TextFileAction() {
		pluginId = m_pluginId;
	}

	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;

	// 用于动态提取上下文
	std::wstring searchKeyword;  // 当前搜索的关键词
	bool needExtractContext = false;  // 是否需要提取上下文

	std::wstring& getTitle() override {
		return title;
	}

	std::wstring& getSubTitle() override {
		// 如果需要提取上下文且有搜索关键词，动态生成副标题
		if (needExtractContext && !searchKeyword.empty()) {
			subTitle = ExtractContextLazy(matchText, searchKeyword);
			needExtractContext = false; // 提取一次后就不再重复提取
		}
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

	~TextFileAction() override {
	}

private:
	// 懒加载方式提取上下文
	static std::wstring ExtractContextLazy(const std::wstring& content, const std::wstring& searchText, size_t contextLength = 50) {
		if (content.empty() || searchText.empty()) {
			return L"";
		}

		size_t pos = content.find(searchText);
		if (pos == std::wstring::npos) {
			return L"";
		}

		size_t start = (pos > contextLength) ? (pos - contextLength) : 0;
		size_t end = std::min(pos + searchText.length() + contextLength, content.length());

		std::wstring context = content.substr(start, end - start);

		// 替换换行符为空格
		for (auto& ch : context) {
			if (ch == L'\n' || ch == L'\r' || ch == L'\t') {
				ch = L' ';
			}
		}

		// 添加省略号
		std::wstring result;
		if (start > 0) {
			result += L"...";
		}
		result += context;
		if (end < content.length()) {
			result += L"...";
		}

		return result;
	}
};
