#pragma once

#include <algorithm>
#include <execution>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "common/GlobalState.hpp"
#include "manager/FuzzyScorer.hpp"
#include "manager/UsageHistory.hpp"
#include "model/ScoredAction.hpp"
#include "plugins/BaseAction.hpp"
#include "util/StringUtil.hpp"

namespace TextMatchDetail {

inline fuzzy::Fields FieldsOf(const BaseAction& action) {
	fuzzy::Fields f;
	f.title = &action.matchTitle();
	f.pinyin = &action.matchPinyin();
	f.initials = &action.matchInitials();
	f.text = &action.matchText();
	return f;
}

// 单个条目打分：模糊分 + 使用历史加分；不匹配返回 nullopt
inline std::optional<ScoredAction> ScoreOne(const fuzzy::Query& query, const std::shared_ptr<BaseAction>& action,
											const size_t index, const UsageHistory::Map* usage, const int64_t now) {
	double score = fuzzy::Score(query, FieldsOf(*action));
	if (score <= 0) return std::nullopt;
	if (usage) score += UsageHistory::BonusFor(*usage, action->usageKey, now);
	const size_t titleLength = action->matchTitle().empty() ? action->matchText().size() : action->matchTitle().size();
	return ScoredAction{action, score, action->pluginPriority, static_cast<uint32_t>(titleLength),
						static_cast<uint32_t>(index)};
}

// Top-K（nth_element 避免全量排序）+ 排序 + 输出
inline void EmitSorted(std::vector<ScoredAction>& scored, std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const size_t limit = (pref_max_search_results > 0)
							? static_cast<size_t>(pref_max_search_results)
							: scored.size();
	if (scored.size() > limit) {
		const auto nth = scored.begin() + static_cast<std::ptrdiff_t>(limit);
		std::nth_element(scored.begin(), nth, scored.end(), std::greater<>());
		scored.erase(nth, scored.end());
	}
	std::sort(scored.begin(), scored.end(), std::greater<>());
	filteredActions.reserve(filteredActions.size() + scored.size());
	for (const auto& s : scored) filteredActions.push_back(s.action);
}

inline std::shared_ptr<const UsageHistory::Map> UsageSnapshot() {
	if (!pref_usage_history_boost) return nullptr;
	return g_usageHistory.Snapshot();
}

}

/**
 * 模糊匹配（单线程），Debug 下使用，也便于对比
 * @param keyword 搜索词
 */
static void Fuzzymatch(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
						std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const fuzzy::Query query(MyToLower(keyword), static_cast<double>(pref_fuzzy_match_score_threshold));
	const auto usage = TextMatchDetail::UsageSnapshot();
	const int64_t now = UsageHistory::Now();

	std::vector<ScoredAction> scored;
	for (size_t i = 0; i < allActions.size(); ++i) {
		if (auto s = TextMatchDetail::ScoreOne(query, allActions[i], i, usage.get(), now)) {
			scored.push_back(std::move(*s));
		}
	}
	TextMatchDetail::EmitSorted(scored, filteredActions);
}

/**
 * 模糊匹配（并行打分）。打分写入等长缓冲区，不复制 shared_ptr 列表
 * @param keyword 搜索词
 */
static void Fuzzymatch_Parallel(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
								std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const fuzzy::Query query(MyToLower(keyword), static_cast<double>(pref_fuzzy_match_score_threshold));
	const auto usage = TextMatchDetail::UsageSnapshot();
	const int64_t now = UsageHistory::Now();

	std::vector<size_t> indices(allActions.size());
	std::iota(indices.begin(), indices.end(), size_t{0});
	std::vector<std::optional<ScoredAction>> tmp(allActions.size());
	const UsageHistory::Map* usageMap = usage.get();
	std::for_each(std::execution::par, indices.begin(), indices.end(), [&](const size_t i) {
		tmp[i] = TextMatchDetail::ScoreOne(query, allActions[i], i, usageMap, now);
	});

	std::vector<ScoredAction> scored;
	for (auto& o : tmp) if (o) scored.push_back(std::move(*o));
	TextMatchDetail::EmitSorted(scored, filteredActions);
}


/**
 * 正常匹配，通过空格分隔每个关键字，通过 contain 来筛选项
 * 如果项达到上百万条，还可以进一步使用多线程方案
 * @param keyword 搜索词
 */
static void Exactmatch_Optimized(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
								std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const std::wstring lowerKeyword = MyToLower(keyword);
	if (lowerKeyword.find_first_of(L" \t\r\n") == std::wstring::npos) {
		for (const auto& action : allActions) {
			if (action->matchText().find(lowerKeyword) != std::wstring::npos) {
				filteredActions.push_back(action);
				if (pref_max_search_results > 0 &&
					filteredActions.size() >= static_cast<size_t>(pref_max_search_results)) {
					break;
				}
			}
		}
		return;
	}

	// 分隔关键字
	std::wstringstream ss(lowerKeyword);
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) {
		if (!word.empty()) words.push_back(word);
	}

	if (words.empty()) {
		return;
	}

	for (const auto& action : allActions) {
		const std::wstring& runCommand = action->matchText();

		bool isMatch = true;
		for (const auto& w : words) {
			if (runCommand.find(w) == std::wstring::npos) {
				isMatch = false;
				break;
			}
		}

		if (isMatch) {
			// 只将匹配项添加到 vector 中
			filteredActions.push_back(action);
			if (pref_max_search_results > 0 && filteredActions.size() >= static_cast<size_t>(
				pref_max_search_results)) {
				break; // 达到最大结果数，停止筛选
			}
		}
	}
}


inline void TextMatch(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
					std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	if (allActions.empty()) {
		return;
	}
	if (pref_fuzzy_match) {
		MethodTimerStart(L"fuzzymatch");
#if defined(DEBUG) || _DEBUG
		Fuzzymatch(keyword, allActions, filteredActions);
#else
		Fuzzymatch_Parallel(keyword, allActions, filteredActions);
#endif
		MethodTimerEnd(L"fuzzymatch");
	} else {
		MethodTimerStart(L"Exactmatch");
		Exactmatch_Optimized(keyword, allActions, filteredActions);
		MethodTimerEnd(L"Exactmatch");
	}
}
