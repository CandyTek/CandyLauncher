#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <ShlObj.h>

// 一个来源串（标题或插件给的 match_text）加工出的全部搜索字段，全部小写，由 manager/MatchTextBuilder.hpp 生成并缓存。
// 条目只持有指向它的 shared_ptr：同一标题的条目、刷新前后的同一条目共用同一份，不再每条复制四个字符串
//（实测 4000 多条时每条平均多复制 300 多字节，合计 1.4 MB）。
struct ProcessedMatchText {
	std::wstring text; // 旧格式拼接串：原文 + 全拼 + 首字母……，精确模式的 contains 目标
	std::wstring title; // 清理后的小写原文
	std::wstring pinyin; // 标题全拼（中文替换为拼音），无中文时为空
	std::wstring initials; // 首字母缩写：中文取拼音首字母，英文单词取首字符
};

class BaseAction {
public:
	// 使用历史的键（插件包名 + stable_key 或 标题/副标题 的哈希），0 表示不记录
	uint64_t usageKey = 0;
	std::uint16_t pluginId = 65535;
	int32_t pluginPriority = 0;

	// ---- 匹配字段（全部小写，由宿主在条目入列表时填充，见 manager/MatchTextBuilder.hpp）----
	// matchText 用于编辑框搜索匹配：精确模式的 contains 目标；模糊模式里作为权重较低的“附加”通道，
	// 通常是 标题 + 拼音变体 + 路径 等的拼接。
	// matchTitle / matchPinyin / matchInitials 是模糊打分的结构化字段，分开打分可避免长 matchText 稀释标题命中的分数。
	const std::wstring& matchText() const {
		if (m_hasOwnMatchText) return m_ownMatchText;
		if (m_matchTextSource) return m_matchTextSource->text;
		return m_matchFields ? m_matchFields->text : EmptyMatchString();
	}

	const std::wstring& matchTitle() const { return m_matchFields ? m_matchFields->title : EmptyMatchString(); }
	const std::wstring& matchPinyin() const { return m_matchFields ? m_matchFields->pinyin : EmptyMatchString(); }
	const std::wstring& matchInitials() const { return m_matchFields ? m_matchFields->initials : EmptyMatchString(); }

	// 结构化字段（通常由标题加工而来）；没有另行指定 matchText 时，matchText 也取自这里
	void setMatchFields(std::shared_ptr<const ProcessedMatchText> fields) { m_matchFields = std::move(fields); }

	// matchText 来自另一来源串（如 标题 + 路径）的处理结果，与缓存共享，不复制
	void setMatchTextFrom(std::shared_ptr<const ProcessedMatchText> source) {
		m_matchTextSource = std::move(source);
		m_hasOwnMatchText = false;
		m_ownMatchText.clear();
	}

	// 插件自行拼好、原样使用的 matchText（CL_ACTION_MATCH_TEXT_VERBATIM 等），这是唯一每条单独保存的匹配串
	void setMatchText(std::wstring text) {
		m_ownMatchText = std::move(text);
		m_hasOwnMatchText = true;
		m_matchTextSource.reset();
	}

	// 撤销上面两种指定，matchText 回到结构化字段的 text
	void resetMatchTextOverride() {
		m_matchTextSource.reset();
		m_hasOwnMatchText = false;
		m_ownMatchText.clear();
	}

	virtual std::wstring& getTitle() = 0;

	virtual std::wstring& getSubTitle() = 0;

	virtual std::wstring& getIconFilePath() =0;

	virtual int getIconFilePathIndex() =0;

	virtual HBITMAP getIconBitmap() =0;

	// 按主窗口 DPI 缩放到 size x size 的图标位图（预乘 alpha），由实现类缓存；
	// 返回 nullptr 表示没有缓存，列表直接拉伸绘制 getIconBitmap()
	virtual HBITMAP getIconBitmapScaled(int /*size*/) { return nullptr; }

	virtual ~BaseAction() = default;

private:
	static const std::wstring& EmptyMatchString() {
		static const std::wstring empty;
		return empty;
	}

	std::shared_ptr<const ProcessedMatchText> m_matchFields;
	std::shared_ptr<const ProcessedMatchText> m_matchTextSource;
	std::wstring m_ownMatchText;
	bool m_hasOwnMatchText = false;
};
