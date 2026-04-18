#pragma once

#include <vector>
#include <memory>
#include "../util/ThreadPool.hpp"
#include <sstream>
#include <rapidfuzz/fuzz.hpp>

#include <optional>
#include <execution>
#include <string>
#include <algorithm>
#include <functional> // For std::greater
#include <thread>     // For std::thread::hardware_concurrency
#include <future>     // For std::async and std::future

#include "common/GlobalState.hpp"
#include "model/ScoredAction.hpp"
#include "plugins/BaseAction.hpp"
#include "util/StringUtil.hpp"

// 模糊搜索的异步线程
inline ThreadPool poolFuzzyMatch(std::thread::hardware_concurrency());

/**
 * 模糊匹配方法2，和方法1耗时上没区别
 * @param keyword 搜索词
 */
static void Fuzzymatch_MultiThreaded2(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
									std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const std::wstring lowerKeyword = MyToLower(keyword);

	// 拷贝快照，保证并行只读
	std::vector<std::shared_ptr<BaseAction>> snapshot = allActions;

	// 1) 并行打分到等长缓冲区
	std::vector<std::optional<ScoredAction>> tmp(snapshot.size());
	std::vector<size_t> idx(snapshot.size());
	std::iota(idx.begin(), idx.end(), 0);

	std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t i) {
		const auto& action = snapshot[i];
		const std::wstring& searchableText = action->matchText;

		if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword, searchableText); score >=
			pref_fuzzy_match_score_threshold) {
			tmp[i] = ScoredAction{action, score, action->pluginPriority};
		}
	});

	// 2) 压缩有效项
	std::vector<ScoredAction> scored;
	for (auto& o : tmp) if (o) scored.push_back(std::move(*o));

	// Top-K 用 nth_element 避免了不必要的全量排序
	// 3) Top-K + 排序（如果需要）
	size_t limit = (pref_max_search_results > 0)
						? static_cast<size_t>(pref_max_search_results)
						: scored.size();

	if (scored.size() > limit) {
		auto nth = scored.begin() + limit;
		std::nth_element(scored.begin(), nth, scored.end(),
						[](const ScoredAction& a, const ScoredAction& b) {
							return a.score > b.score;
						});
		scored.erase(nth, scored.end());
	}
	std::sort(scored.begin(), scored.end(), std::greater<>());

	std::transform(scored.begin(), scored.end(),
					std::back_inserter(filteredActions),
					[](const ScoredAction& s) {
						return s.action;
					});
}


static void Fuzzymatch(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
						std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	// 1. 准备数据：将关键字转为小写
	const std::wstring lowerKeyword = MyToLower(keyword);

	std::vector<ScoredAction> scoredActions;

	// 2. 计算分数：为所有 action 计算匹配度分数
	for (const std::shared_ptr<BaseAction>& action : allActions) {
		// 使用 rapidfuzz::fuzz::WRatio 计算加权比率分数。
		// WRatio 对于不同长度的字符串和乱序的单词有很好的效果，是通用的优选。
		if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword, action->matchText); score >=
			pref_fuzzy_match_score_threshold) {
			scoredActions.push_back({action, score, action->pluginPriority});
		}
	}

	// 3. 排序：按分数从高到低排序
	std::sort(scoredActions.begin(), scoredActions.end(), std::greater<>());

	int matchedCount = 0;
	for (const auto& [action, score, priority] : scoredActions) {
		filteredActions.push_back(action);
		if (pref_max_search_results > 0) {
			matchedCount++;
			if (matchedCount >= pref_max_search_results) break;
		}
	}
}


/**
 * 正常匹配，通过空格分隔每个关键字，通过 contain 来筛选项
 * 如果项达到上百万条，还可以进一步使用多线程方案
 * @param keyword 搜索词 
 */
static void Exactmatch_Optimized(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
								std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	// 分隔关键字
	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) {
		if (!word.empty()) words.push_back(word);
	}

	if (words.empty()) {
		return;
	}

	for (const auto& action : allActions) {
		const std::wstring& runCommand = action->matchText;

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


/**
 * 线程分配逻辑完全可控，可以做更精细的分块优化
 * @param keyword 搜索词
 */
static void Fuzzymatch_MultiThreaded(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
									std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	const std::wstring lowerKeyword = MyToLower(keyword);

	// 并行计算分数
	// 确定要使用的线程数，通常基于硬件核心数
	// hardware_concurrency() 可能返回0，所以至少保证1个线程
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) {
		numThreads = 1;
	}

	std::vector<std::future<std::vector<ScoredAction>>> futures;
	const size_t totalSize = allActions.size();
	const size_t chunkSize = (totalSize + numThreads - 1) / numThreads;

	// 创建并分发任务给多个线程
	for (unsigned int i = 0; i < numThreads; ++i) {
		// 1. 先计算索引，而不是直接操作迭代器
		const size_t start_index = i * chunkSize;

		// 2. 检查起始索引是否已超出范围，如果超出则后续已无数据可分，直接退出循环
		if (start_index >= totalSize) {
			break;
		}
		// 3. 计算结束索引，并确保它不会超过容器的实际大小
		const size_t end_index = std::min(start_index + chunkSize, totalSize);

		// 4. 使用安全的索引来创建迭代器
		const auto start_iterator = allActions.begin() + start_index;
		const auto end_iterator = allActions.begin() + end_index;

		// std::async 启动一个异步任务
		// 注意 lambda 的捕获列表，使用 start_iterator 和 end_iterator
		futures.push_back(poolFuzzyMatch.enqueue([&, start_iterator, end_iterator]()
			// futures.push_back(std::async(std::launch::async, [start_iterator, end_iterator, &lowerKeyword, this]() 
			{
				std::vector<ScoredAction> localScoredActions;

				// 每个线程在自己的数据块上进行计算
				for (auto it = start_iterator; it != end_iterator; ++it) {
					const auto& action = *it;
					if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword,
																	action->matchText); score >=
						pref_fuzzy_match_score_threshold) {
						localScoredActions.push_back({action, score});
					}
				}
				return localScoredActions; // 返回该线程的处理结果
			}));
	}

	// 合并所有线程的结果 (在主线程中完成)
	std::vector<ScoredAction> scoredActions;
	try {
		for (auto& f : futures) {
			// f.get() 会等待线程完成，并返回其结果
			std::vector<ScoredAction> localResult = f.get();
			// 将子线程的结果移动到主列表中，提高效率
			scoredActions.insert(scoredActions.end(),
								std::make_move_iterator(localResult.begin()),
								std::make_move_iterator(localResult.end()));
		}
	} catch (const std::exception& e) {
		// 记录错误或适当处理
		std::wcerr << L"An exception occurred during parallel fuzzy matching: " << e.what() << std::endl;
		// Clear any partial results and return
		return;
	}

	// 排序：按分数从高到低排序 (在主线程中完成)
	if (pref_max_search_results > 0 && scoredActions.size() > static_cast<size_t>(pref_max_search_results)) {
		// 只对前 pref_max_search_results 个元素进行部分排序
		std::partial_sort(scoredActions.begin(),
						scoredActions.begin() + static_cast<std::ptrdiff_t>(pref_max_search_results),
						scoredActions.end(),
						std::greater<>()); // 依然是降序

		// 调整大小，丢弃后面不需要的元素
		scoredActions.resize(static_cast<size_t>(pref_max_search_results));
	} else {
		// 如果匹配项不多，或者不需要限制数量，则进行全排序
		std::sort(scoredActions.begin(), scoredActions.end(), std::greater<>());
	}
	std::transform(scoredActions.begin(), scoredActions.end(),
					std::back_inserter(filteredActions),
					[](const ScoredAction& s) {
						return s.action;
					});
}


static void Exactmatch(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
						std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) words.push_back(word);
	int matchedCount = 0;

	for (const auto& action : allActions) {
		const std::wstring& runCommand = action->matchText;

		bool allMatch = true;
		for (const auto& w : words) {
			if (runCommand.find(w) == std::wstring::npos) {
				allMatch = false;
				break;
			}
		}

		if (allMatch) {
			filteredActions.push_back(action); // <-- 保存筛选后的 action 顺序
			if (pref_max_search_results > 0) {
				matchedCount++;
				if (matchedCount >= pref_max_search_results) break;
			}
		}
	}
}


inline void TextMatch(const std::wstring& keyword, const std::vector<std::shared_ptr<BaseAction>>& allActions,
					std::vector<std::shared_ptr<BaseAction>>& filteredActions) {
	if (pref_fuzzy_match) {
		MethodTimerStart();
#if defined(DEBUG) || _DEBUG
		Fuzzymatch(keyword, allActions, filteredActions);
#else
		// Fuzzymatch_MultiThreaded(keyword);
		Fuzzymatch_MultiThreaded2(keyword,allActions,filteredActions);
#endif
		MethodTimerEnd(L"fuzzymatch");
	} else {
		MethodTimerStart();
		// Exactmatch(keyword);
		Exactmatch_Optimized(keyword, allActions, filteredActions);
		MethodTimerEnd(L"Exactmatch");
	}
}
