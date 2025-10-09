#pragma once


#include <memory>

#include "plugins/BaseAction.hpp"
/**
 * 定义一个结构体来存储 action 及其匹配分数
 */
struct ScoredAction {
	std::shared_ptr<BaseAction> action;
	double score;

	/**
	 * 用于排序的比较运算符
	 */
	bool operator>(const ScoredAction& other) const {
		return score > other.score;
	}
};
