#pragma once

#include "BrowserHistoryAction.hpp"
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

#include "util/BitmapUtil.hpp"
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
};

// 从 Chromium 类浏览器（Chrome/Edge）的 History SQLite 数据库读取历史记录
// 先在单线程内取出原始行（SQLite 游标不能并行），再并行转换字符串与生成匹配文本
static std::vector<std::shared_ptr<BaseAction>> GetChromiumHistoryFromDB(
	const std::string& historyDbPath,
	const std::wstring& browserIconPath,
	int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

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
		const std::string sql = "SELECT u.url, u.title FROM ("
			"SELECT id FROM urls "
			"WHERE hidden = 0 AND url NOT LIKE 'chrome://%' AND url NOT LIKE 'edge://%' "
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
			rows.push_back({std::string(urlText, urlLen), titleText ? std::string(titleText, titleLen) : std::string()});
		}
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		const auto t1 = std::chrono::steady_clock::now();

		const auto icon = std::make_shared<LazySysImageIndex>(browserIconPath);

		// 每个元素按下标写入，保持按访问时间排序；转换失败的记录留空后再剔除
		result.resize(rows.size());
		ParallelForRange(rows.size(), 64, [&](const size_t begin, const size_t end) {
			for (size_t i = begin; i < end; ++i) {
				const auto& row = rows[i];
				auto action = std::make_shared<BrowserHistoryAction>();
				// 非法 UTF-8 序列会被替换为 U+FFFD
				Utf8ToWideFast(row.url, action->url);
				if (action->url.empty()) continue;
				Utf8ToWideFast(row.title, action->title);
				// 如果标题为空，使用 URL
				if (action->title.empty()) {
					action->title = action->url;
				}
				action->subTitle = action->url;
				action->icon = icon;
				try {
					if (isMatchTextUrl) {
						action->matchText = m_host->GetTheProcessedMatchingText(action->title) + action->url;
					} else {
						action->matchText = m_host->GetTheProcessedMatchingText(action->title);
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
static std::vector<std::shared_ptr<BaseAction>> GetChromeHistory(int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

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

	return GetChromiumHistoryFromDB(historyPath, browserIconPath, maxResults);
}

// 获取 Edge 历史记录
static std::vector<std::shared_ptr<BaseAction>> GetEdgeHistory(int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

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

	return GetChromiumHistoryFromDB(historyPath, browserIconPath, maxResults);
}

// 从自定义路径获取历史记录
static std::vector<std::shared_ptr<BaseAction>> GetHistoryFromCustomPath(
	const std::string& profilePath,
	int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

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

		return GetChromiumHistoryFromDB(historyPath.string(), browserIconPath, maxResults);
	} catch (const std::exception& e) {
		Loge(L"BrowserHistory", L"Exception in GetHistoryFromCustomPath: ", e.what());
	}

	return result;
}

// 获取所有配置的浏览器历史记录，各浏览器并发读取，结果按配置顺序合并
static std::vector<std::shared_ptr<BaseAction>> GetAllBrowserHistory() {
	std::vector<std::shared_ptr<BaseAction>> result;

	// 从配置读取浏览器列表和最大结果数
	const int maxResults = static_cast<int>(m_host->GetSettingsMap().at("com.candytek.browserhistoryplugin.max_results").intValue);
	const std::vector<std::string> browserList = m_host->GetSettingsMap().at("com.candytek.browserhistoryplugin.browser_list").stringArr;

	std::vector<std::future<std::vector<std::shared_ptr<BaseAction>>>> futures;
	futures.reserve(browserList.size());
	for (const std::string& browser : browserList) {
		std::string trimmedBrowser = MyTrim(browser);
		if (trimmedBrowser.empty()) continue;

		futures.push_back(std::async(std::launch::async, [trimmedBrowser, maxResults]() -> std::vector<std::shared_ptr<BaseAction>> {
			try {
				if (trimmedBrowser == "chrome") {
					return GetChromeHistory(maxResults);
				}
				if (trimmedBrowser == "edge") {
					return GetEdgeHistory(maxResults);
				}
				// 自定义路径
				char expandedPath[MAX_PATH];
				ExpandEnvironmentStringsA(trimmedBrowser.c_str(), expandedPath, MAX_PATH);
				if (GetFileAttributesA(expandedPath) == INVALID_FILE_ATTRIBUTES) {
					return {};
				}
				return GetHistoryFromCustomPath(expandedPath, maxResults);
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
