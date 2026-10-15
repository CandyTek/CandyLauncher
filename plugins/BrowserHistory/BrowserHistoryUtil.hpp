#pragma once

#include "BrowserHistoryAction.hpp"
#include "BrowserHistoryPluginData.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <sqlite3.h>
#include <future>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/ParallelUtil.hpp"


// 返回第一个找到的 .ico 文件路径（非递归）
static std::string findFirstIcoFile(const std::string& folderPath) {
	namespace fs = std::filesystem;
	try {
		const fs::path dir(folderPath);
		if (!fs::exists(dir) || !fs::is_directory(dir)) {
			return "";
		}

		for (const auto& entry : fs::directory_iterator(dir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".ico") {
				return entry.path().string();
			}
		}
	} catch (const std::exception& e) {
		Loge(L"BrowserHistory", L"Error finding ico file: ", e.what());
	}
	return "";
}

inline std::string getLocalAppData() {
	char* buffer = nullptr;
	size_t len = 0;

	if (_dupenv_s(&buffer, &len, "LOCALAPPDATA") == 0 && buffer != nullptr) {
		std::string localAppData(buffer);
		free(buffer);
		return localAppData;
	}

	return "";
}

// 将 Chrome/Edge Webkit 时间戳（1601年以来的微秒数）转换为可读时间
static std::wstring ConvertWebkitTimestamp(int64_t webkit_timestamp) {
	// Chrome/Edge 使用从 1601-01-01 00:00:00 UTC 开始的微秒数
	// Windows FILETIME 也是从 1601-01-01 开始的 100 纳秒间隔数
	// 所以 webkit_timestamp 微秒 = webkit_timestamp * 10 个 100纳秒间隔

	FILETIME ft;
	SYSTEMTIME st, localSt;

	// 将微秒转换为 FILETIME（100纳秒单位）
	ULARGE_INTEGER uli;
	uli.QuadPart = webkit_timestamp * 10;
	ft.dwLowDateTime = uli.LowPart;
	ft.dwHighDateTime = uli.HighPart;

	// 转换为系统时间
	if (!FileTimeToSystemTime(&ft, &st)) {
		return L"";
	}

	// 转换为本地时间
	if (!SystemTimeToTzSpecificLocalTime(nullptr, &st, &localSt)) {
		return L"";
	}

	// 格式化时间字符串
	wchar_t buffer[100];
	swprintf_s(buffer, 100, L"%04d-%02d-%02d %02d:%02d:%02d",
				localSt.wYear, localSt.wMonth, localSt.wDay,
				localSt.wHour, localSt.wMinute, localSt.wSecond);

	return buffer;
}

struct RawHistoryRow {
	std::string url;
	std::string title;
	int64_t lastVisitTime = 0;
};

// 副标题和匹配文本里只保留地址的前这么多个字符。Edge 历史里带 token 的地址平均 1000 多字符、最长几万字符，
// 列表最多显示约 100 个字符，长地址对搜索也没有价值；完整地址只在 action->url 里保留一份用于打开和复制。
// 实测 2000 条历史把地址存 7 份要 41 MB，截断并改用哈希键后只剩几 MB。
constexpr size_t kMaxIndexedUrlChars = 256;

static std::wstring TruncateUrlForIndex(const std::wstring& url) {
	if (url.size() <= kMaxIndexedUrlChars) return url;
	return url.substr(0, kMaxIndexedUrlChars) + L"\u2026";
}

// 使用历史的稳定键：完整地址的 FNV-1a 64 位哈希（16 个十六进制字符），而不是再存一份地址
static std::wstring UrlStableKey(const std::wstring& url) {
	uint64_t h = 1469598103934665603ULL;
	for (const wchar_t ch : url) {
		h ^= static_cast<uint64_t>(ch);
		h *= 1099511628211ULL;
	}
	wchar_t buffer[17];
	swprintf_s(buffer, 17, L"%016llx", static_cast<unsigned long long>(h));
	return buffer;
}

// 从 Chromium 类浏览器（Chrome/Edge）的 History SQLite 数据库读取历史记录
// 先在单线程内取出原始行（SQLite 游标不能并行），再并行转换字符串与生成匹配文本。
// matchUrl 是本次刷新开始时抓取的设置快照：并行构造期间设置回调可能改写设置，不能边读全局变量边构造
static std::vector<std::shared_ptr<BrowserHistoryAction>> GetChromiumHistoryFromDB(
	const std::string& historyDbPath,
	const std::wstring& browserIconPath,
	const bool matchUrl,
	int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BrowserHistoryAction>> result;

	try {
		// 检查文件是否存在
		if (!fs::exists(historyDbPath)) {
			Logi(L"BrowserHistory", L"History database not found: ", historyDbPath);
			return result;
		}

		const auto t0 = std::chrono::steady_clock::now();
		// immutable=1：不加锁、不检查 WAL，可直接读取浏览器正在使用的数据库
		sqlite3* db = nullptr;
		const std::string dbUri = "file:" + historyDbPath + "?immutable=1";
		int rc = sqlite3_open_v2(dbUri.c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr);
		if (rc != SQLITE_OK) {
			Loge(L"BrowserHistory", L"Failed to open history database: ", sqlite3_errmsg(db));
			if (db) sqlite3_close(db);
			return result;
		}
		sqlite3_exec(db, "PRAGMA query_only = ON;", nullptr, nullptr, nullptr);
		// 使用内存映射读取，减少全表扫描时的 read 系统调用与页拷贝
		sqlite3_exec(db, "PRAGMA mmap_size = 268435456;", nullptr, nullptr, nullptr);

		// urls 表结构：id, url, title, visit_count, typed_count, last_visit_time, hidden
		// 子查询只对 id 排序，再按主键取 url/title，避免排序器搬运所有行的长字符串（约快一倍）
		// CROSS JOIN 固定以子查询为外层循环，结果保持 last_visit_time 降序
		const std::string sql = "SELECT u.url, u.title, u.last_visit_time FROM ("
			"SELECT id FROM urls "
			// data:/blob:/javascript: 地址动辄几十 KB 且无法再次打开，直接在库里过滤掉
			"WHERE hidden = 0 AND url NOT LIKE 'chrome://%' AND url NOT LIKE 'edge://%' "
			"AND url NOT LIKE 'data:%' AND url NOT LIKE 'blob:%' AND url NOT LIKE 'javascript:%' "
			"ORDER BY last_visit_time DESC" + (maxResults > 0 ? " LIMIT " + std::to_string(maxResults) : "") +
			") s CROSS JOIN urls u ON u.id = s.id;";

		sqlite3_stmt* stmt = nullptr;
		rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
		if (rc != SQLITE_OK) {
			Loge(L"BrowserHistory", L"Failed to prepare SQL statement: ", sqlite3_errmsg(db));
			sqlite3_close(db);
			return result;
		}

		std::vector<RawHistoryRow> rows;
		rows.reserve(maxResults > 0 ? static_cast<size_t>(maxResults) : 4096);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const auto* urlText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
			if (!urlText) continue; // 跳过空 URL
			const int urlLen = sqlite3_column_bytes(stmt, 0);
			const auto* titleText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
			const int titleLen = titleText ? sqlite3_column_bytes(stmt, 1) : 0;
			rows.push_back({std::string(urlText, urlLen), titleText ? std::string(titleText, titleLen) : std::string(),
							sqlite3_column_int64(stmt, 2)});
		}
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		const auto t1 = std::chrono::steady_clock::now();

		// 每个元素按下标写入，保持按访问时间排序；转换失败的记录留空后再剔除
		result.resize(rows.size());
		ParallelForRange(rows.size(), 64, [&](const size_t begin, const size_t end) {
			for (size_t i = begin; i < end; ++i) {
				auto& row = rows[i];
				auto action = std::make_shared<BrowserHistoryAction>();
				// 非法 UTF-8 序列会被替换为 U+FFFD；宽字符版只在这里临时用一下（截断、哈希），不保存
				std::wstring wideUrl;
				Utf8ToWideFast(row.url, wideUrl);
				if (wideUrl.empty()) continue;
				Utf8ToWideFast(row.title, action->title);
				// 显示和匹配都用截断后的地址，完整地址只以 UTF-8 留在 url 里
				const std::wstring indexedUrl = TruncateUrlForIndex(wideUrl);
				// 如果标题为空，使用 URL
				if (action->title.empty()) {
					action->title = indexedUrl;
				}
				// 副标题：访问时间 + URL
				const std::wstring visitTime = row.lastVisitTime > 0 ? ConvertWebkitTimestamp(row.lastVisitTime) : std::wstring();
				if (visitTime.empty()) {
					action->subtitle = indexedUrl;
				} else {
					action->subtitle = visitTime + L"  " + indexedUrl;
				}
				// 同一浏览器的所有条目共享图标，由主程序延迟解析
				action->iconPath = browserIconPath;
				// 键仍按宽字符地址哈希，和旧版本的使用历史保持一致
				action->stableKey = UrlStableKey(wideUrl);
				action->url = std::move(row.url);
				// 在插件内并行处理匹配文本，主程序原样使用
				action->matchTextVerbatim = true;
				try {
					if (matchUrl) {
						action->matchText = m_host->ProcessMatchText(action->title) + indexedUrl;
					} else {
						action->matchText = m_host->ProcessMatchText(action->title);
					}
				} catch (...) {
					// 如果处理失败，使用原始标题
					action->matchText = action->title;
				}
				result[i] = std::move(action);
			}
		});
		result.erase(std::remove(result.begin(), result.end(), nullptr), result.end());

		const auto t2 = std::chrono::steady_clock::now();
		using ms = std::chrono::milliseconds;
		Logi(L"BrowserHistory", L"[perf] ", historyDbPath, L" query=", std::chrono::duration_cast<ms>(t1 - t0).count(),
			L"ms build=", std::chrono::duration_cast<ms>(t2 - t1).count(), L"ms count=", result.size());
	} catch (const std::exception& e) {
		Loge(L"BrowserHistory", L"Exception in GetChromiumHistoryFromDB: ", e.what());
	}

	return result;
}

// 获取 Chrome 历史记录
static std::vector<std::shared_ptr<BrowserHistoryAction>> GetChromeHistory(const bool matchUrl, int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BrowserHistoryAction>> result;

	const std::string localAppData = getLocalAppData();
	if (localAppData.empty()) {
		return result;
	}

	// %LOCALAPPDATA%\Google\Chrome\User Data\Default\History
	fs::path baseDir = fs::path(localAppData) / "Google" / "Chrome" / "User Data" / "Default";
	std::string historyPath = (baseDir / "History").string();

	// 查找图标
	std::wstring browserIconPath = utf8_to_wide(findFirstIcoFile(baseDir.string()));
	if (browserIconPath.empty()) {
		// 使用默认浏览器图标
		browserIconPath = LR"(C:\Program Files\Internet Explorer\iexplore.exe)";
	}

	return GetChromiumHistoryFromDB(historyPath, browserIconPath, matchUrl, maxResults);
}

// 获取 Edge 历史记录
static std::vector<std::shared_ptr<BrowserHistoryAction>> GetEdgeHistory(const bool matchUrl, int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BrowserHistoryAction>> result;

	const std::string localAppData = getLocalAppData();
	if (localAppData.empty()) {
		return result;
	}

	// %LOCALAPPDATA%\Microsoft\Edge\User Data\Default\History
	fs::path baseDir = fs::path(localAppData) / "Microsoft" / "Edge" / "User Data" / "Default";
	std::string historyPath = (baseDir / "History").string();

	// 查找图标
	std::wstring browserIconPath = utf8_to_wide(findFirstIcoFile(baseDir.string()));
	if (browserIconPath.empty()) {
		browserIconPath = LR"(C:\Program Files\Internet Explorer\iexplore.exe)";
	}

	return GetChromiumHistoryFromDB(historyPath, browserIconPath, matchUrl, maxResults);
}

// 从自定义路径获取历史记录
static std::vector<std::shared_ptr<BrowserHistoryAction>> GetHistoryFromCustomPath(
	const std::string& profilePath,
	const bool matchUrl,
	int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BrowserHistoryAction>> result;

	try {
		fs::path p(profilePath);
		fs::path historyPath;

		if (fs::is_directory(p)) {
			// 如果是目录，尝试找 History 文件
			historyPath = p / "History";
		} else {
			// 直接使用文件路径
			historyPath = p;
		}

		if (!fs::exists(historyPath)) {
			return result;
		}

		// 查找图标
		std::wstring browserIconPath = utf8_to_wide(findFirstIcoFile(historyPath.parent_path().string()));
		if (browserIconPath.empty()) {
			browserIconPath = LR"(C:\Program Files\Internet Explorer\iexplore.exe)";
		}

		return GetChromiumHistoryFromDB(historyPath.string(), browserIconPath, matchUrl, maxResults);
	} catch (const std::exception& e) {
		Loge(L"BrowserHistory", L"Exception in GetHistoryFromCustomPath: ", e.what());
	}

	return result;
}

// 获取所有配置的浏览器历史记录，各浏览器并发读取，结果按配置顺序合并。
// matchUrl 由调用方在刷新开始时读取一次，整次刷新用同一个值
static std::vector<std::shared_ptr<BrowserHistoryAction>> GetAllBrowserHistory(const bool matchUrl) {
	std::vector<std::shared_ptr<BrowserHistoryAction>> result;

	// 从配置读取浏览器列表和最大结果数
	const int maxResults = static_cast<int>(m_host->GetInt(L"com.candytek.browserhistoryplugin.max_results", 2000));
	std::vector<std::string> browserList;
	for (const auto& item : m_host->GetStringArray(L"com.candytek.browserhistoryplugin.browser_list")) {
		browserList.push_back(wide_to_utf8(item));
	}

	std::vector<std::future<std::vector<std::shared_ptr<BrowserHistoryAction>>>> futures;
	futures.reserve(browserList.size());
	for (const std::string& browser : browserList) {
		std::string trimmedBrowser = MyTrim(browser);
		if (trimmedBrowser.empty()) continue;

		futures.push_back(std::async(std::launch::async, [trimmedBrowser, matchUrl, maxResults]() -> std::vector<std::shared_ptr<BrowserHistoryAction>> {
			try {
				if (trimmedBrowser == "chrome") {
					return GetChromeHistory(matchUrl, maxResults);
				}
				if (trimmedBrowser == "edge") {
					return GetEdgeHistory(matchUrl, maxResults);
				}
				// 自定义路径
				char expandedPath[MAX_PATH];
				ExpandEnvironmentStringsA(trimmedBrowser.c_str(), expandedPath, MAX_PATH);
				if (GetFileAttributesA(expandedPath) == INVALID_FILE_ATTRIBUTES) {
					return {};
				}
				return GetHistoryFromCustomPath(expandedPath, matchUrl, maxResults);
			} catch (...) {
				return {};
			}
		}));
	}

	for (auto& fut : futures) {
		auto history = fut.get();
		result.insert(result.end(),
					std::make_move_iterator(history.begin()),
					std::make_move_iterator(history.end()));
	}

	return result;
}
