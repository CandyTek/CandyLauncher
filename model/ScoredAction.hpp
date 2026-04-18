#pragma once


#include <memory>

#include "plugins/BaseAction.hpp"
/**
 * 定义一个结构体来存储 action 及其匹配分数
 */
struct ScoredAction {
	std::shared_ptr<BaseAction> action;
	double score;
	int32_t pluginPriority = 0;

	/**
	 * 用于排序的比较运算符：先按分数降序，分数相同时按插件优先级降序
	 */
	bool operator>(const ScoredAction& other) const {
		if (score != other.score) return score > other.score;
		return pluginPriority > other.pluginPriority;
	}
};
