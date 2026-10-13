#pragma once
#include <cstdint>
#include <string>
#include <ShlObj.h>
#include <cstdint>

class BaseAction {
public:
	// 此文本用于编辑框搜索匹配（精确模式的 contains 目标；模糊模式里作为权重较低的“附加”通道，
	// 通常是 标题 + 拼音变体 + 路径 等的拼接，全部小写）
	std::wstring matchText;
	// 模糊匹配的结构化字段，全部小写，由宿主在条目入列表时填充（见 manager/MatchTextBuilder.hpp）。
	// 分开打分可避免长 matchText 稀释标题命中的分数。
	std::wstring matchTitle; // 标题（清理后小写）
	std::wstring matchPinyin; // 标题全拼（中文替换为拼音），无中文时为空
	std::wstring matchInitials; // 首字母缩写：中文取拼音首字母，英文单词取首字符
	// 使用历史的键（插件包名 + stable_key 或 标题/副标题 的哈希），0 表示不记录
	uint64_t usageKey = 0;
	std::uint16_t pluginId = 65535;
	int32_t pluginPriority = 0;

	virtual std::wstring& getTitle() = 0;

	virtual std::wstring& getSubTitle() = 0;

	virtual std::wstring& getIconFilePath() =0;

	virtual int getIconFilePathIndex() =0;

	virtual HBITMAP getIconBitmap() =0;

	virtual ~BaseAction() = default;
};
