#include <vector>
#include <string>
#include <map>
#include <functional>
#include <unordered_map>
#include <cstddef>
#include <fstream>
#include <filesystem>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginApi.h"
#include "model/SettingItem.hpp"
#include "util/MyJsonUtil.hpp"

// 被测插件源码直接编译进本程序，通过 C ABI 入口访问；
// 也可以传入插件 DLL 路径，加载任意编译器构建的插件：PluginTest.exe <plugin.dll> [--host-minor=0]
extern "C" __declspec(dllexport) const CL_PluginApi* CL_CALL CandyPlugin_GetApi(void);

static void TestLog(const std::wstring& msg) {
	Logi(L"PluginTest", msg);
}

static std::wstring W(const wchar_t* value) {
	return value ? std::wstring(value) : std::wstring();
}

struct MockAction {
	uint64_t userData = 0;
	std::wstring title;
	std::wstring subtitle;
	std::wstring matchText;
	std::wstring stableKey;
	std::wstring iconKey;
};

// 与主程序一致：以首元素的 size 作为步长，缺失的尾部字段读作 0
static std::vector<MockAction> ReadDescs(const CL_ActionDesc* items, const size_t count) {
	std::vector<MockAction> actions;
	for (size_t i = 0; i < count; ++i) {
		CL_ActionDesc desc{};
		std::memcpy(&desc, reinterpret_cast<const char*>(items) + i * items[0].size,
					std::min<size_t>(items[0].size, sizeof(desc)));
		MockAction action{desc.user_data, W(desc.title), W(desc.subtitle), W(desc.match_text)};
		action.stableKey = W(desc.stable_key);
		action.iconKey = W(desc.icon_key);
		actions.push_back(std::move(action));
	}
	return actions;
}

// 简单的 Mock 主程序，只依赖 CandyPluginApi.h
struct MockHost {
	std::unordered_map<std::string, SettingItem> settingsMap;
	std::map<uint32_t, std::vector<MockAction>> sets;
	std::map<uint32_t, uint32_t> setFlags;
	int messageCount = 0;
	std::vector<std::pair<uint32_t, std::wstring>> sinkQueries;
	std::wstring dragUrl;
	std::wstring dragHtml;
	std::wstring dragRtf;
	std::wstring clipboardText;
	int clipboardCount = 0;
	// 2.2
	std::vector<std::wstring> keywords;
	uint32_t triggerFlags = 0;
	bool keywordsSet = false;
	std::vector<MockAction> sinkDescs;
	std::vector<std::pair<uint64_t, std::vector<MockAction>>> postedResults;
	uint64_t currentQueryId = 1;
	int notifyCount = 0;
	// 2.3：模拟主程序已解析好的字符串表（key -> 当前语言文本）
	std::unordered_map<std::wstring, std::wstring> strings;
	std::wstring language = L"en-US";
};

static MockHost g_mock;

static const SettingItem* FindSetting(const wchar_t* key) {
	const auto it = g_mock.settingsMap.find(wide_to_utf8(W(key)));
	return it != g_mock.settingsMap.end() ? &it->second : nullptr;
}

// hostMinor 为 0 时模拟 2.0 主程序：表更小，不含之后追加的函数
static CL_HostApi MakeMockHostApi(const uint32_t hostMinor) {
	CL_HostApi api{};
	api.size = sizeof(api);
	api.api_major = CL_API_VERSION_MAJOR;
	api.api_minor = hostMinor;
	api.log = [](CL_PluginHandle, int32_t level, const wchar_t* message) {
		TestLog(L"[Log " + std::to_wstring(level) + L"] " + W(message));
	};
	api.show_message = [](CL_PluginHandle, const wchar_t* title, const wchar_t* message) {
		++g_mock.messageCount;
		TestLog(L"[Message] " + W(title) + L": " + W(message));
	};
	api.show_toast = [](CL_PluginHandle, const wchar_t* title, const wchar_t* message) {
		TestLog(L"[Toast] " + W(title) + L": " + W(message));
	};
	api.show_main_window = [](CL_PluginHandle, int32_t, int32_t) -> int32_t { return 1; };
	api.task_done = [](CL_PluginHandle) {};
	api.set_edit_text = [](CL_PluginHandle, const wchar_t* text) { TestLog(L"[ChangeText] " + W(text)); };
	api.get_edit_text = [](CL_PluginHandle, CL_StrSink sink, void* ctx) { sink(ctx, L"", 0); };
	api.get_action_arg = [](CL_PluginHandle, CL_StrSink sink, void* ctx) { sink(ctx, L"", 0); };
	api.process_match_text = [](CL_PluginHandle, const wchar_t* source, CL_StrSink sink, void* ctx) {
		// 简单返回原文本（实际会做小写和拼音处理）
		const std::wstring text = W(source);
		sink(ctx, text.c_str(), text.size());
	};
	api.set_actions = [](CL_PluginHandle, uint32_t setId, const CL_ActionDesc* items, size_t count, uint32_t flags) {
		g_mock.sets[setId] = ReadDescs(items, count);
		g_mock.setFlags[setId] = flags;
	};
	api.set_action_set_flags = [](CL_PluginHandle, uint32_t setId, uint32_t flags) { g_mock.setFlags[setId] = flags; };
	api.clear_actions = [](CL_PluginHandle, uint32_t setId) { g_mock.sets.erase(setId); };
	api.show_action_set = [](CL_PluginHandle, uint32_t setId) {
		TestLog(L"[ShowActionSet] " + std::to_wstring(setId));
	};
	api.sink_add_matches = [](CL_ResultSinkHandle, CL_PluginHandle, uint32_t setId, const wchar_t* query) -> size_t {
		g_mock.sinkQueries.emplace_back(setId, W(query));
		return 0;
	};
	api.sink_add_items = [](CL_ResultSinkHandle, CL_PluginHandle, uint32_t, const uint64_t*, size_t) -> size_t {
		return 0;
	};
	api.get_setting_bool = [](CL_PluginHandle, const wchar_t* key, int32_t* out) -> int32_t {
		const SettingItem* item = FindSetting(key);
		if (!item) return 0;
		*out = item->boolValue ? 1 : 0;
		return 1;
	};
	api.get_setting_int = [](CL_PluginHandle, const wchar_t* key, int64_t* out) -> int32_t {
		const SettingItem* item = FindSetting(key);
		if (!item) return 0;
		*out = item->intValue;
		return 1;
	};
	api.get_setting_double = [](CL_PluginHandle, const wchar_t* key, double* out) -> int32_t {
		const SettingItem* item = FindSetting(key);
		if (!item) return 0;
		*out = item->doubleValue;
		return 1;
	};
	api.get_setting_string = [](CL_PluginHandle, const wchar_t* key, CL_StrSink sink, void* ctx) -> int32_t {
		const SettingItem* item = FindSetting(key);
		if (!item) return 0;
		const std::wstring value = utf8_to_wide(item->stringValue);
		sink(ctx, value.c_str(), value.size());
		return 1;
	};
	api.get_setting_string_array = [](CL_PluginHandle, const wchar_t* key, CL_StrSink sink, void* ctx) -> int32_t {
		const SettingItem* item = FindSetting(key);
		if (!item) return 0;
		for (const auto& element : item->stringArr) {
			const std::wstring value = utf8_to_wide(element);
			sink(ctx, value.c_str(), value.size());
		}
		return 1;
	};
	api.set_setting_json = [](CL_PluginHandle, const wchar_t* key, const wchar_t* valueJson) -> int32_t {
		const auto it = g_mock.settingsMap.find(wide_to_utf8(W(key)));
		if (it == g_mock.settingsMap.end()) return 0;
		it->second.setValue(nlohmann::json::parse(wide_to_utf8(W(valueJson))));
		return 1;
	};
	api.is_plugin_enabled = [](CL_PluginHandle, const wchar_t*) -> int32_t { return 1; };
	api.enum_plugin_catalog = [](CL_PluginHandle, CL_CatalogVisitor, void*) -> int32_t { return 0; };
	api.set_plugin_enabled = [](CL_PluginHandle, const wchar_t*, int32_t) -> int32_t { return 0; };
	api.has_app_command = [](CL_PluginHandle, const wchar_t*) -> int32_t { return 0; };
	api.invoke_app_command = [](CL_PluginHandle, const wchar_t*) -> int32_t { return 0; };
	api.register_app_command = [](CL_PluginHandle, const wchar_t*, CL_AppCommandFn, void*) -> int32_t { return 1; };
	api.unregister_app_command = [](CL_PluginHandle, const wchar_t*) {};
	api.traverse_files = [](CL_PluginHandle, const CL_TraverseOptions*, CL_FileVisitor, void*) {};
	api.search_files = [](CL_PluginHandle, const wchar_t*, uint32_t, CL_StrSink, void*) {};
	api.begin_drag = [](CL_PluginHandle, const CL_DragData* data, HWND) -> int32_t {
		if (!CL_HAS_FIELD(data, CL_DragData, bitmap)) return 0;
		g_mock.dragUrl = W(data->url);
		g_mock.dragHtml = W(data->html);
		g_mock.dragRtf = W(data->rtf);
		return 1;
	};
	api.load_res_icon_bitmap = [](CL_PluginHandle, int32_t, int32_t, int32_t) -> HBITMAP { return nullptr; };
	api.load_svg_bitmap = [](CL_PluginHandle, const wchar_t*, uint32_t, uint32_t) -> HBITMAP { return nullptr; };
	// 2.1
	api.set_clipboard_text = [](CL_PluginHandle, const wchar_t* text) -> int32_t {
		++g_mock.clipboardCount;
		g_mock.clipboardText = W(text);
		return 1;
	};
	// 2.2
	api.set_trigger_keywords = [](CL_PluginHandle, const wchar_t* const* keywords, size_t count, uint32_t flags) {
		g_mock.keywords.clear();
		for (size_t i = 0; keywords && i < count; ++i) g_mock.keywords.push_back(W(keywords[i]));
		g_mock.triggerFlags = flags;
		g_mock.keywordsSet = true;
	};
	api.sink_add_descs = [](CL_ResultSinkHandle, CL_PluginHandle, const CL_ActionDesc* items, size_t count) -> size_t {
		auto actions = ReadDescs(items, count);
		g_mock.sinkDescs.insert(g_mock.sinkDescs.end(), actions.begin(), actions.end());
		return count;
	};
	api.post_results = [](CL_PluginHandle, uint64_t queryId, const CL_ActionDesc* items, size_t count,
						int32_t) -> int32_t {
		if (queryId != g_mock.currentQueryId) return 0;
		g_mock.postedResults.emplace_back(queryId, ReadDescs(items, count));
		return 1;
	};
	api.notify_actions_changed = [](CL_PluginHandle) { ++g_mock.notifyCount; };
	api.get_current_query_id = [](CL_PluginHandle) -> uint64_t { return g_mock.currentQueryId; };
	api.sink_take_over = [](CL_ResultSinkHandle, CL_PluginHandle) { TestLog(L"[TakeOver]"); };
	api.request_refresh = [](CL_PluginHandle) { ++g_mock.notifyCount; };
	api.get_ui_language = [](CL_PluginHandle, CL_StrSink sink, void* ctx) {
		sink(ctx, g_mock.language.c_str(), g_mock.language.size());
	};
	api.get_string = [](CL_PluginHandle, const wchar_t* key, CL_StrSink sink, void* ctx) -> int32_t {
		const auto it = g_mock.strings.find(W(key));
		if (it == g_mock.strings.end()) return 0;
		sink(ctx, it->second.c_str(), it->second.size());
		return 1;
	};
	if (hostMinor == 0) {
		api.size = static_cast<uint32_t>(offsetof(CL_HostApi, set_clipboard_text));
	} else if (hostMinor == 1) {
		api.size = static_cast<uint32_t>(offsetof(CL_HostApi, set_trigger_keywords));
	} else if (hostMinor == 2) {
		api.size = static_cast<uint32_t>(offsetof(CL_HostApi, get_ui_language));
	}
	return api;
}

static bool Check(const bool condition, const std::wstring& what) {
	if (!condition) Loge(L"PluginTest", L"FAILED: ", what);
	return condition;
}

static const MockAction* FindMockAction(const uint32_t setId, const std::wstring& title) {
	const auto it = g_mock.sets.find(setId);
	if (it == g_mock.sets.end()) return nullptr;
	for (const auto& action : it->second) {
		if (action.title == title) return &action;
	}
	return nullptr;
}

static std::wstring GetItemCopyText(const CL_PluginApi* plugin, void* self, const uint64_t userData, bool& handled) {
	std::wstring text;
	handled = plugin->get_item_copy_text(self, userData, [](void* ctx, const wchar_t* str, const size_t len) {
		static_cast<std::wstring*>(ctx)->assign(str, len);
	}, &text) != 0;
	return text;
}

// 书签插件：用临时书签文件驱动全部入口，检查跨编译器 ABI 与 2.0/2.1 兼容
static bool TestBookmarkPlugin(const CL_PluginApi* plugin, void* self, const uint32_t hostMinor) {
	bool ok = true;
	const auto set = g_mock.sets.find(0);
	ok &= Check(set != g_mock.sets.end() && set->second.size() == 2, L"bookmark set 0 has 2 items");
	if (!ok) return false;
	ok &= Check(g_mock.setFlags[0] == 0, L"bookmark set not in global search when start_str is set");
	const MockAction* github = FindMockAction(0, L"GitHub 主页");
	ok &= Check(github != nullptr, L"utf-8 bookmark title converted");
	if (!github) return false;
	ok &= Check(github->subtitle == L"https://github.com/", L"bookmark subtitle is url");
	ok &= Check(github->matchText.find(L"https://github.com/") != std::wstring::npos, L"match text contains url");

	plugin->on_intercept_input(self, L"bm git", nullptr);
	ok &= Check(g_mock.sinkQueries.size() == 1 && g_mock.sinkQueries[0].first == 0 &&
				g_mock.sinkQueries[0].second == L"git", L"intercept input adds matches");
	plugin->on_intercept_input(self, L"git", nullptr);
	ok &= Check(g_mock.sinkQueries.size() == 1, L"input without start_str ignored");

	// 2.2：激活命令作为触发关键字注册，书签 URL 作为稳定标识；旧主程序上不调用 set_trigger_keywords
	if (hostMinor >= 2 && plugin->api_minor >= 2) {
		ok &= Check(g_mock.keywordsSet && g_mock.keywords.size() == 1 && g_mock.keywords[0] == L"bm ",
					L"start_str registered as trigger keyword");
		ok &= Check(github->stableKey == L"https://github.com/", L"stable_key is the url");
		ok &= Check(CL_HAS_FIELD(plugin, CL_PluginApi, flags) && (plugin->flags & CL_PLUGIN_REFRESH_ANY_THREAD),
					L"bookmark plugin refreshes on a worker");
	} else {
		ok &= Check(!g_mock.keywordsSet, L"no set_trigger_keywords call on an older host");
	}

	ok &= Check(plugin->on_action_execute(self, 0xFFFF000000000000ull, L"") == 0, L"unknown user_data rejected");
	ok &= Check(plugin->on_hotkey(self, github->userData, 'X', 0, 'X') == 0, L"unrelated hotkey ignored");

	ok &= Check(plugin->on_item_begin_drag(self, github->userData, nullptr, POINT{0, 0}) != 0, L"begin drag");
	ok &= Check(g_mock.dragUrl == L"https://github.com/", L"drag url");
	ok &= Check(g_mock.dragHtml.find(L"href=\"https://github.com/\"") != std::wstring::npos, L"drag html");
	if (plugin->api_minor >= 1) {
		ok &= Check(g_mock.dragRtf.rfind(LR"({\rtf1\ansi)", 0) == 0, L"drag rtf header");
	}

	// Ctrl+Shift+C 通过 2.1 的 set_clipboard_text 复制 markdown 链接，旧主程序上应返回未处理
	const bool expectCopyLink = plugin->api_minor >= 1 && hostMinor >= 1;
	const int32_t copied = plugin->on_hotkey(self, github->userData, 'C', MOD_CONTROL | MOD_SHIFT, 'C');
	ok &= Check((copied != 0) == expectCopyLink, L"Ctrl+Shift+C handled only on a 2.1 host");
	if (expectCopyLink) {
		ok &= Check(g_mock.clipboardText == L"[GitHub 主页](https://github.com/)", L"markdown link copied");
	} else {
		ok &= Check(g_mock.clipboardCount == 0, L"no clipboard call on a 2.0 host");
	}

	// 2.1 插件回调；旧插件的表里没有这个字段，主程序不会调用
	if (CL_HAS_FIELD(plugin, CL_PluginApi, get_item_copy_text) && plugin->get_item_copy_text) {
		bool handled = false;
		const std::wstring text = GetItemCopyText(plugin, self, github->userData, handled);
		ok &= Check(handled && text == L"https://github.com/", L"get_item_copy_text returns url");
		GetItemCopyText(plugin, self, 0xFFFF000000000000ull, handled);
		ok &= Check(!handled, L"get_item_copy_text rejects unknown user_data");
	} else {
		ok &= Check(plugin->api_minor == 0, L"2.1 plugin exposes get_item_copy_text");
		TestLog(L"get_item_copy_text not provided (api 2.0 plugin), host skips it");
	}

	// 刷新后旧 user_data 失效
	const uint64_t staleUserData = github->userData;
	plugin->refresh_actions(self);
	ok &= Check(plugin->on_item_begin_drag(self, staleUserData, nullptr, POINT{0, 0}) == 0, L"stale user_data rejected");
	return ok;
}

static std::filesystem::path WriteBookmarkFixture() {
	const std::filesystem::path path = std::filesystem::temp_directory_path() / L"CandyPluginTest_Bookmarks.json";
	std::ofstream out(path, std::ios::binary);
	out << u8R"({"roots":{"bookmark_bar":{"type":"folder","children":[
		{"type":"url","name":"GitHub 主页","url":"https://github.com/"},
		{"type":"folder","name":"dev","children":[{"type":"url","name":"CMake","url":"https://cmake.org/"}]}
	]},"other":{"type":"folder","children":[]}}})";
	return path;
}

int main(int argc, char** argv) {
	// 不要设置这两个，宽窄字符输出混用的情况下都会造成崩溃，测试不通过
	// _setmode(_fileno(stdout), _O_U8TEXT);
	// _setmode(_fileno(stdout), _O_WTEXT);

	// 设置这两行即可，全流程使用窄字符流
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	TestLog(L"Plugin Test");
	TestLog(L"=========================");

	std::wstring dllPath;
	uint32_t hostMinor = CL_API_VERSION_MINOR;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg.rfind("--host-minor=", 0) == 0) hostMinor = static_cast<uint32_t>(std::stoul(arg.substr(13)));
		else dllPath = utf8_to_wide(arg);
	}

	const CL_PluginApi* plugin = nullptr;
	HMODULE module = nullptr;
	if (dllPath.empty()) {
		plugin = CandyPlugin_GetApi();
	} else {
		module = LoadLibraryW(dllPath.c_str());
		if (!Check(module != nullptr, L"LoadLibraryW " + dllPath + L" error " + std::to_wstring(GetLastError()))) return 1;
		const auto getApi = reinterpret_cast<CL_GetPluginApiFn>(GetProcAddress(module, CL_PLUGIN_ENTRY_NAME));
		if (!Check(getApi != nullptr, L"CandyPlugin_GetApi export")) return 1;
		plugin = getApi();
	}
	TestLog(L"Host api 2." + std::to_wstring(hostMinor) + (dllPath.empty() ? L"" : L", plugin dll: " + dllPath));
	if (!Check(plugin != nullptr, L"CandyPlugin_GetApi returned null")) return 1;
	if (!Check(plugin->api_major == CL_API_VERSION_MAJOR, L"api major")) return 1;
	// 旧插件的表可以比当前定义小，但至少要包含 2.0 的全部字段
	if (!Check(CL_HAS_FIELD(plugin, CL_PluginApi, on_setting_item_execute), L"api table size")) return 1;
	if (!Check(plugin->create && plugin->destroy && plugin->package_name, L"required entries")) return 1;

	// 输出插件信息
	TestLog(L"Plugin Name: " + W(plugin->name));
	TestLog(L"Plugin Version: " + W(plugin->version));
	TestLog(L"Plugin Api: " + std::to_wstring(plugin->api_major) + L"." + std::to_wstring(plugin->api_minor) +
		L", table size " + std::to_wstring(plugin->size));
	TestLog(L"Plugin Package: " + W(plugin->package_name));
	TestLog(L"Description: " + W(plugin->description));
	TestLog(L"=========================");

	// 默认设置：优先 DLL 表里的 default_setting_json，否则读 DLL 旁边的 <名>.plugin.json 清单
	// （内置编译的示例插件用 PLUGIN_TEST_EXAMPLE_MANIFEST 指定清单路径）
	std::string settingJsonText = plugin->default_setting_json ? wide_to_utf8(plugin->default_setting_json) : "";
	if (settingJsonText.empty()) {
		std::filesystem::path manifestPath;
		if (!dllPath.empty()) manifestPath = std::filesystem::path(dllPath).replace_extension().wstring() + L".plugin.json";
#ifdef PLUGIN_TEST_EXAMPLE_MANIFEST
		else manifestPath = PLUGIN_TEST_EXAMPLE_MANIFEST;
#endif
		std::ifstream manifestFile(manifestPath, std::ios::binary);
		if (manifestFile) {
			try {
				const nlohmann::json manifest = nlohmann::json::parse(std::string(std::istreambuf_iterator<char>(manifestFile), {}),
																	nullptr, true, true);
				if (manifest.contains("default_setting_json")) {
					const auto& settings = manifest.at("default_setting_json");
					settingJsonText = settings.is_string() ? settings.get<std::string>() : settings.dump();
				}
				TestLog(L"Manifest: " + manifestPath.wstring());
			} catch (const std::exception& e) {
				Loge(L"PluginTest", L"Bad manifest ", manifestPath.wstring(), L": ", e.what());
			}
		}
	}
	if (!settingJsonText.empty()) {
		std::vector<SettingItem> settings = ParseConfig(settingJsonText, 0);
		std::function<void(std::vector<SettingItem>&)> loadSettingsToMap = [&](std::vector<SettingItem>& items) {
			for (auto& item : items) {
				if (EndsWith(item.key, ".start_str")) {
					item.stringValue = "";
				}
				g_mock.settingsMap[item.key] = item;
				if ((item.type == "expand" || item.type == "expandswitch") && !item.children.empty()) {
					loadSettingsToMap(item.children);
				}
			}
		};
		loadSettingsToMap(settings);
	}
	TestLog(L"Setting items size: " + std::to_wstring(g_mock.settingsMap.size()));

	const bool isBookmark = W(plugin->package_name) == L"com.candytek.bookmarkplugin";
	if (isBookmark) {
		g_mock.settingsMap["com.candytek.bookmarkplugin.start_str"].stringValue = "bm ";
		g_mock.settingsMap["com.candytek.bookmarkplugin.browser_list"].stringArr = {WriteBookmarkFixture().u8string()};
	}

	static const CL_HostApi hostApi = MakeMockHostApi(hostMinor);
	const auto handle = reinterpret_cast<CL_PluginHandle>(&g_mock);
	void* self = plugin->create(handle, &hostApi);
	if (!Check(self != nullptr, L"create")) return 1;

	bool ok = true;
	if (plugin->on_user_settings_loaded) plugin->on_user_settings_loaded(self);

	TestLog(L"Refreshing actions...");
	if (plugin->refresh_actions) plugin->refresh_actions(self);

	size_t total = 0;
	for (const auto& [setId, actions] : g_mock.sets) {
		total += actions.size();
		const size_t displayCount = std::min<size_t>(5, actions.size());
		TestLog(L"Set " + std::to_wstring(setId) + L": " + std::to_wstring(actions.size()) + L" actions");
		for (size_t i = 0; i < displayCount; ++i) {
			TestLog(L"  [" + std::to_wstring(i + 1) + L"] " + actions[i].title);
			TestLog(L"      " + actions[i].subtitle);
		}
	}
	TestLog(L"Total actions loaded: " + std::to_wstring(total));

	// 示例插件的额外检查：动态 action、执行、过期 user_data 被拒绝
	if (W(plugin->package_name) == L"com.example.plugin") {
		ok &= Check(total == 3, L"example static actions");

		// 没有字符串表时使用代码里的默认（英文）文本
		plugin->on_user_input(self, L"test");
		const MockAction* testAction = FindMockAction(1, L"Test feature");
		ok &= Check(testAction != nullptr, L"dynamic action published");
		if (testAction) {
			const uint64_t staleUserData = testAction->userData;
			const int before = g_mock.messageCount;
			ok &= Check(plugin->on_action_execute(self, staleUserData, L"") != 0, L"execute dynamic action");
			ok &= Check(g_mock.messageCount == before + 1, L"dynamic action shows message");

			plugin->on_user_input(self, L"calc");
			ok &= Check(FindMockAction(1, L"Calculator") != nullptr, L"dynamic set replaced");
			ok &= Check(plugin->on_action_execute(self, staleUserData, L"") == 0, L"stale user_data rejected");
		}

		const MockAction* hello = FindMockAction(0, L"Hello World");
		ok &= Check(hello != nullptr, L"static action published");
		if (hello) {
			ok &= Check(plugin->on_action_execute(self, hello->userData, L"") != 0, L"execute static action");
			ok &= Check(hello->stableKey == L"ExamplePlugin_hello", L"stable_key published");
		}
		const MockAction* date = FindMockAction(0, L"Show Date");
		ok &= Check(date && date->iconKey == L"clock", L"icon_key published");

		// 2.3 本地化：主程序提供字符串表后 Tr 返回翻译，缺失的 key 仍回退到代码里的默认文本；
		// 旧主程序（表里没有 get_string）上 Tr 始终返回默认文本
		const bool host23 = hostMinor >= 3;
		g_mock.strings[L"action.test.title"] = L"测试功能";
		g_mock.strings[L"action.hello.title"] = L"你好世界";
		g_mock.language = L"zh-CN";
		if (CL_HAS_FIELD(plugin, CL_PluginApi, on_language_changed) && plugin->on_language_changed) {
			plugin->on_language_changed(self, L"zh-CN");
		}
		plugin->refresh_actions(self);
		plugin->on_user_input(self, L"test");
		ok &= Check((FindMockAction(1, L"测试功能") != nullptr) == host23, L"translated title only on a 2.3 host");
		ok &= Check((FindMockAction(0, L"你好世界") != nullptr) == host23, L"translated static title only on a 2.3 host");
		ok &= Check(FindMockAction(0, L"Show Time") != nullptr, L"untranslated key falls back to the default text");
		g_mock.strings.clear();
		g_mock.language = L"en-US";
		plugin->refresh_actions(self);
		plugin->on_user_input(self, L"calc");

		// 2.2 机制；在 2.0 / 2.1 主程序上 SDK 必须静默退化
		const bool host22 = hostMinor >= 2;
		ok &= Check(g_mock.keywordsSet == host22, L"set_trigger_keywords only on a 2.2 host");
		if (host22) {
			ok &= Check(g_mock.keywords.size() == 1 && g_mock.keywords[0] == L"ex " &&
						g_mock.triggerFlags == CL_TRIGGER_ALSO_GLOBAL, L"example trigger keywords");
		}

		// 临时结果：不进入集合，但执行时插件仍能找到
		plugin->on_intercept_input(self, L"ex hello", nullptr);
		ok &= Check(g_mock.sinkDescs.size() == (host22 ? 1u : 0u), L"sink_add_descs only on a 2.2 host");
		if (host22) {
			ok &= Check(g_mock.sinkDescs[0].title == L"Echo: hello", L"transient result title");
			const int before = g_mock.messageCount;
			ok &= Check(plugin->on_action_execute(self, g_mock.sinkDescs[0].userData, L"") != 0, L"execute transient");
			ok &= Check(g_mock.messageCount == before + 1, L"transient action executed");
			const uint64_t stale = g_mock.sinkDescs[0].userData;
			plugin->on_intercept_input(self, L"ex again", nullptr);
			ok &= Check(plugin->on_action_execute(self, stale, L"") == 0, L"transient result of previous input is stale");
		}

		// 异步查询：当前查询 id 的结果被接受，过期 id 的被拒绝
		if (CL_HAS_FIELD(plugin, CL_PluginApi, on_query) && plugin->on_query) {
			g_mock.currentQueryId = 7;
			plugin->on_query(self, 7, L"async", L"");
			ok &= Check(g_mock.postedResults.size() == (host22 ? 1u : 0u), L"post_results only on a 2.2 host");
			if (host22) {
				ok &= Check(g_mock.postedResults[0].second.size() == 1 &&
							g_mock.postedResults[0].second[0].subtitle == L"query 7", L"async result posted");
				ok &= Check(plugin->on_action_execute(self, g_mock.postedResults[0].second[0].userData, L"") != 0,
							L"execute async result");
				g_mock.currentQueryId = 8;
				plugin->on_query(self, 7, L"async", L"");
				ok &= Check(g_mock.postedResults.size() == 1, L"stale query results rejected by the host");
			}
			plugin->on_query(self, 8, L"nothing", L"");
			plugin->on_query_cancel(self, 8);
		}
		if (CL_HAS_FIELD(plugin, CL_PluginApi, on_setting_changed) && plugin->on_setting_changed) {
			plugin->on_setting_changed(self, L"example_key_3");
		}
	}

	if (isBookmark) {
		ok &= TestBookmarkPlugin(plugin, self, hostMinor);
	}

	plugin->destroy(self);
	if (module) FreeLibrary(module);

	if (!ok) {
		Loge(L"PluginTest", L"Test failed");
		return 1;
	}
	TestLog(L"\n✅✅ Test completed successfully!");
	return 0;
}
