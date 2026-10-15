#pragma once
// 把标题/来源串加工成搜索字段：小写 + 拼音变体（matchText）以及模糊打分用的结构化字段
// （matchTitle / matchPinyin / matchInitials），并计算使用历史的键。
// 拼音转换是发布条目时最贵的一步，所以结果按来源串缓存，拼音方案切换时整体丢弃。
// 条目（BaseAction）直接共享缓存里的 ProcessedMatchText，不再逐条复制字符串。

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <windows.h>

#include "plugins/BaseAction.hpp"
#include "util/PinyinHelper.hpp"
#include "util/StringUtil.hpp"

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
//
// 条目消失后它的缓存项不会马上删除（下次刷新大多还会用到，省掉拼音转换）；但窗口标题这类不断变化的来源串会让
// 缓存只增不减，所以条数超过上次清理后两倍（至少 4096）时，把最近 10 分钟没用过的项清掉。
struct MatchTextCache {
	struct Entry {
		std::shared_ptr<const ProcessedMatchText> value;
		uint32_t lastUseMinute = 0;
	};

	std::shared_mutex mutex;
	std::unordered_map<std::wstring, Entry> map;
	const void* scheme = nullptr;
	size_t sweepThreshold = MIN_SWEEP_THRESHOLD;
	static constexpr size_t MAX_ENTRIES = 200000;
	static constexpr size_t MIN_SWEEP_THRESHOLD = 4096;
	static constexpr uint32_t IDLE_MINUTES = 10;

	static uint32_t NowMinute() { return static_cast<uint32_t>(GetTickCount64() / 60000); }

	std::shared_ptr<const ProcessedMatchText> Get(const std::wstring& source) {
		const uint32_t now = NowMinute();
		{
			std::shared_lock<std::shared_mutex> lock(mutex);
			if (scheme == pCurrPinyin) {
				const auto it = map.find(source);
				if (it != map.end()) {
					// 持共享锁写一个 32 位时间戳：多个线程同时写也无妨，它只用于清理判断
					it->second.lastUseMinute = now;
					return it->second.value;
				}
			}
		}
		auto processed = std::make_shared<const ProcessedMatchText>(BuildProcessedMatchText(source));
		std::unique_lock<std::shared_mutex> lock(mutex);
		if (scheme != pCurrPinyin) {
			map.clear();
			scheme = pCurrPinyin;
			sweepThreshold = MIN_SWEEP_THRESHOLD;
		} else if (map.size() >= MAX_ENTRIES) {
			map.clear();
		} else if (map.size() >= sweepThreshold) {
			SweepLocked(now);
		}
		// 并发构建同一来源串时以先插入者为准，让所有条目共享同一份
		const auto [it, inserted] = map.try_emplace(source, Entry{processed, now});
		if (!inserted) it->second.lastUseMinute = now;
		return it->second.value;
	}

	// 只填结构化字段，不动 matchText（matchText 可能由插件自行拼接）
	void FillFields(BaseAction& action, const std::wstring& title) {
		action.setMatchFields(Get(title));
	}

	// matchText 与结构化字段都来自标题
	void FillAll(BaseAction& action, const std::wstring& title) {
		action.setMatchFields(Get(title));
		action.resetMatchTextOverride();
	}

private:
	void SweepLocked(const uint32_t now) {
		for (auto it = map.begin(); it != map.end();) {
			if (now - it->second.lastUseMinute > IDLE_MINUTES) it = map.erase(it);
			else ++it;
		}
		sweepThreshold = (std::max)(MIN_SWEEP_THRESHOLD, map.size() * 2);
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

// 以 NUL 结尾的字符串版本，供 ABI 侧的 const wchar_t* 直接哈希，不先拷贝成 std::wstring
inline uint64_t FnvHash64(const wchar_t* s, uint64_t h = 1469598103934665603ULL) {
	for (; s && *s; ++s) {
		h ^= static_cast<uint64_t>(*s);
		h *= 1099511628211ULL;
	}
	return h;
}

// 使用历史的键：插件包名 + stable_key；没有 stable_key（空或 null）时退回 标题 + 副标题。
// stableKey 直接取 CL_ActionDesc::stable_key，主程序不保存它的副本
inline uint64_t ComputeUsageKey(const std::wstring& pkgName, const wchar_t* stableKey,
								const std::wstring& title, const std::wstring& subtitle) {
	uint64_t h = FnvHash64(pkgName);
	h = FnvHash64(L"\x1f", h);
	if (stableKey && *stableKey) return FnvHash64(stableKey, h);
	h = FnvHash64(title, h);
	h = FnvHash64(L"\x1f", h);
	return FnvHash64(subtitle, h);
}
