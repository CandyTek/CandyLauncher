#pragma once
// Host implementation of CL_HostApi (plugins/sdk/CandyPluginApi.h). Included at the end of PluginManager.hpp.
// Every function is a C entry point called from plugin DLLs: no exception may escape.

#include <cstring>
#include <shared_mutex>
#include <unordered_map>

#include "util/ClipboardUtil.hpp"

namespace PluginHostApiImpl {

inline std::wstring W(const wchar_t* value) {
	return value ? std::wstring(value) : std::wstring();
}

inline void Emit(const CL_StrSink sink, void* ctx, const std::wstring& value) {
	if (sink) sink(ctx, value.c_str(), value.size());
}

template <class R, class Fn>
R Guard(const wchar_t* name, R fallback, Fn&& fn) {
	try {
		return fn();
	} catch (const std::exception& e) {
		Loge(TAG, L"Host api ", name, L" exception: ", e.what());
	} catch (...) {
		Loge(TAG, L"Host api ", name, L" unknown exception");
	}
	return fallback;
}

template <class Fn>
void GuardVoid(const wchar_t* name, Fn&& fn) {
	Guard(name, 0, [&] {
		fn();
		return 0;
	});
}

inline bool IsUiThread() {
	return g_mainHwnd && GetWindowThreadProcessId(g_mainHwnd, nullptr) == GetCurrentThreadId();
}

inline std::shared_ptr<const PluginActionSet> GetSet(CL_PluginContext* h, const uint32_t setId) {
	std::lock_guard<std::mutex> lock(h->mutex);
	const auto it = h->sets.find(setId);
	return it != h->sets.end() ? it->second : nullptr;
}

inline const SettingItem* FindSetting(const wchar_t* key, std::initializer_list<const char*> types) {
	if (!key) return nullptr;
	const auto it = g_settings_map.find(wide_to_utf8(key));
	if (it == g_settings_map.end()) return nullptr;
	for (const char* type : types) {
		if (it->second.type == type) return &it->second;
	}
	return nullptr;
}

// g_matchTextCache (manager/MatchTextBuilder.hpp) keeps the lower case + pinyin result per source string.

// Reads item i of a CL_ActionDesc array published by a plugin built against any 2.x header:
// the plugin's own struct size is the stride, missing trailing fields read as zero.
struct DescReader {
	const char* base = nullptr;
	size_t stride = 0;

	CL_ActionDesc operator()(const size_t i) const {
		CL_ActionDesc desc{};
		std::memcpy(&desc, base + i * stride, (std::min)(stride, sizeof(desc)));
		return desc;
	}
};

inline bool ValidateDescs(CL_PluginContext* h, const CL_ActionDesc* items, const size_t count, DescReader& reader) {
	if (count > 0 && !items) return false;
	reader.base = reinterpret_cast<const char*>(items);
	reader.stride = count > 0 ? items[0].size : 0;
	if (count > 0 && reader.stride < offsetof(CL_ActionDesc, icon_bitmap) + sizeof(HBITMAP)) {
		Loge(h->logTag, L"CL_ActionDesc too small: ", reader.stride);
		return false;
	}
	return true;
}

// Copies plugin descriptors into host-owned proxies. `processMatchText` is false for items that are
// never matched (intercept / query results), which skips the pinyin step entirely.
inline std::vector<std::shared_ptr<BaseAction>> BuildProxies(CL_PluginContext* h, const CL_ActionDesc* items,
															const size_t count, const bool processMatchText) {
	DescReader readDesc;
	std::vector<std::shared_ptr<BaseAction>> actions;
	if (!ValidateDescs(h, items, count, readDesc)) return actions;

	// 同一路径的图标只解析一次，由本次发布的所有条目共享；icon_key 相同的位图只复制一次
	std::unordered_map<std::wstring, std::shared_ptr<LazySysImageIndex>> lazyIcons;
	std::unordered_map<std::wstring, std::shared_ptr<SharedIconBitmap>> sharedIcons;
	{
		std::lock_guard<std::mutex> lock(h->mutex);
		for (auto it = h->iconCache.begin(); it != h->iconCache.end();) {
			if (auto alive = it->second.lock()) {
				sharedIcons.emplace(it->first, std::move(alive));
				++it;
			} else {
				it = h->iconCache.erase(it);
			}
		}
	}
	for (size_t i = 0; i < count; ++i) {
		const CL_ActionDesc desc = readDesc(i);
		if (desc.icon_index < 0 && desc.icon_path && *desc.icon_path && !lazyIcons.count(desc.icon_path)) {
			lazyIcons.emplace(desc.icon_path, std::make_shared<LazySysImageIndex>(desc.icon_path));
		}
		if (desc.icon_key && *desc.icon_key && desc.icon_bitmap && !sharedIcons.count(desc.icon_key)) {
			const auto copy = static_cast<HBITMAP>(CopyImage(desc.icon_bitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
			if (copy) sharedIcons.emplace(desc.icon_key, std::make_shared<SharedIconBitmap>(copy));
		}
	}
	{
		std::lock_guard<std::mutex> lock(h->mutex);
		for (const auto& [key, icon] : sharedIcons) h->iconCache[key] = icon;
	}

	const uint16_t pluginId = h->pluginId;
	const uint64_t serial = h->serial;
	const int32_t priority = h->priority.load();
	actions.resize(count);
	ParallelForRange(count, 256, [&](const size_t begin, const size_t end) {
		for (size_t i = begin; i < end; ++i) {
			const CL_ActionDesc desc = readDesc(i);
			auto proxy = std::make_shared<PluginActionProxy>();
			proxy->pluginId = pluginId;
			proxy->pluginPriority = priority;
			proxy->ownerSerial = serial;
			proxy->userData = desc.user_data;
			proxy->title = W(desc.title);
			proxy->subtitle = W(desc.subtitle);
			proxy->stableKey = W(desc.stable_key);
			if (!processMatchText) {
				proxy->matchText = desc.match_text ? std::wstring(desc.match_text) : proxy->title;
				proxy->matchTitle = MyToLower(proxy->title);
			} else if (desc.match_text && (desc.flags & CL_ACTION_MATCH_TEXT_VERBATIM)) {
				// 插件自己拼好了 matchText（通常已经过 process_match_text，标题的拼音在缓存里）；
				// 结构化字段仍按标题取，这样标题命中不会被路径等附加内容稀释
				proxy->matchText = desc.match_text;
				g_matchTextCache.FillFields(*proxy, proxy->title);
			} else if (!desc.match_text || proxy->title == desc.match_text) {
				g_matchTextCache.FillAll(*proxy, proxy->title);
			} else {
				proxy->matchText = g_matchTextCache.Get(desc.match_text)->text;
				g_matchTextCache.FillFields(*proxy, proxy->title);
			}
			proxy->usageKey = ComputeUsageKey(h->pkgName, proxy->stableKey, proxy->title, proxy->subtitle);
			proxy->iconIndex = desc.icon_index;
			if (desc.icon_index < 0 && desc.icon_path && *desc.icon_path) {
				proxy->lazyIcon = lazyIcons.at(desc.icon_path);
			}
			if (desc.icon_key && *desc.icon_key) {
				const auto shared = sharedIcons.find(desc.icon_key);
				if (shared != sharedIcons.end()) proxy->iconBitmap = shared->second;
			} else if (desc.icon_bitmap) {
				const auto copy = static_cast<HBITMAP>(CopyImage(desc.icon_bitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
				if (copy) proxy->iconBitmap = std::make_shared<SharedIconBitmap>(copy);
			}
			actions[i] = std::move(proxy);
		}
	});
	return actions;
}

// ---- logging / UI ----

inline void CL_CALL Log(CL_PluginHandle h, const int32_t level, const wchar_t* message) {
	GuardVoid(L"log", [&] {
		const std::wstring& tag = h ? h->logTag : TAG;
		if (level >= CL_LOG_ERROR) Loge(tag, W(message));
		else if (level == CL_LOG_WARN) Logw(tag, W(message));
		else Logi(tag, W(message));
	});
}

inline void CL_CALL ShowMessage(CL_PluginHandle, const wchar_t* title, const wchar_t* message) {
	GuardVoid(L"show_message", [&] {
		MessageBoxW(nullptr, W(message).c_str(), W(title).c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	});
}

inline void CL_CALL ShowToast(CL_PluginHandle, const wchar_t* title, const wchar_t* message) {
	GuardVoid(L"show_toast", [&] {
#if !defined(__MINGW32__)
		MyShowSimpleToast(W(title), W(message));
#endif
	});
}

inline int32_t CL_CALL ShowMainWindow(CL_PluginHandle, const int32_t cmdShow, const int32_t foreground) {
	return Guard(L"show_main_window", 0, [&] {
		ShowWindow(g_mainHwnd, cmdShow);
		if (foreground) SetForegroundWindow(g_mainHwnd);
		return 1;
	});
}

inline void CL_CALL TaskDone(CL_PluginHandle) {
	GuardVoid(L"task_done", [&] {
		if (pref_close_after_open_item) HideWindow();
	});
}

inline void CL_CALL SetEditText(CL_PluginHandle, const wchar_t* text) {
	GuardVoid(L"set_edit_text", [&] {
		SetWindowTextW(g_editHwnd, W(text).c_str());
		// 将光标移到文本末尾
		const int textLength = GetWindowTextLengthW(g_editHwnd);
		SendMessageW(g_editHwnd, EM_SETSEL, (WPARAM)textLength, (LPARAM)textLength);
		SendMessageW(g_editHwnd, EM_SCROLLCARET, 0, 0);
	});
}

inline void CL_CALL GetEditText(CL_PluginHandle, const CL_StrSink sink, void* ctx) {
	GuardVoid(L"get_edit_text", [&] { Emit(sink, ctx, editTextBuffer); });
}

inline void CL_CALL GetActionArg(CL_PluginHandle, const CL_StrSink sink, void* ctx) {
	GuardVoid(L"get_action_arg", [&] { Emit(sink, ctx, currectActionArg); });
}

inline void CL_CALL ProcessMatchText(CL_PluginHandle, const wchar_t* source, const CL_StrSink sink, void* ctx) {
	GuardVoid(L"process_match_text", [&] {
		Emit(sink, ctx, g_matchTextCache.Get(W(source))->text);
	});
}

// ---- action sets ----

inline void CL_CALL SetActions(CL_PluginHandle h, const uint32_t setId, const CL_ActionDesc* items, const size_t count,
								const uint32_t setFlags) {
	GuardVoid(L"set_actions", [&] {
		if (!h || (count > 0 && !items)) return;
		auto set = std::make_shared<PluginActionSet>();
		set->flags = setFlags;
		set->actions = BuildProxies(h, items, count, true);
		set->byUserData.reserve(set->actions.size());
		for (size_t i = 0; i < set->actions.size(); ++i) {
			set->byUserData.emplace(static_cast<const PluginActionProxy*>(set->actions[i].get())->userData, i);
		}
		{
			std::lock_guard<std::mutex> lock(h->mutex);
			h->sets[setId] = std::move(set);
		}
		PluginManager::BumpActionsVersion();
	});
}

inline void CL_CALL SetActionSetFlags(CL_PluginHandle h, const uint32_t setId, const uint32_t setFlags) {
	GuardVoid(L"set_action_set_flags", [&] {
		if (!h) return;
		std::lock_guard<std::mutex> lock(h->mutex);
		auto& slot = h->sets[setId];
		// Sets are immutable snapshots: replace instead of mutating what readers may hold.
		auto set = slot ? std::make_shared<PluginActionSet>(*slot) : std::make_shared<PluginActionSet>();
		set->flags = setFlags;
		slot = std::move(set);
		PluginManager::BumpActionsVersion();
	});
}

inline void CL_CALL ClearActions(CL_PluginHandle h, const uint32_t setId) {
	GuardVoid(L"clear_actions", [&] {
		if (!h) return;
		{
			std::lock_guard<std::mutex> lock(h->mutex);
			h->sets.erase(setId);
		}
		PluginManager::BumpActionsVersion();
	});
}

inline void CL_CALL ShowActionSet(CL_PluginHandle h, const uint32_t setId) {
	GuardVoid(L"show_action_set", [&] {
		if (!h || !g_pluginManager) return;
		if (!IsUiThread()) {
			Loge(h->logTag, L"show_action_set must be called on the UI thread");
			return;
		}
		const auto set = GetSet(h, setId);
		g_pluginManager->ShowResultsDerectly(set ? set->actions : std::vector<std::shared_ptr<BaseAction>>{});
	});
}

inline size_t CL_CALL SinkAddMatches(CL_ResultSinkHandle sink, CL_PluginHandle h, const uint32_t setId,
									const wchar_t* query) {
	return Guard(L"sink_add_matches", size_t{0}, [&] {
		if (!sink || !h) return size_t{0};
		const auto set = GetSet(h, setId);
		if (!set) return size_t{0};
		const size_t before = sink->actions.size();
		if (!query || !*query) {
			sink->actions.insert(sink->actions.end(), set->actions.begin(), set->actions.end());
		} else {
			std::vector<std::shared_ptr<BaseAction>> matched;
			TextMatch(query, set->actions, matched);
			sink->actions.insert(sink->actions.end(), matched.begin(), matched.end());
		}
		return sink->actions.size() - before;
	});
}

inline size_t CL_CALL SinkAddItems(CL_ResultSinkHandle sink, CL_PluginHandle h, const uint32_t setId,
									const uint64_t* userData, const size_t count) {
	return Guard(L"sink_add_items", size_t{0}, [&] {
		if (!sink || !h) return size_t{0};
		const auto set = GetSet(h, setId);
		if (!set) return size_t{0};
		const size_t before = sink->actions.size();
		if (!userData || count == 0) {
			sink->actions.insert(sink->actions.end(), set->actions.begin(), set->actions.end());
			return sink->actions.size() - before;
		}
		for (size_t i = 0; i < count; ++i) {
			if (const auto it = set->byUserData.find(userData[i]); it != set->byUserData.end()) {
				sink->actions.push_back(set->actions[it->second]);
			}
		}
		return sink->actions.size() - before;
	});
}

// ---- settings ----

inline int32_t CL_CALL GetSettingBool(CL_PluginHandle, const wchar_t* key, int32_t* out) {
	return Guard(L"get_setting_bool", 0, [&] {
		std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
		const SettingItem* item = FindSetting(key, {"bool", "expandswitch"});
		if (!item || !out) return 0;
		*out = item->boolValue ? 1 : 0;
		return 1;
	});
}

inline int32_t CL_CALL GetSettingInt(CL_PluginHandle, const wchar_t* key, int64_t* out) {
	return Guard(L"get_setting_int", 0, [&] {
		std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
		const SettingItem* item = FindSetting(key, {"long"});
		if (!item || !out) return 0;
		*out = item->intValue;
		return 1;
	});
}

inline int32_t CL_CALL GetSettingDouble(CL_PluginHandle, const wchar_t* key, double* out) {
	return Guard(L"get_setting_double", 0, [&] {
		std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
		const SettingItem* item = FindSetting(key, {"double", "long"});
		if (!item || !out) return 0;
		*out = item->type == "long" ? static_cast<double>(item->intValue) : item->doubleValue;
		return 1;
	});
}

inline int32_t CL_CALL GetSettingString(CL_PluginHandle, const wchar_t* key, const CL_StrSink sink, void* ctx) {
	return Guard(L"get_setting_string", 0, [&] {
		std::wstring value;
		{
			std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
			const SettingItem* item = FindSetting(key, {"string", "hotkeystring", "list"});
			if (!item) return 0;
			value = utf8_to_wide(item->stringValue);
		}
		Emit(sink, ctx, value);
		return 1;
	});
}

// ---- 2.3 localization (PluginI18n.hpp). Both any thread: the language is a mutex guarded mirror and the
// resolved table is an immutable shared_ptr swapped atomically by PluginManager. ----

inline void CL_CALL GetUiLanguage(CL_PluginHandle, const CL_StrSink sink, void* ctx) {
	GuardVoid(L"get_ui_language", [&] { Emit(sink, ctx, PluginI18n::Language()); });
}

inline int32_t CL_CALL GetString(CL_PluginHandle h, const wchar_t* key, const CL_StrSink sink, void* ctx) {
	return Guard(L"get_string", 0, [&] {
		if (!h || !key) return 0;
		const auto strings = std::atomic_load(&h->strings);
		if (!strings) return 0;
		const auto it = strings->find(key);
		if (it == strings->end()) return 0;
		Emit(sink, ctx, it->second);
		return 1;
	});
}

inline int32_t CL_CALL GetSettingStringArray(CL_PluginHandle, const wchar_t* key, const CL_StrSink sink, void* ctx) {
	return Guard(L"get_setting_string_array", 0, [&] {
		std::vector<std::string> values;
		{
			std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
			const SettingItem* item = FindSetting(key, {"stringArr"});
			if (!item) return 0;
			values = item->stringArr;
		}
		for (const auto& value : values) {
			Emit(sink, ctx, utf8_to_wide(value));
		}
		return 1;
	});
}

inline int32_t CL_CALL SetSettingJson(CL_PluginHandle h, const wchar_t* key, const wchar_t* valueJson) {
	return Guard(L"set_setting_json", 0, [&] {
		if (!h || !key || !valueJson || !g_pluginManager) return 0;
		const nlohmann::json value = nlohmann::json::parse(wide_to_utf8(valueJson));
		return g_pluginManager->SetPluginSettingValue(h->pluginId, wide_to_utf8(key), value) ? 1 : 0;
	});
}

// ---- plugins ----

inline int32_t CL_CALL IsPluginEnabled(CL_PluginHandle, const wchar_t* packageName) {
	return Guard(L"is_plugin_enabled", 0, [&] {
		return g_pluginManager && g_pluginManager->IsPluginEnabledByPackageName(W(packageName)) ? 1 : 0;
	});
}

inline int32_t CL_CALL EnumPluginCatalog(CL_PluginHandle h, const CL_CatalogVisitor visitor, void* ctx) {
	return Guard(L"enum_plugin_catalog", 0, [&] {
		if (!h || !visitor || !g_pluginManager || !g_pluginManager->CanUseRestrictedPluginBrowserApi(h->pluginId)) {
			return 0;
		}
		const std::vector<PluginCatalogInfo> catalog = g_pluginCatalog;
		for (const auto& info : catalog) {
			// 2.3: name / description in the current UI language ("plugin.name" / "plugin.description" keys).
			const std::wstring name = info.DisplayName();
			const std::wstring description = info.DisplayDescription();
			CL_CatalogEntry entry{};
			entry.size = sizeof(entry);
			entry.name = name.c_str();
			entry.version = info.version.c_str();
			entry.package_name = info.pkgName.c_str();
			entry.file_path = info.filePath.c_str();
			entry.default_setting_json = info.defaultSettingJson.c_str();
			entry.enabled = info.enabled ? 1 : 0;
			entry.description = description.c_str();
			visitor(ctx, &entry);
		}
		return 1;
	});
}

inline int32_t CL_CALL SetPluginEnabled(CL_PluginHandle h, const wchar_t* packageName, const int32_t enabled) {
	return Guard(L"set_plugin_enabled", 0, [&] {
		if (!h || !g_pluginManager) return 0;
		return g_pluginManager->SetPluginEnabled(h->pluginId, W(packageName), enabled != 0) ? 1 : 0;
	});
}

// ---- app commands ----

inline int32_t CL_CALL HasAppCommand(CL_PluginHandle, const wchar_t* key) {
	return Guard(L"has_app_command", 0, [&] {
		return key && appLaunchActionCallBacks.count(wide_to_utf8(key)) ? 1 : 0;
	});
}

inline int32_t CL_CALL InvokeAppCommand(CL_PluginHandle, const wchar_t* key) {
	return Guard(L"invoke_app_command", 0, [&] {
		if (!key) return 0;
		const auto it = appLaunchActionCallBacks.find(wide_to_utf8(key));
		if (it == appLaunchActionCallBacks.end() || !it->second) return 0;
		// 回调可能修改回调表，先拷贝
		const std::function<void()> callback = it->second;
		callback();
		return 1;
	});
}

inline int32_t CL_CALL RegisterAppCommand(CL_PluginHandle h, const wchar_t* key, const CL_AppCommandFn fn, void* ctx) {
	return Guard(L"register_app_command", 0, [&] {
		if (!h || !key || !fn) return 0;
		const std::string utf8Key = wide_to_utf8(key);
		appLaunchActionCallBacks[utf8Key] = [fn, ctx]() { fn(ctx); };
		if (std::find(h->appCommands.begin(), h->appCommands.end(), utf8Key) == h->appCommands.end()) {
			h->appCommands.push_back(utf8Key);
		}
		return 1;
	});
}

inline void CL_CALL UnregisterAppCommand(CL_PluginHandle h, const wchar_t* key) {
	GuardVoid(L"unregister_app_command", [&] {
		if (!h || !key) return;
		const std::string utf8Key = wide_to_utf8(key);
		const auto owned = std::find(h->appCommands.begin(), h->appCommands.end(), utf8Key);
		if (owned == h->appCommands.end()) return; // 只能注销自己注册的命令
		h->appCommands.erase(owned);
		appLaunchActionCallBacks.erase(utf8Key);
	});
}

// ---- files ----

inline void CL_CALL TraverseFilesApi(CL_PluginHandle, const CL_TraverseOptions* raw, const CL_FileVisitor visitor,
									void* ctx) {
	GuardVoid(L"traverse_files", [&] {
		if (!raw || !visitor || !raw->folder || !g_pluginManager ||
			!CL_HAS_FIELD(raw, CL_TraverseOptions, index_files_only)) {
			return;
		}
		auto toVector = [](const wchar_t* const* values, const size_t count) {
			std::vector<std::wstring> result;
			if (!values) return result;
			result.reserve(count);
			for (size_t i = 0; i < count; ++i) result.push_back(W(values[i]));
			return result;
		};
		TraverseOptions options;
		options.folder = raw->folder;
		options.extensions = toVector(raw->extensions, raw->extension_count);
		options.excludeNames = toVector(raw->exclude_names, raw->exclude_name_count);
		options.excludeWords = toVector(raw->exclude_words, raw->exclude_word_count);
		options.renameSources = toVector(raw->rename_sources, raw->rename_count);
		options.renameTargets = toVector(raw->rename_targets, raw->rename_count);
		for (size_t i = 0; i < options.renameSources.size() && i < options.renameTargets.size(); ++i) {
			options.renameMap[options.renameSources[i]] = options.renameTargets[i];
		}
		options.recursive = raw->recursive != 0;
		options.indexFilesOnly = raw->index_files_only != 0;
		g_pluginManager->TraverseFilesForEverythingSDK(options.folder, options,
			[visitor, ctx](const std::wstring& name, const std::wstring& fullPath, const std::wstring& parent,
							const std::wstring& ext) {
				visitor(ctx, name.c_str(), fullPath.c_str(), parent.c_str(), ext.c_str());
			});
	});
}

inline void CL_CALL SearchFiles(CL_PluginHandle, const wchar_t* query, const uint32_t maxResults, const CL_StrSink sink,
								void* ctx) {
	GuardVoid(L"search_files", [&] {
		if (!query || !g_pluginManager) return;
		for (const auto& file : g_pluginManager->SearchPossibleTargets(query, maxResults)) {
			Emit(sink, ctx, file.file_path.wstring());
		}
	});
}

// ---- drag & drop ----

inline int32_t CL_CALL BeginDrag(CL_PluginHandle, const CL_DragData* raw, const HWND sourceHwnd) {
	return Guard(L"begin_drag", 0, [&] {
		if (!raw || !g_pluginManager || !CL_HAS_FIELD(raw, CL_DragData, bitmap)) return 0;
		OleDragDropData data;
		if (raw->file_paths) {
			for (size_t i = 0; i < raw->file_count; ++i) data.filePaths.push_back(W(raw->file_paths[i]));
		}
		data.text = W(raw->text);
		data.url = W(raw->url);
		data.html = W(raw->html);
		data.rtf = W(raw->rtf);
		data.bitmap = raw->bitmap;
		return g_pluginManager->BeginOleDragDropData(data, sourceHwnd) ? 1 : 0;
	});
}

// ---- images ----

inline HBITMAP CL_CALL LoadResIconBitmap(CL_PluginHandle, const int32_t resId, const int32_t cx, const int32_t cy) {
	return Guard(L"load_res_icon_bitmap", static_cast<HBITMAP>(nullptr), [&] {
		return g_pluginManager ? g_pluginManager->LoadResIconAsBitmap(resId, cx, cy) : nullptr;
	});
}

inline HBITMAP CL_CALL LoadSvgBitmap(CL_PluginHandle, const wchar_t* path, const uint32_t width, const uint32_t height) {
	return Guard(L"load_svg_bitmap", static_cast<HBITMAP>(nullptr), [&] {
		return path ? MyLoadSvgAsHBITMAP(path, width, height) : nullptr;
	});
}

// ---- added in 2.1 ----

inline int32_t CL_CALL SetClipboardTextApi(CL_PluginHandle, const wchar_t* text) {
	return Guard(L"set_clipboard_text", 0, [&] {
		return CopyTextToClipboard(g_mainHwnd, W(text)) ? 1 : 0;
	});
}

// ---- added in 2.2 ----

inline void CL_CALL SetTriggerKeywords(CL_PluginHandle h, const wchar_t* const* keywords, const size_t count,
										const uint32_t flags) {
	GuardVoid(L"set_trigger_keywords", [&] {
		if (!h) return;
		std::vector<std::wstring> lowered;
		for (size_t i = 0; keywords && i < count; ++i) {
			std::wstring keyword = MyToLower(W(keywords[i]));
			if (!keyword.empty()) lowered.push_back(std::move(keyword));
		}
		std::lock_guard<std::mutex> lock(h->mutex);
		h->keywords = std::move(lowered);
		h->triggerFlags = flags;
		h->keywordsConfigured = !h->keywords.empty() || flags != 0;
	});
}

inline size_t CL_CALL SinkAddDescs(CL_ResultSinkHandle sink, CL_PluginHandle h, const CL_ActionDesc* items,
									const size_t count) {
	return Guard(L"sink_add_descs", size_t{0}, [&] {
		if (!sink || !h) return size_t{0};
		auto actions = BuildProxies(h, items, count, false);
		const size_t added = actions.size();
		sink->actions.insert(sink->actions.end(), std::make_move_iterator(actions.begin()),
							std::make_move_iterator(actions.end()));
		return added;
	});
}

inline int32_t CL_CALL PostResults(CL_PluginHandle h, const uint64_t queryId, const CL_ActionDesc* items,
									const size_t count, const int32_t flags) {
	return Guard(L"post_results", 0, [&] {
		if (!h || !g_pluginManager) return 0;
		// Build the proxies on the caller's thread so the UI thread only appends.
		if (queryId != PluginManager::CurrentQueryId()) return 0;
		auto actions = BuildProxies(h, items, count, false);
		return g_pluginManager->QueueQueryResults(queryId, std::move(actions), flags) ? 1 : 0;
	});
}

inline void CL_CALL SinkTakeOver(CL_ResultSinkHandle sink, CL_PluginHandle h) {
	GuardVoid(L"sink_take_over", [&] {
		if (sink && h) sink->takeOver = true;
	});
}

inline void CL_CALL RequestRefresh(CL_PluginHandle h) {
	GuardVoid(L"request_refresh", [&] {
		if (h && g_pluginManager) g_pluginManager->RequestPluginRefresh(h->pluginId);
	});
}

inline void CL_CALL NotifyActionsChangedApi(CL_PluginHandle h) {
	GuardVoid(L"notify_actions_changed", [&] {
		if (h && g_pluginManager) g_pluginManager->RequestActionsChanged();
	});
}

inline uint64_t CL_CALL GetCurrentQueryId(CL_PluginHandle) {
	return PluginManager::CurrentQueryId();
}

}

inline const CL_HostApi* PluginManager::GetHostApi() {
	static const CL_HostApi api = [] {
		CL_HostApi a{};
		a.size = sizeof(a);
		a.api_major = CL_API_VERSION_MAJOR;
		a.api_minor = CL_API_VERSION_MINOR;
		a.log = PluginHostApiImpl::Log;
		a.show_message = PluginHostApiImpl::ShowMessage;
		a.show_toast = PluginHostApiImpl::ShowToast;
		a.show_main_window = PluginHostApiImpl::ShowMainWindow;
		a.task_done = PluginHostApiImpl::TaskDone;
		a.set_edit_text = PluginHostApiImpl::SetEditText;
		a.get_edit_text = PluginHostApiImpl::GetEditText;
		a.get_action_arg = PluginHostApiImpl::GetActionArg;
		a.process_match_text = PluginHostApiImpl::ProcessMatchText;
		a.set_actions = PluginHostApiImpl::SetActions;
		a.set_action_set_flags = PluginHostApiImpl::SetActionSetFlags;
		a.clear_actions = PluginHostApiImpl::ClearActions;
		a.show_action_set = PluginHostApiImpl::ShowActionSet;
		a.sink_add_matches = PluginHostApiImpl::SinkAddMatches;
		a.sink_add_items = PluginHostApiImpl::SinkAddItems;
		a.get_setting_bool = PluginHostApiImpl::GetSettingBool;
		a.get_setting_int = PluginHostApiImpl::GetSettingInt;
		a.get_setting_double = PluginHostApiImpl::GetSettingDouble;
		a.get_setting_string = PluginHostApiImpl::GetSettingString;
		a.get_setting_string_array = PluginHostApiImpl::GetSettingStringArray;
		a.set_setting_json = PluginHostApiImpl::SetSettingJson;
		a.is_plugin_enabled = PluginHostApiImpl::IsPluginEnabled;
		a.enum_plugin_catalog = PluginHostApiImpl::EnumPluginCatalog;
		a.set_plugin_enabled = PluginHostApiImpl::SetPluginEnabled;
		a.has_app_command = PluginHostApiImpl::HasAppCommand;
		a.invoke_app_command = PluginHostApiImpl::InvokeAppCommand;
		a.register_app_command = PluginHostApiImpl::RegisterAppCommand;
		a.unregister_app_command = PluginHostApiImpl::UnregisterAppCommand;
		a.traverse_files = PluginHostApiImpl::TraverseFilesApi;
		a.search_files = PluginHostApiImpl::SearchFiles;
		a.begin_drag = PluginHostApiImpl::BeginDrag;
		a.load_res_icon_bitmap = PluginHostApiImpl::LoadResIconBitmap;
		a.load_svg_bitmap = PluginHostApiImpl::LoadSvgBitmap;
		a.set_clipboard_text = PluginHostApiImpl::SetClipboardTextApi;
		a.set_trigger_keywords = PluginHostApiImpl::SetTriggerKeywords;
		a.sink_add_descs = PluginHostApiImpl::SinkAddDescs;
		a.post_results = PluginHostApiImpl::PostResults;
		a.notify_actions_changed = PluginHostApiImpl::NotifyActionsChangedApi;
		a.get_current_query_id = PluginHostApiImpl::GetCurrentQueryId;
		a.sink_take_over = PluginHostApiImpl::SinkTakeOver;
		a.request_refresh = PluginHostApiImpl::RequestRefresh;
		a.get_ui_language = PluginHostApiImpl::GetUiLanguage;
		a.get_string = PluginHostApiImpl::GetString;
		return a;
	}();
	return &api;
}
