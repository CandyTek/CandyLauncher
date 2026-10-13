#pragma once
// 把标题/来源串加工成搜索字段：小写 + 拼音变体（matchText）以及模糊打分用的结构化字段
// （matchTitle / matchPinyin / matchInitials），并计算使用历史的键。
// 拼音转换是发布条目时最贵的一步，所以结果按来源串缓存，拼音方案切换时整体丢弃。

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "plugins/BaseAction.hpp"
#include "util/PinyinHelper.hpp"
#include "util/StringUtil.hpp"

struct ProcessedMatchText {
	std::wstring text; // 旧格式拼接串：原文 + 全拼 + 首字母……
	std::wstring title; // 清理后的小写原文
	std::wstring pinyin;
	std::wstring initials;
};

inline ProcessedMatchText BuildProcessedMatchText(const std::wstring& source) {
	ProcessedMatchText r;
	r.title = MyToLower(sanitizeVisible(source));
	const PinyinHelper::MatchVariants v = PinyinHelper::BuildMatchVariants(r.title);
	r.text = PinyinHelper::ComposeMatchText(r.title, v);
	r.pinyin = v.pinyin;
	r.initials = v.acronym;
	return r;
}

// Lower case + pinyin processing is the expensive part of a publish; titles rarely change between
// refreshes, so the result is kept per source string. Dropped when the pinyin scheme changes.
struct MatchTextCache {
	std::shared_mutex mutex;
	std::unordered_map<std::wstring, std::shared_ptr<const ProcessedMatchText>> map;
	const void* scheme = nullptr;
	static constexpr size_t MAX_ENTRIES = 200000;

	std::shared_ptr<const ProcessedMatchText> Get(const std::wstring& source) {
		{
			std::shared_lock<std::shared_mutex> lock(mutex);
			if (scheme == pCurrPinyin) {
				const auto it = map.find(source);
				if (it != map.end()) return it->second;
			}
		}
		auto processed = std::make_shared<const ProcessedMatchText>(BuildProcessedMatchText(source));
		std::unique_lock<std::shared_mutex> lock(mutex);
		if (scheme != pCurrPinyin) {
			map.clear();
			scheme = pCurrPinyin;
		} else if (map.size() >= MAX_ENTRIES) {
			map.clear();
		}
		map.emplace(source, processed);
		return processed;
	}

	// 只填结构化字段，不动 matchText（matchText 可能由插件自行拼接）
	void FillFields(BaseAction& action, const std::wstring& title) {
		const auto p = Get(title);
		action.matchTitle = p->title;
		action.matchPinyin = p->pinyin;
		action.matchInitials = p->initials;
	}

	// matchText 与结构化字段都来自标题
	void FillAll(BaseAction& action, const std::wstring& title) {
		const auto p = Get(title);
		action.matchText = p->text;
		action.matchTitle = p->title;
		action.matchPinyin = p->pinyin;
		action.matchInitials = p->initials;
	}
};

inline MatchTextCache g_matchTextCache;

inline uint64_t FnvHash64(const std::wstring& s, uint64_t h = 1469598103934665603ULL) {
	for (const wchar_t ch : s) {
		h ^= static_cast<uint64_t>(ch);
		h *= 1099511628211ULL;
	}
	return h;
}

// 使用历史的键：插件包名 + stable_key；没有 stable_key 时退回 标题 + 副标题
inline uint64_t ComputeUsageKey(const std::wstring& pkgName, const std::wstring& stableKey,
								const std::wstring& title, const std::wstring& subtitle) {
	uint64_t h = FnvHash64(pkgName);
	h = FnvHash64(L"\x1f", h);
	if (!stableKey.empty()) return FnvHash64(stableKey, h);
	h = FnvHash64(title, h);
	h = FnvHash64(L"\x1f", h);
	return FnvHash64(subtitle, h);
}
