// 模糊打分器与使用历史的单元测试，不依赖全局状态
#include <cstdio>
#include <string>
#include <vector>

#include "manager/FuzzyScorer.hpp"
#include "manager/UsageHistory.hpp"

namespace {

int g_failed = 0;

void Check(const bool ok, const char* what) {
	if (!ok) {
		++g_failed;
		std::printf("FAIL: %s\n", what);
	} else {
		std::printf("ok:   %s\n", what);
	}
}

struct Item {
	std::wstring title;
	std::wstring pinyin;
	std::wstring initials;
	std::wstring text;

	Item(std::wstring t, std::wstring p, std::wstring i, std::wstring x)
		: title(std::move(t)), pinyin(std::move(p)), initials(std::move(i)), text(std::move(x)) {
	}

	fuzzy::Fields Fields() const {
		fuzzy::Fields f;
		f.title = &title;
		f.pinyin = &pinyin;
		f.initials = &initials;
		f.text = &text;
		return f;
	}
};

double S(const std::wstring& query, const Item& item, const double threshold = 60) {
	const fuzzy::Query q(query, threshold);
	return fuzzy::Score(q, item.Fields());
}

const Item kChrome{L"google chrome", L"", L"gc", L"google chrome gc c:\\program files\\google\\chrome\\application\\chrome.exe"};
const Item kVsCode{L"visual studio code", L"", L"vsc", L"visual studio code vsc"};
const Item kVs{L"visual studio", L"", L"vs", L"visual studio vs"};
const Item kVs2022{L"visual studio 2022", L"", L"vs2", L"visual studio 2022 vs2"};
const Item kVsPrefixTitle{L"vscodium", L"", L"v", L"vscodium v"};
const Item kRun{L"run", L"", L"r", L"run r"};
const Item kQBit{L"qbittorrent", L"", L"q", L"qbittorrent q"};
const Item kXSplit{L"xsplit broadcaster - admin", L"", L"xba", L"xsplit broadcaster - admin xba"};
const Item kCodeEditor{L"code editor", L"", L"ce", L"code editor ce"};
const Item kBarcode{L"barcode tool", L"", L"bt", L"barcode tool bt"};
const Item kWeChat{L"微信", L"weixin", L"wx", L"微信 weixin wx"};
const Item kWeiBo{L"微博", L"weibo", L"wb", L"微博 weibo wb"};
const Item kGitBash{L"git bash", L"", L"gb", L"git bash gb"};
const Item kBash{L"bash shell", L"", L"bs", L"bash shell bs"};
const Item kLongTitle{L"microsoft visual studio code insiders preview build (x64)", L"", L"mvscipbx",
						L"microsoft visual studio code insiders preview build (x64) mvscipbx"};
// 只在路径里命中 chrome 的条目
const Item kChromeDriver{L"driver helper", L"", L"dh", L"driver helper dh d:\\tools\\chrome\\driver.exe"};

void TestBasics() {
	Check(S(L"zzzz", kChrome) == 0, "unrelated query is rejected");
	Check(S(L"", kChrome) == 0, "empty query is rejected");
	Check(S(L"chrome", kChrome) > 0, "substring query matches");
	Check(S(L"chrmoe", kChrome) > 0, "typo still matches through WRatio");
	Check(S(L"google chrome", kChrome) >= 130, "exact title gets exact bonus");
	Check(S(L"chromee", kChrome) > 0, "query one char longer than the title still matches");
}

void TestShortTargetsAreNotNeedles() {
	// WRatio 会把较短的目标当作子串：以前 "run" 对 "chrome" 得 60+，"zzzzqq" 对首字母 "q" 得 90
	Check(S(L"chrome", kRun) == 0, "a 3-letter title does not match a 6-letter query");
	Check(S(L"zzzzqq", kQBit) == 0, "a 1-letter initials string does not match a long query");
}

void TestTitleBeatsPath() {
	const double title = S(L"chrome", kChrome);
	const double path = S(L"chrome", kChromeDriver);
	Check(path > 0, "path-only hit is still a match");
	Check(title > path, "title hit ranks above path-only hit");
}

void TestPositionalOrder() {
	const double prefix = S(L"code", kCodeEditor);
	const double wordStart = S(L"code", kVsCode);
	const double substring = S(L"code", kBarcode);
	Check(prefix > wordStart, "prefix beats word start");
	Check(wordStart > substring, "word start beats inner substring");
	Check(substring > 0, "inner substring still matches");
}

void TestLongTitleNotDiluted() {
	// 标题比查询长 8 倍以上，纯 WRatio 会落到 60 分上限
	Check(S(L"code", kLongTitle) > 60, "word start in a very long title scores above the threshold");
}

void TestShortQueries() {
	Check(S(L"vs", kVs) > S(L"vs", kVsCode), "exact initials beat initials prefix for a short query");
	Check(S(L"vs", kVsCode) > 0, "initials prefix matches for a short query");
	Check(S(L"vs", kVs2022) == S(L"vs", kVsCode), "two initials-prefix hits tie (list order decides)");
	Check(S(L"x", kChrome) == 0, "single char does not match through the extra text");
	Check(S(L"c", kChrome) > 0, "single char matches a word start");
	Check(S(L"g", kChrome) > 0, "single char matches a title prefix");
	Check(S(L"wx", kWeChat) == fuzzy::shorttier::kInitialsExact, "pinyin initials exact for a short query");
	Check(S(L"wb", kWeChat) == 0, "wrong pinyin initials are rejected");
	Check(S(L"微", kWeChat) == fuzzy::shorttier::kTitlePrefix, "one chinese char is a short query and matches the title prefix");
	Check(S(L"vs", kVsPrefixTitle) > S(L"vs", kVs), "title prefix beats exact initials for a short query");
	Check(fuzzy::shorttier::kInitialsExact - fuzzy::shorttier::kInitialsPrefix < UsageHistory::kMaxBonus,
		"usage history can lift an initials-prefix item above an exact-initials one");
}

void TestPinyin() {
	Check(S(L"weixin", kWeChat) >= 100, "full pinyin matches the pinyin channel with exact bonus");
	Check(S(L"wei xin", kWeChat) > 0, "pinyin typed with a space still matches");
	Check(S(L"weixin", kWeChat) > S(L"weixin", kWeiBo), "the right pinyin item ranks first");
	Check(S(L"微信", kWeChat) >= 130, "two chinese chars take the normal path and hit the title exactly");
	Check(S(L"微信", kWeChat) > S(L"微信", kWeiBo), "exact chinese title beats a one-char overlap");
}

void TestMultiToken() {
	Check(S(L"git bash", kBash) == 0, "every token must match: 'git' missing");
	Check(S(L"git bash", kGitBash) >= 130, "all tokens match the title exactly");
	Check(S(L"visual code", kVsCode) > 0, "tokens out of order still match");
	Check(S(L"visual code", kVsCode) > S(L"visual code", kVs2022), "all-token hit ranks above partial");
	Check(S(L"git bash", kXSplit) == 0, "short tokens need a close match: 'git' vs 'split' is rejected");
	Check(S(L"vsual studio", kVsCode) > 0, "a typo inside a token still matches");
}

void TestThreshold() {
	Check(S(L"chrmoe", kChrome, 95) == 0, "a high threshold rejects typo matches");
	Check(S(L"chrome", kChrome, 95) > 0, "a high threshold keeps real hits because of the bonus");
}

void TestUsageHistory() {
	UsageHistory::Map map;
	const int64_t now = 1'800'000'000;
	Check(UsageHistory::BonusFor(map, 42, now) == 0, "unknown key gives no bonus");
	map[42] = UsageHistory::Entry{1.0, now};
	const double once = UsageHistory::BonusFor(map, 42, now);
	Check(once > 5 && once < 7, "one use is worth about 6 points");
	map[42] = UsageHistory::Entry{50.0, now};
	Check(UsageHistory::BonusFor(map, 42, now) == UsageHistory::kMaxBonus, "bonus is capped");
	map[42] = UsageHistory::Entry{1.0, now - 7 * 86400};
	Check(UsageHistory::BonusFor(map, 42, now) < once, "bonus decays after a week");
	Check(UsageHistory::BonusFor(map, 0, now) == 0, "key 0 is never boosted");
}

}

int main() {
	TestBasics();
	TestShortTargetsAreNotNeedles();
	TestTitleBeatsPath();
	TestPositionalOrder();
	TestLongTitleNotDiluted();
	TestShortQueries();
	TestPinyin();
	TestMultiToken();
	TestThreshold();
	TestUsageHistory();
	if (g_failed) {
		std::printf("%d check(s) failed\n", g_failed);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
