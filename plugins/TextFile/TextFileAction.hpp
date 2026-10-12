#pragma once
#include <algorithm>
#include <string>
#include "TextFilePluginData.hpp"

class TextFileAction final : public candy::ActionBase {
public:
	std::wstring filePath;
	// 小写的 标题拼音 + 全文内容，插件自行做子串匹配；只保存在插件侧，不发布给主程序
	std::wstring content;

	// 从内容中提取关键词前后的上下文（content 已是小写，searchText 也需传小写）
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
