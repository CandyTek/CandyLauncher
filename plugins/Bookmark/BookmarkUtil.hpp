#pragma once

#include "util/json.hpp"
#include "BookmarkAction.hpp"

#include <filesystem>
#include <fstream>
#include <cstdlib>   // std::getenv
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <optional>

#include "util/BitmapUtil.hpp"
#include "util/StringUtil.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

// 递归收集节点中的书签
static void CollectBookmarksFromNode(const json& node,
									std::vector<std::shared_ptr<BaseAction>>& out,
									const std::wstring& browserIconPath) {
	// Chrome/Edge 节点类型：folder / url
	if (!node.is_object()) return;
	int iconFilePathIndex= GetSysImageIndex(browserIconPath);

	auto typeIt = node.find("type");
	if (typeIt != node.end() && typeIt->is_string()) {
		const std::string type = *typeIt;
		if (type == "url") {
			// 直接取 name / url
			const auto nameIt = node.find("name");
			const auto urlIt = node.find("url");
			if (nameIt != node.end() && urlIt != node.end() &&
				nameIt->is_string() && urlIt->is_string()) {
				auto temp = std::make_shared<BookmarkAction>();
				std::wstring name = utf8_to_wide(nameIt.value().get<std::string>());
				std::wstring url = utf8_to_wide(urlIt.value().get<std::string>());
				temp->title = name;
				temp->subTitle = url;
				temp->url = url;
				temp->iconFilePath = browserIconPath;
				temp->iconFilePathIndex = iconFilePathIndex;
				// temp->matchText = (name) + url;
				if (isMatchTextUrl) {
					temp->matchText = m_host->GetTheProcessedMatchingText(name) + url;
				} else {
					temp->matchText = m_host->GetTheProcessedMatchingText(name);
				}
				out.push_back(temp);
			}
			return;
		}
		if (type == "folder") {
			// 递归 children
			const auto childrenIt = node.find("children");
			if (childrenIt != node.end() && childrenIt->is_array()) {
				for (const auto& child : *childrenIt) {
					CollectBookmarksFromNode(child, out, browserIconPath);
				}
			}
			return;
		}
	}

	// 某些入口（比如 roots.bookmark_bar）本身可能没有 type 字段，但包含 children
	const auto childrenIt = node.find("children");
	if (childrenIt != node.end() && childrenIt->is_array()) {
		for (const auto& child : *childrenIt) {
			CollectBookmarksFromNode(child, out, browserIconPath);
		}
	}
}

// 返回第一个找到的 .ico 文件路径（非递归）
static std::string findFirstIcoFile(const std::string& folderPath) {
	try {
		const fs::path dir(folderPath);
		if (!fs::exists(dir) || !fs::is_directory(dir)) {
			return "";
		}

		for (const auto& entry : fs::directory_iterator(dir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".ico") {
				return entry.path().string(); // 找到第一个就返回
			}
		}
	} catch (const std::exception& e) {
		std::wcerr << L"Error: " << e.what() << std::endl;
	}
	return "";
}

// 解析指定基目录 + profile 下的 Chromium 书签（Chrome/Edge 通用）
// 如果你在多 Profile 环境（比如同时有 "Profile 1"、"Profile 2"），用 GetChromeBookmarksFromBaseDir(baseDir, "Profile 1") 指定即可。
static std::vector<std::shared_ptr<BaseAction>> GetChromeBookmarksFromBaseDir(const std::string& baseDir, const std::string& profile) {
	std::vector<std::shared_ptr<BaseAction>> result;


	try {
		fs::path p2 = fs::path(baseDir);
		fs::path p = fs::path(baseDir) / profile / "Bookmarks";
		if (is_directory(p2)) {
			if (!fs::exists(p)) {
				return {}; // 文件不存在，返回空
			}
		} else {
			p = p2;
		}
		std::string iconPath = p.parent_path().string();
		std::wstring browserIconPath = utf8_to_wide(findFirstIcoFile(iconPath));
		if (browserIconPath.empty()) {
			browserIconPath = LR"(C:\Program Files\Internet Explorer\iexplore.exe)";
		}

		std::ifstream ifs(p, std::ios::binary);
		if (!ifs) return {};

		json j;
		ifs >> j;

		// Chrome 的根一般是 j["roots"]，里面有 bookmark_bar / other / synced 等
		const auto rootsIt = j.find("roots");
		if (rootsIt == j.end() || !rootsIt->is_object()) {
			return {};
		}

		// 书签栏
		if (auto bb = rootsIt->find("bookmark_bar"); bb != rootsIt->end()) {
			CollectBookmarksFromNode(*bb, result, browserIconPath);
		}
		// 其他书签
		if (auto other = rootsIt->find("other"); other != rootsIt->end()) {
			CollectBookmarksFromNode(*other, result, browserIconPath);
		}
		// 如需包含“移动设备同步书签”，可开启：
		// if (auto synced = rootsIt->find("synced"); synced != rootsIt->end()) {
		//     CollectBookmarksFromNode(*synced, result);
		// }
	} catch (...) {
		// 解析/IO 出错就返回目前收集到的（或空）
	}
	return result;
}

inline std::string getLocalAppData() {
	char* buffer = nullptr;
	size_t len = 0;

	if (_dupenv_s(&buffer, &len, "LOCALAPPDATA") == 0 && buffer != nullptr) {
		std::string localAppData(buffer);
		free(buffer); // 记得释放内存
		return localAppData;
	}

	return "";
}


static std::vector<std::shared_ptr<BaseAction>> GetChromeBookmarks() {
	std::vector<std::shared_ptr<BaseAction>> result;

	const std::string localAppData = getLocalAppData();
	if (localAppData.empty()) {
		return result;
	}

	// %LOCALAPPDATA%\Google\Chrome\User Data\Default\Bookmarks
	fs::path baseDir = fs::path(localAppData) / "Google" / "Chrome" / "User Data";
	return GetChromeBookmarksFromBaseDir(baseDir.u8string(), "Default");
}

// ---- Edge: 新增支持 ----

// 如果未来需要支持自定义 profile（例如 "Profile 1"），可复用此函数
static std::vector<std::shared_ptr<BaseAction>> GetEdgeBookmarksFromBaseDir(const std::string& baseDir, const std::string& profile) {
	// Edge 书签文件结构与 Chrome 一致，直接复用解析逻辑
	return GetChromeBookmarksFromBaseDir(baseDir, profile);
}

static std::vector<std::shared_ptr<BaseAction>> GetEdgeBookmarks() {
	std::vector<std::shared_ptr<BaseAction>> result;

	const std::string localAppData = getLocalAppData();
	if (localAppData.empty()) {
		return result;
	}

	// %LOCALAPPDATA%\Microsoft\Edge\User Data\Default\Bookmarks
	fs::path baseDir = fs::path(localAppData) / "Microsoft" / "Edge" / "User Data";
	return GetEdgeBookmarksFromBaseDir(baseDir.u8string(), "Default");
}

// （可选）如需一次性聚合 Chrome + Edge：
static std::vector<std::shared_ptr<BaseAction>> GetAllChromiumBookmarks() {
	std::vector<std::shared_ptr<BaseAction>> result;
	std::vector<std::string> list = m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.browser_list").stringArr;

	for (std::string& basic_string : list) {
		if (basic_string == "chrome") {
			auto bookmarks = GetChromeBookmarks();
			result.insert(result.end(),
						std::make_move_iterator(bookmarks.begin()),
						std::make_move_iterator(bookmarks.end()));
		} else if (basic_string == "edge") {
			auto bookmarks = GetEdgeBookmarks();
			result.insert(result.end(),
						std::make_move_iterator(bookmarks.begin()),
						std::make_move_iterator(bookmarks.end()));
		} else if (MyTrim(basic_string).empty()) {
			continue;
		} else {
			char expandedPath[MAX_PATH];
			ExpandEnvironmentStringsA(basic_string.c_str(), expandedPath, MAX_PATH);
			if (GetFileAttributesA(expandedPath) == INVALID_FILE_ATTRIBUTES) {
				continue;
			} else {
				// 从路径推断浏览器类型
				auto bookmarks = GetChromeBookmarksFromBaseDir(expandedPath, "Default");
				result.insert(result.end(),
							std::make_move_iterator(bookmarks.begin()),
							std::make_move_iterator(bookmarks.end()));
			}
		}
	}

	return result;
}
