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
#include <future>
#include <iostream>
#include <optional>

#include "util/BitmapUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/ParallelUtil.hpp"

using nlohmann::json;

// 1. 轻量结构体：仅引用 JSON 字符串，零堆内存分配
struct RawBookmarkRef {
	std::string_view name;
	std::string_view url;
};

// 2. 单线程快速扫描整棵树（内存访问连续，毫秒级即可完成数万节点的遍历）
static void FlattenBookmarkNodes(const json& node, std::vector<RawBookmarkRef>& rawList) {
	if (!node.is_object()) return;

	auto typeIt = node.find("type");
	if (typeIt != node.end() && typeIt->is_string()) {
		const std::string_view type = typeIt->get_ref<const std::string&>();
		if (type == "url") {
			const auto nameIt = node.find("name");
			const auto urlIt = node.find("url");
			if (nameIt != node.end() && urlIt != node.end() &&
				nameIt->is_string() && urlIt->is_string()) {
				// 直接持有 json 内存块的 string_view
				rawList.push_back({
					nameIt->get_ref<const std::string&>(),
					urlIt->get_ref<const std::string&>()
				});
				}
			return;
		}
		if (type == "folder") {
			const auto childrenIt = node.find("children");
			if (childrenIt != node.end() && childrenIt->is_array()) {
				for (const auto& child : *childrenIt) {
					FlattenBookmarkNodes(child, rawList);
				}
			}
			return;
		}
	}

	const auto childrenIt = node.find("children");
	if (childrenIt != node.end() && childrenIt->is_array()) {
		for (const auto& child : *childrenIt) {
			FlattenBookmarkNodes(child, rawList);
		}
	}
}

// 并行构建 Action：每个元素按下标写入，保持书签原有顺序
static void BuildBookmarkActions(
	const std::vector<RawBookmarkRef>& rawList,
	std::vector<std::shared_ptr<BaseAction>>& out,
	const std::shared_ptr<LazySysImageIndex>& icon) {
	const size_t base = out.size();
	out.resize(base + rawList.size());

	ParallelForRange(rawList.size(), 64, [&](const size_t begin, const size_t end) {
		for (size_t i = begin; i < end; ++i) {
			const auto& item = rawList[i];
			auto act = std::make_shared<BookmarkAction>();
			Utf8ToWideFast(item.name, act->title);
			Utf8ToWideFast(item.url, act->url);
			act->subTitle = act->url;
			act->icon = icon;
			try {
				act->matchText = isMatchTextUrl
					? m_host->GetTheProcessedMatchingText(act->title) + act->url
					: m_host->GetTheProcessedMatchingText(act->title);
			} catch (...) {
				act->matchText = act->title;
			}
			out[base + i] = std::move(act);
		}
	});
}

static bool ReadWholeFile(const std::filesystem::path& path, std::string& out) {
	std::ifstream ifs(path, std::ios::binary | std::ios::ate);
	if (!ifs) return false;
	const std::streamoff size = ifs.tellg();
	if (size <= 0) return false;
	out.resize(static_cast<size_t>(size));
	ifs.seekg(0);
	return static_cast<bool>(ifs.read(out.data(), size));
}

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
				return entry.path().string(); // 找到第一个就返回
			}
		}
	} catch (const std::exception& e) {
		Loge(L"BookmarkUtil", L"Error finding ico file: ", e.what());
	}
	return "";
}

// 解析指定基目录 + profile 下的 Chromium 书签（Chrome/Edge 通用）
// 如果你在多 Profile 环境（比如同时有 "Profile 1"、"Profile 2"），用 GetChromeBookmarksFromBaseDir(baseDir, "Profile 1") 指定即可。
static std::vector<std::shared_ptr<BaseAction>> GetChromeBookmarksFromBaseDir(const std::string& baseDir, const std::string& profile) {
	namespace fs = std::filesystem;
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
		std::wstring browserIconPath = utf8_to_wide(findFirstIcoFile(p.parent_path().string()));
		if (browserIconPath.empty()) {
			browserIconPath = LR"(C:\Program Files\Internet Explorer\iexplore.exe)";
		}
		const auto icon = std::make_shared<LazySysImageIndex>(std::move(browserIconPath));

		const auto t0 = std::chrono::steady_clock::now();
		std::string content;
		if (!ReadWholeFile(p, content)) return {};
		// 从连续内存解析，比 istream 逐字符读取快得多
		const json j = json::parse(content.data(), content.data() + content.size(), nullptr, false);
		if (j.is_discarded()) return {};
		const auto t1 = std::chrono::steady_clock::now();

		// Chrome 的根一般是 j["roots"]，里面有 bookmark_bar / other / synced 等
		const auto rootsIt = j.find("roots");
		if (rootsIt == j.end() || !rootsIt->is_object()) {
			return {};
		}

		std::vector<RawBookmarkRef> rawList;
		rawList.reserve(4096);
		// 书签栏
		if (auto bb = rootsIt->find("bookmark_bar"); bb != rootsIt->end()) {
			FlattenBookmarkNodes(*bb, rawList);
		}
		// 其他书签
		if (auto other = rootsIt->find("other"); other != rootsIt->end()) {
			FlattenBookmarkNodes(*other, rawList);
		}
		BuildBookmarkActions(rawList, result, icon);
		const auto t2 = std::chrono::steady_clock::now();
		using ms = std::chrono::milliseconds;
		Logi(L"BookmarkUtil", L"[perf] ", p.wstring(), L" parse=", std::chrono::duration_cast<ms>(t1 - t0).count(),
			L"ms build=", std::chrono::duration_cast<ms>(t2 - t1).count(), L"ms count=", result.size());
		// 如需包含“移动设备同步书签”，可开启：
		// if (auto synced = rootsIt->find("synced"); synced != rootsIt->end()) {
		//     FlattenBookmarkNodes(*synced, rawList);
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
	namespace fs = std::filesystem;
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
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

	const std::string localAppData = getLocalAppData();
	if (localAppData.empty()) {
		return result;
	}

	// %LOCALAPPDATA%\Microsoft\Edge\User Data\Default\Bookmarks
	fs::path baseDir = fs::path(localAppData) / "Microsoft" / "Edge" / "User Data";
	return GetEdgeBookmarksFromBaseDir(baseDir.u8string(), "Default");
}

static std::vector<std::shared_ptr<BaseAction>> GetAllChromiumBookmarks3() {
	std::vector<std::shared_ptr<BaseAction>> result;
	const auto& list = m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.browser_list").stringArr;

	std::vector<std::future<std::vector<std::shared_ptr<BaseAction>>>> futures;

	for (const std::string& item : list) {
		if (MyTrim(item).empty()) continue;

		futures.push_back(std::async(std::launch::async, [item]() {
			if (item == "chrome") {
				return GetChromeBookmarks();
			} else if (item == "edge") {
				return GetEdgeBookmarks();
			} else {
				char expandedPath[MAX_PATH];
				ExpandEnvironmentStringsA(item.c_str(), expandedPath, MAX_PATH);
				if (GetFileAttributesA(expandedPath) == INVALID_FILE_ATTRIBUTES) {
					return std::vector<std::shared_ptr<BaseAction>>{};
				}
				return GetChromeBookmarksFromBaseDir(expandedPath, "Default");
			}
		}));
	}

	// 收集并发结果
	for (auto& fut : futures) {
		auto part = fut.get();
		result.insert(result.end(),
					  std::make_move_iterator(part.begin()),
					  std::make_move_iterator(part.end()));
	}

	return result;
}

static std::vector<std::shared_ptr<BaseAction>> GetAllChromiumBookmarks() {
    std::vector<std::shared_ptr<BaseAction>> result;
    
    // 拷贝配置列表，避免异步线程执行期间 settings 发生重新分配
    const auto list = m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.browser_list").stringArr;

    std::vector<std::future<std::vector<std::shared_ptr<BaseAction>>>> futures;
    futures.reserve(list.size());

    for (const std::string& item : list) {
        std::string trimmed = MyTrim(item);
        if (trimmed.empty()) continue;

        // 按值捕获 item，确保线程内部持有独立副本
        futures.push_back(std::async(std::launch::async, [trimmed]() -> std::vector<std::shared_ptr<BaseAction>> {
            try {
                if (trimmed == "chrome") {
                    return GetChromeBookmarks();
                } else if (trimmed == "edge") {
                    return GetEdgeBookmarks();
                } else {
                    // 安全展开环境变量
                    DWORD requiredSize = ExpandEnvironmentStringsA(trimmed.c_str(), nullptr, 0);
                    if (requiredSize == 0) return {};

                    std::string expandedPath(requiredSize, '\0');
                    if (ExpandEnvironmentStringsA(trimmed.c_str(), expandedPath.data(), requiredSize) == 0) {
                        return {};
                    }
                    expandedPath.pop_back(); // 移除末尾 '\0'

                    if (GetFileAttributesA(expandedPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                        return {};
                    }
                    return GetChromeBookmarksFromBaseDir(expandedPath, "Default");
                }
            } catch (...) {
                return {};
            }
        }));
    }

    // 汇总各异步任务结果
    for (auto& fut : futures) {
        if (fut.valid()) {
            auto part = fut.get();
            result.insert(result.end(),
                          std::make_move_iterator(part.begin()),
                          std::make_move_iterator(part.end()));
        }
    }

    return result;
}

// （可选）如需一次性聚合 Chrome + Edge：
static std::vector<std::shared_ptr<BaseAction>> GetAllChromiumBookmarks2() {
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
