#pragma once
// 模糊打分器。
//
// 基础相似度仍然来自 RapidFuzz 的 WRatio，但有三处与直接对整条 matchText 打分不同：
//   1. 标题 / 全拼 / 首字母 / 完整匹配串 分别打分取最大值，完整串权重略低。
//      WRatio 在目标比查询长 8 倍以上时会把分数乘 0.6，长串拼接会把标题命中稀释到阈值附近。
//   2. 在相似度之上叠加词法加分：整串相等、前缀、词首、首字母、拼音前缀……让“前缀 > 词首 > 子串 > 模糊”
//      的顺序稳定成立，而不是全部挤在 90 分。
//   3. 一两个字符的短查询不用 WRatio（partial_ratio 对单字符恒为 100），改走纯词法分级；
//      多词查询要求每个词都命中。
//
// 本文件不依赖全局状态，便于单元测试（tests/FuzzyScorerTest.cpp）。

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#include <rapidfuzz/fuzz.hpp>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace fuzzy {

// 一个条目的打分目标，全部为小写字符串的指针（允许为空指针或空串）
struct Fields {
	const std::wstring* title = nullptr; // 标题
	const std::wstring* pinyin = nullptr; // 标题全拼，无中文时为空
	const std::wstring* initials = nullptr; // 首字母缩写
	const std::wstring* text = nullptr; // 完整匹配串（标题+变体+路径等）
};

// 词法加分，取满足条件中的最大值而不是累加
namespace bonus {
constexpr double kTitleExact = 30;
constexpr double kInitialsExact = 24;
constexpr double kPinyinExact = 24;
constexpr double kTitlePrefix = 20;
constexpr double kPinyinPrefix = 16;
constexpr double kInitialsPrefix = 14;
constexpr double kTitleWordStart = 12;
constexpr double kAllTokensWordStart = 12;
constexpr double kPinyinWordStart = 10;
constexpr double kTitleContains = 8;
constexpr double kPinyinContains = 6;
constexpr double kAllTokensContained = 6;
constexpr double kInitialsContains = 4;
constexpr double kMax = kTitleExact;
}

// 短查询的分级（固定分，不受阈值影响）。相邻档位之间差距不大，使用历史加分（最多 15）能把常用项提上去
namespace shorttier {
constexpr double kTitleExact = 100;
constexpr double kTitlePrefix = 92;
constexpr double kInitialsExact = 90;
constexpr double kPinyinPrefix = 86;
constexpr double kInitialsPrefix = 84;
constexpr double kTitleWordStart = 80;
constexpr double kTitleContains = 70;
constexpr double kPinyinContains = 65;
constexpr double kInitialsContains = 62;
constexpr double kTextContains = 60;
}

// 完整匹配串的权重：只命中路径/副文本的条目要排在命中标题的条目后面
constexpr double kExtraTextWeight = 0.85;
// 多词查询里每个词的最低相似度：词都很短，partial_ratio 对 3~4 个字符过于宽松（"git" 能以 67 命中 "split"）
constexpr double kTokenMinScore = 75;
// 短查询：去掉空白后“信息量”不超过这个值的查询不走 WRatio。
// ASCII 字符记 1，非 ASCII（中文等，一个字约等于一个音节）记 2，所以 "wx"、单个汉字是短查询，两个汉字不是
constexpr size_t kShortQueryWeight = 2;

inline size_t QueryWeight(const std::wstring& compact) {
	size_t weight = 0;
	for (const wchar_t ch : compact) weight += ch >= 0x80 ? 2 : 1;
	return weight;
}

inline bool IsWordChar(const wchar_t ch) {
	// 非 ASCII（中文等）一律当作单词字符，只有 ASCII 标点和空白算分隔符
	return ch >= 0x80 || std::iswalnum(ch) != 0;
}

inline bool StartsWith(const std::wstring& s, const std::wstring& prefix) {
	return !prefix.empty() && s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// s 中是否有某个单词以 prefix 开头（单词边界 = 串首或前一个字符为分隔符）
inline bool HasWordStartingWith(const std::wstring& s, const std::wstring& prefix) {
	if (prefix.empty() || s.size() < prefix.size()) return false;
	size_t pos = 0;
	while ((pos = s.find(prefix, pos)) != std::wstring::npos) {
		if (pos == 0 || !IsWordChar(s[pos - 1])) return true;
		++pos;
	}
	return false;
}

inline std::wstring Trim(const std::wstring& s) {
	const size_t first = s.find_first_not_of(L" \t\r\n");
	if (first == std::wstring::npos) return {};
	const size_t last = s.find_last_not_of(L" \t\r\n");
	return s.substr(first, last - first + 1);
}

inline std::vector<std::wstring> SplitTokens(const std::wstring& s) {
	std::vector<std::wstring> tokens;
	size_t start = 0;
	while (start < s.size()) {
		const size_t end = s.find_first_of(L" \t\r\n", start);
		if (end == std::wstring::npos) {
			tokens.push_back(s.substr(start));
			break;
		}
		if (end > start) tokens.push_back(s.substr(start, end - start));
		start = end + 1;
	}
	return tokens;
}

// 每次按键只构造一次：查询的小写/紧凑/分词形式和 RapidFuzz 的缓存打分器
struct Query {
	std::wstring lower; // 去首尾空白的小写查询
	std::wstring compact; // 去掉全部空白，用于比对首字母和拼音
	std::vector<std::wstring> tokens; // 按空白分词
	bool isShort = false;
	double threshold = 60;
	rapidfuzz::fuzz::CachedWRatio<wchar_t> whole;
	// WRatio 总是把较短的一方当作子串去找，目标比查询短时会给出虚高的 partial 分
	// （"zzzzqq" 对首字母 "q" 得 90）。目标更短时改用不含 partial 的普通 ratio
	rapidfuzz::fuzz::CachedRatio<wchar_t> plain;
	// 多词查询时每个词一个 partial_ratio 打分器
	std::vector<rapidfuzz::fuzz::CachedPartialRatio<wchar_t>> tokenScorers;

	Query(const std::wstring& lowerKeyword, const double scoreThreshold)
		: lower(Trim(lowerKeyword)), threshold(scoreThreshold), whole(lower), plain(lower) {
		for (const wchar_t ch : lower) {
			if (!std::iswspace(ch)) compact += ch;
		}
		tokens = SplitTokens(lower);
		isShort = QueryWeight(compact) <= kShortQueryWeight;
		if (tokens.size() > 1) {
			tokenScorers.reserve(tokens.size());
			for (const auto& token : tokens) tokenScorers.emplace_back(token);
		}
	}

	Query(const Query&) = delete;
	Query& operator=(const Query&) = delete;
};

namespace detail {
inline const std::wstring& Str(const std::wstring* s) {
	static const std::wstring empty;
	return s ? *s : empty;
}

inline bool AllTokensSatisfy(const std::wstring& s, const std::vector<std::wstring>& tokens,
							bool (*pred)(const std::wstring&, const std::wstring&)) {
	for (const auto& token : tokens) {
		if (!pred(s, token)) return false;
	}
	return true;
}

inline bool Contains(const std::wstring& s, const std::wstring& needle) {
	return !needle.empty() && s.find(needle) != std::wstring::npos;
}

// 一两个字符：纯词法分级。分数固定在 60~100，不受阈值影响
inline double ScoreShort(const Query& q, const Fields& f) {
	const std::wstring& c = q.compact;
	const std::wstring& title = Str(f.title);
	const std::wstring& pinyin = Str(f.pinyin);
	const std::wstring& initials = Str(f.initials);
	const bool twoChars = c.size() >= 2;

	if (title == c) return shorttier::kTitleExact;
	if (StartsWith(title, c)) return shorttier::kTitlePrefix;
	if (!initials.empty() && initials == c) return shorttier::kInitialsExact;
	if (StartsWith(pinyin, c)) return shorttier::kPinyinPrefix;
	if (StartsWith(initials, c)) return shorttier::kInitialsPrefix;
	if (HasWordStartingWith(title, c)) return shorttier::kTitleWordStart;
	if (Contains(title, c)) return shorttier::kTitleContains;
	// 单字符的“包含”在拼音/首字母/附加串里几乎命中一切，只对两字符放开
	if (twoChars) {
		if (Contains(pinyin, c)) return shorttier::kPinyinContains;
		if (Contains(initials, c)) return shorttier::kInitialsContains;
		if (Contains(Str(f.text), c)) return shorttier::kTextContains;
	}
	return 0;
}

inline double LexicalBonus(const Query& q, const Fields& f) {
	const std::wstring& title = Str(f.title);
	const std::wstring& pinyin = Str(f.pinyin);
	const std::wstring& initials = Str(f.initials);
	double best = 0;
	const auto lift = [&best](const double b) {
		if (b > best) best = b;
	};

	if (!title.empty()) {
		if (title == q.lower) lift(bonus::kTitleExact);
		else if (StartsWith(title, q.lower)) lift(bonus::kTitlePrefix);
		else if (HasWordStartingWith(title, q.lower)) lift(bonus::kTitleWordStart);
		else if (Contains(title, q.lower)) lift(bonus::kTitleContains);
		else if (q.tokens.size() > 1) {
			if (AllTokensSatisfy(title, q.tokens, &HasWordStartingWith)) lift(bonus::kAllTokensWordStart);
			else if (AllTokensSatisfy(title, q.tokens, &Contains)) lift(bonus::kAllTokensContained);
		}
	}
	if (!initials.empty()) {
		if (initials == q.compact) lift(bonus::kInitialsExact);
		else if (StartsWith(initials, q.compact)) lift(bonus::kInitialsPrefix);
		else if (Contains(initials, q.compact)) lift(bonus::kInitialsContains);
	}
	if (!pinyin.empty()) {
		if (pinyin == q.compact) lift(bonus::kPinyinExact);
		else if (StartsWith(pinyin, q.compact)) lift(bonus::kPinyinPrefix);
		else if (HasWordStartingWith(pinyin, q.compact)) lift(bonus::kPinyinWordStart);
		else if (Contains(pinyin, q.compact)) lift(bonus::kPinyinContains);
	}
	return best;
}
}

// 返回 0 表示不匹配；否则返回不低于阈值的分数（可超过 100）
inline double Score(const Query& q, const Fields& f) {
	if (q.compact.empty()) return 0;
	if (q.isShort) return detail::ScoreShort(q, f);

	// 1. 基础相似度：各通道取最大。cutoff 让 RapidFuzz 在不可能超过当前最优时提前退出；
	//    加分最多 kMax，所以基础分低于 threshold - kMax 的通道无论如何都进不了结果
	double base = 0;
	const double floor = std::max(0.0, q.threshold - bonus::kMax);
	const auto consider = [&](const std::wstring* s, const double weight) {
		if (!s || s->empty()) return;
		const double cutoff = std::min(100.0, std::max(base, floor) / weight);
		const double raw = s->size() < q.lower.size()
								? q.plain.similarity(*s, cutoff)
								: q.whole.similarity(*s, cutoff);
		const double score = raw * weight;
		if (score > base) base = score;
	};
	consider(f.title, 1.0);
	consider(f.pinyin, 1.0);
	consider(f.initials, 1.0);
	if (f.text && (!f.title || *f.text != *f.title)) consider(f.text, kExtraTextWeight);

	// 2. 多词查询：每个词都必须在某个通道里命中，取最弱的那个词作为整体分
	if (!q.tokenScorers.empty()) {
		const double tokenCutoff = std::max(q.threshold, kTokenMinScore);
		double weakest = 100;
		for (size_t i = 0; i < q.tokenScorers.size(); ++i) {
			const auto& scorer = q.tokenScorers[i];
			const size_t tokenLength = q.tokens[i].size();
			double best = 0;
			for (const std::wstring* s : {f.title, f.pinyin, f.initials, f.text}) {
				// 比词还短的目标不参与（partial_ratio 会反过来拿目标当子串）
				if (!s || s->size() < tokenLength) continue;
				best = std::max(best, scorer.similarity(*s, std::max(best, tokenCutoff)));
			}
			weakest = std::min(weakest, best);
			if (weakest < tokenCutoff) return 0;
		}
		base = std::max(base, weakest);
	}

	// 3. 词法加分
	const double lexical = detail::LexicalBonus(q, f);
	if (base <= 0 && lexical <= 0) return 0;
	const double total = base + lexical;
	return total >= q.threshold ? total : 0;
}

}
