#pragma once


#include <cstdint>
#include <memory>

#include "plugins/BaseAction.hpp"
/**
 * 定义一个结构体来存储 action 及其匹配分数
 */
struct ScoredAction {
	std::shared_ptr<BaseAction> action;
	double score = 0;
	int32_t pluginPriority = 0;
	// 平局时的确定性次序：标题越短越精确，再按原列表位置，避免同分项在两次按键之间跳动
	uint32_t titleLength = 0;
	uint32_t index = 0;

	/**
	 * 用于排序的比较运算符：分数降序 > 插件优先级降序 > 标题长度升序 > 原顺序
	 */
	bool operator>(const ScoredAction& other) const {
		if (score != other.score) return score > other.score;
		if (pluginPriority != other.pluginPriority) return pluginPriority > other.pluginPriority;
		if (titleLength != other.titleLength) return titleLength < other.titleLength;
		return index < other.index;
	}
};
