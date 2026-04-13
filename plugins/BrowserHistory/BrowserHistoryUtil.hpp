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

#include "util/BitmapUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"


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

// 从 Chromium 类浏览器（Chrome/Edge）的 History SQLite 数据库读取历史记录
static std::vector<std::shared_ptr<BaseAction>> GetChromiumHistoryFromDB(
	const std::string& historyDbPath,
	const std::wstring& browserIconPath,
	int maxResults = 2000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

	try {
		// 检查文件是否存在
		if (!fs::exists(historyDbPath)) {
			ConsolePrintln(L"BrowserHistory", L"History database not found: " + utf8_to_wide(historyDbPath));
			return result;
		}

		// 由于浏览器可能正在使用数据库，我们需要复制一份到临时位置
		// std::string tempDbPath = historyDbPath + ".tmp";
		// try {
		// 	fs::copy_file(historyDbPath, tempDbPath, fs::copy_options::overwrite_existing);
		// } catch (const std::exception& e) {
		// 	Loge(L"BrowserHistory", L"Failed to copy history database: ", e.what());
		// 	return result;
		// }

		sqlite3* db = nullptr;
		// int rc = sqlite3_open(tempDbPath.c_str(), &db);
		// int rc = sqlite3_open_v2(historyDbPath.c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_SHAREDCACHE, nullptr);
		std::string dbUri = "file:" + historyDbPath + "?immutable=1";

		int rc = sqlite3_open_v2(
			dbUri.c_str(),
			&db,
			SQLITE_OPEN_READONLY | SQLITE_OPEN_URI | SQLITE_OPEN_SHAREDCACHE,
			nullptr
		);

		if (rc != SQLITE_OK) {
			Loge(L"BrowserHistory", L"Failed to open history database: ", sqlite3_errmsg(db));
			if (db) sqlite3_close(db);
			// 删除临时文件
			// try {
			// fs::remove(tempDbPath);
			// } catch (...) {}
			return result;
		}
		sqlite3_exec(db, "PRAGMA query_only = ON;", nullptr, nullptr, nullptr);

		// 查询历史记录，按访问次数排序（参考 Chromium.cs 实现）
		// urls 表结构：id, url, title, visit_count, typed_count, last_visit_time, hidden
		std::string sql = "SELECT url, title, visit_count, last_visit_time FROM urls "
			"WHERE hidden = 0 AND url NOT LIKE 'chrome://%' AND url NOT LIKE 'edge://%' "
			"ORDER BY last_visit_time DESC" + (maxResults > 0 ? " LIMIT " + std::to_string(maxResults) : "") + ";";
		// std::string sql = "SELECT url, title FROM urls "
		// 	"ORDER BY visit_count DESC LIMIT " + std::to_string(maxResults) + ";";

		sqlite3_stmt* stmt = nullptr;
		rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

		if (rc != SQLITE_OK) {
			Loge(L"BrowserHistory", L"Failed to prepare SQL statement: ", sqlite3_errmsg(db));
			sqlite3_close(db);
			// try {
			// 	fs::remove(tempDbPath);
			// } catch (...) {}
			return result;
		}

		int iconFilePathIndex = GetSysImageIndex(browserIconPath);

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			auto action = std::make_shared<BrowserHistoryAction>();

			try {
				// 读取 URL
				const unsigned char* url_text = sqlite3_column_text(stmt, 0);
				if (!url_text) {
					continue; // 跳过空 URL
				}
				std::string url = reinterpret_cast<const char*>(url_text);

				// 先尝试严格模式转换
				int size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.data(), static_cast<int>(url.size()), nullptr, 0);
				if (size_needed > 0) {
					std::wstring wurl(size_needed, 0);
					MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.data(), static_cast<int>(url.size()), &wurl[0], size_needed);
					action->url = wurl;
				} else {
					// 严格模式失败，尝试宽容模式
					size_needed = MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), nullptr, 0);
					if (size_needed > 0) {
						std::wstring wurl(size_needed, 0);
						MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), &wurl[0], size_needed);
						action->url = wurl;
					} else {
						// 完全失败，跳过这条记录
						continue;
					}
				}

				// 读取标题
				const unsigned char* title_text = sqlite3_column_text(stmt, 1);
				std::string title = title_text ? reinterpret_cast<const char*>(title_text) : "";

				if (!title.empty()) {
					// 先尝试严格模式
					size_needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), static_cast<int>(title.size()), nullptr,
													0);
					if (size_needed > 0) {
						std::wstring wtitle(size_needed, 0);
						MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, title.data(), static_cast<int>(title.size()), &wtitle[0],
											size_needed);
						action->title = wtitle;
					} else {
						// 严格模式失败，尝试宽容模式
						size_needed = MultiByteToWideChar(CP_UTF8, 0, title.data(), static_cast<int>(title.size()), nullptr, 0);
						if (size_needed > 0) {
							std::wstring wtitle(size_needed, 0);
							MultiByteToWideChar(CP_UTF8, 0, title.data(), static_cast<int>(title.size()), &wtitle[0], size_needed);
							action->title = wtitle;
						} else {
							// 标题转换失败，使用 URL
							action->title = action->url;
						}
					}
				}

				// 如果标题为空，使用 URL
				if (action->title.empty()) {
					action->title = action->url;
				}

				// 设置副标题为 URL
				action->subTitle = action->url;
			} catch (const std::exception& e) {
				// 转换出错，跳过这条记录
				Logi(L"BrowserHistory", L"Conversion error, skip this record", e.what());
				continue;
			}

			// 设置图标
			action->iconFilePath = browserIconPath;
			action->iconFilePathIndex = iconFilePathIndex;

			// 设置匹配文本（包装在 try-catch 中以防止 UTF-16 错误）
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

			result.push_back(action);
		}

		sqlite3_finalize(stmt);
		sqlite3_close(db);

		// 删除临时文件
		// try {
		// 	fs::remove(tempDbPath);
		// } catch (const std::exception& e) {
		// // 忽略删除临时文件的错误
		// 	Loge(L"BrowserHistory", L"Failed to remove temp database: ", e.what());
		// }
		ConsolePrintln(L"BrowserHistory", L"Loaded " + std::to_wstring(result.size()) + L" history items");
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

// 获取所有配置的浏览器历史记录
static std::vector<std::shared_ptr<BaseAction>> GetAllBrowserHistory() {
	std::vector<std::shared_ptr<BaseAction>> result;

	// 从配置读取浏览器列表和最大结果数
	int maxResults = static_cast<int>(m_host->GetSettingsMap().at("com.candytek.browserhistoryplugin.max_results").intValue);
	std::vector<std::string> browserList = m_host->GetSettingsMap().at("com.candytek.browserhistoryplugin.browser_list").stringArr;

	for (const std::string& browser : browserList) {
		std::string trimmedBrowser = MyTrim(browser);

		if (trimmedBrowser == "chrome") {
			auto history = GetChromeHistory(maxResults);
			result.insert(result.end(),
						std::make_move_iterator(history.begin()),
						std::make_move_iterator(history.end()));
		} else if (trimmedBrowser == "edge") {
			auto history = GetEdgeHistory(maxResults);
			result.insert(result.end(),
						std::make_move_iterator(history.begin()),
						std::make_move_iterator(history.end()));
		} else if (trimmedBrowser.empty()) {
			continue;
		} else {
			// 自定义路径
			char expandedPath[MAX_PATH];
			ExpandEnvironmentStringsA(trimmedBrowser.c_str(), expandedPath, MAX_PATH);

			if (GetFileAttributesA(expandedPath) != INVALID_FILE_ATTRIBUTES) {
				auto history = GetHistoryFromCustomPath(expandedPath, maxResults);
				result.insert(result.end(),
							std::make_move_iterator(history.begin()),
							std::make_move_iterator(history.end()));
			}
		}
	}

	return result;
}
