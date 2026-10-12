#pragma once
// Header-only C++ convenience layer over CandyPluginApi.h.
// Everything here is compiled into the plugin DLL, so std types never cross the DLL boundary.
//
// Minimal plugin:
//
//   class MyPlugin : public candy::Plugin {
//       candy::ActionSet<MyAction> m_actions{0};
//   public:
//       void RefreshActions() override { m_actions.Publish(Host(), {...}); }
//       bool OnActionExecute(uint64_t userData, const std::wstring& arg) override {
//           auto action = m_actions.Find(userData); ...
//       }
//   };
//   CANDY_PLUGIN_EXPORT(MyPlugin, L"Name", L"Description", L"1.0.0", L"com.example.my", nullptr)
//
// Everything marked "2.2" degrades gracefully on an older host: the Host wrappers return false / do nothing
// when the host table is too small, so a plugin built with this header still loads on a 2.0 host.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "CandyPluginApi.h"

namespace candy {

namespace detail {
inline void CL_CALL AppendToWString(void* ctx, const wchar_t* str, const size_t len) {
	if (str) static_cast<std::wstring*>(ctx)->assign(str, len);
}

inline void CL_CALL PushToWStringVector(void* ctx, const wchar_t* str, const size_t len) {
	static_cast<std::vector<std::wstring>*>(ctx)->emplace_back(str ? std::wstring(str, len) : std::wstring());
}

inline std::vector<const wchar_t*> ToPointers(const std::vector<std::wstring>& values) {
	std::vector<const wchar_t*> result;
	result.reserve(values.size());
	for (const auto& value : values) result.push_back(value.c_str());
	return result;
}
}

// Base type for list items published through ActionSet / TransientSet. Owns iconBitmap.
struct ActionBase {
	std::wstring title;
	std::wstring subtitle;
	// Empty: the host processes title. Otherwise processed by the host unless matchTextVerbatim.
	std::wstring matchText;
	bool matchTextVerbatim = false;
	// Used when iconIndex < 0: the host resolves the system icon lazily.
	std::wstring iconPath;
	int iconIndex = -1;
	// 32bpp ARGB, deleted with the action. The host keeps its own copy.
	HBITMAP iconBitmap = nullptr;
	// 2.2: identity that survives republishing (path, URL, id ...). Empty = none.
	std::wstring stableKey;
	// 2.2: items with the same iconKey share one host-side copy of iconBitmap. Only the first item
	// carrying a given key needs iconBitmap set; later ones may leave it null.
	std::wstring iconKey;

	ActionBase() = default;
	ActionBase(const ActionBase&) = delete;
	ActionBase& operator=(const ActionBase&) = delete;

	virtual ~ActionBase() {
		if (iconBitmap) {
			DeleteObject(iconBitmap);
			iconBitmap = nullptr;
		}
	}
};

namespace detail {
// Fills a CL_ActionDesc from an ActionBase; the strings stay owned by `item`.
inline CL_ActionDesc MakeDesc(const ActionBase& item, const uint64_t userData) {
	CL_ActionDesc desc{};
	desc.size = sizeof(desc);
	desc.flags = item.matchTextVerbatim ? CL_ACTION_MATCH_TEXT_VERBATIM : 0u;
	desc.user_data = userData;
	desc.title = item.title.c_str();
	desc.subtitle = item.subtitle.c_str();
	desc.match_text = item.matchText.empty() ? nullptr : item.matchText.c_str();
	desc.icon_path = item.iconPath.empty() ? nullptr : item.iconPath.c_str();
	desc.icon_index = item.iconIndex;
	desc.icon_bitmap = item.iconBitmap;
	desc.stable_key = item.stableKey.empty() ? nullptr : item.stableKey.c_str();
	desc.icon_key = item.iconKey.empty() ? nullptr : item.iconKey.c_str();
	return desc;
}
}

struct TraverseOptions {
	std::wstring folder;
	std::vector<std::wstring> extensions;
	std::vector<std::wstring> excludeNames;
	std::vector<std::wstring> excludeWords;
	std::vector<std::wstring> renameSources;
	std::vector<std::wstring> renameTargets;
	bool recursive = false;
	bool indexFilesOnly = true;
};

struct DragData {
	std::vector<std::wstring> filePaths;
	std::wstring text;
	std::wstring url;
	std::wstring html;
	std::wstring rtf;
	HBITMAP bitmap = nullptr;
};

struct CatalogEntry {
	std::wstring name;
	std::wstring version;
	std::wstring packageName;
	std::wstring filePath;
	std::wstring defaultSettingJson;
	bool enabled = false;
};

// Thin wrapper over CL_HostApi bound to this plugin's handle.
class Host {
public:
	Host() = default;

	Host(const CL_HostApi* api, CL_PluginHandle handle) : m_api(api), m_handle(handle) {
	}

	const CL_HostApi* Api() const { return m_api; }
	CL_PluginHandle Handle() const { return m_handle; }
	explicit operator bool() const { return m_api != nullptr; }

	// True when the host provides `field` (added after 2.0). Usage: host.Has(&CL_HostApi::post_results).
	template <class F>
	bool Has(F CL_HostApi::* field) const {
		if (!m_api) return false;
		const auto offset = reinterpret_cast<const char*>(&(m_api->*field)) - reinterpret_cast<const char*>(m_api);
		return m_api->size >= static_cast<uint32_t>(offset + sizeof(F)) && (m_api->*field) != nullptr;
	}

	void Log(const std::wstring& message, const int32_t level = CL_LOG_INFO) const {
		m_api->log(m_handle, level, message.c_str());
	}

	void ShowMessage(const std::wstring& title, const std::wstring& message) const {
		m_api->show_message(m_handle, title.c_str(), message.c_str());
	}

	void ShowToast(const std::wstring& title, const std::wstring& message) const {
		m_api->show_toast(m_handle, title.c_str(), message.c_str());
	}

	bool ShowMainWindow(const int cmdShow, const bool foreground) const {
		return m_api->show_main_window(m_handle, cmdShow, foreground ? 1 : 0) != 0;
	}

	void TaskDone() const { m_api->task_done(m_handle); }

	void SetEditText(const std::wstring& text) const { m_api->set_edit_text(m_handle, text.c_str()); }

	std::wstring GetEditText() const {
		std::wstring result;
		m_api->get_edit_text(m_handle, detail::AppendToWString, &result);
		return result;
	}

	std::wstring GetActionArg() const {
		std::wstring result;
		m_api->get_action_arg(m_handle, detail::AppendToWString, &result);
		return result;
	}

	std::wstring ProcessMatchText(const std::wstring& source) const {
		std::wstring result;
		m_api->process_match_text(m_handle, source.c_str(), detail::AppendToWString, &result);
		return result;
	}

	void SetActions(const uint32_t setId, const CL_ActionDesc* items, const size_t count, const uint32_t setFlags) const {
		m_api->set_actions(m_handle, setId, items, count, setFlags);
	}

	void SetActionSetFlags(const uint32_t setId, const uint32_t setFlags) const {
		m_api->set_action_set_flags(m_handle, setId, setFlags);
	}

	void ClearActions(const uint32_t setId) const { m_api->clear_actions(m_handle, setId); }

	void ShowActionSet(const uint32_t setId) const { m_api->show_action_set(m_handle, setId); }

	bool GetBool(const wchar_t* key, const bool defValue = false) const {
		int32_t value = 0;
		return m_api->get_setting_bool(m_handle, key, &value) ? value != 0 : defValue;
	}

	int64_t GetInt(const wchar_t* key, const int64_t defValue = 0) const {
		int64_t value = 0;
		return m_api->get_setting_int(m_handle, key, &value) ? value : defValue;
	}

	double GetDouble(const wchar_t* key, const double defValue = 0.0) const {
		double value = 0;
		return m_api->get_setting_double(m_handle, key, &value) ? value : defValue;
	}

	std::wstring GetString(const wchar_t* key, const std::wstring& defValue = L"") const {
		std::wstring value;
		return m_api->get_setting_string(m_handle, key, detail::AppendToWString, &value) ? value : defValue;
	}

	std::vector<std::wstring> GetStringArray(const wchar_t* key) const {
		std::vector<std::wstring> values;
		m_api->get_setting_string_array(m_handle, key, detail::PushToWStringVector, &values);
		return values;
	}

	bool SetSettingJson(const wchar_t* key, const std::wstring& valueJson) const {
		return m_api->set_setting_json(m_handle, key, valueJson.c_str()) != 0;
	}

	bool IsPluginEnabled(const std::wstring& packageName) const {
		return m_api->is_plugin_enabled(m_handle, packageName.c_str()) != 0;
	}

	std::vector<CatalogEntry> GetPluginCatalog() const {
		std::vector<CatalogEntry> entries;
		m_api->enum_plugin_catalog(m_handle, [](void* ctx, const CL_CatalogEntry* entry) {
			if (!entry) return;
			auto str = [](const wchar_t* s) { return s ? std::wstring(s) : std::wstring(); };
			CatalogEntry item;
			item.name = str(entry->name);
			item.version = str(entry->version);
			item.packageName = str(entry->package_name);
			item.filePath = str(entry->file_path);
			item.defaultSettingJson = str(entry->default_setting_json);
			item.enabled = entry->enabled != 0;
			static_cast<std::vector<CatalogEntry>*>(ctx)->push_back(std::move(item));
		}, &entries);
		return entries;
	}

	bool SetPluginEnabled(const std::wstring& packageName, const bool enabled) const {
		return m_api->set_plugin_enabled(m_handle, packageName.c_str(), enabled ? 1 : 0) != 0;
	}

	bool HasAppCommand(const wchar_t* key) const { return m_api->has_app_command(m_handle, key) != 0; }
	bool InvokeAppCommand(const wchar_t* key) const { return m_api->invoke_app_command(m_handle, key) != 0; }

	// fn/ctx must stay valid until UnregisterAppCommand or plugin unload.
	bool RegisterAppCommand(const wchar_t* key, const CL_AppCommandFn fn, void* ctx) const {
		return m_api->register_app_command(m_handle, key, fn, ctx) != 0;
	}

	void UnregisterAppCommand(const wchar_t* key) const { m_api->unregister_app_command(m_handle, key); }

	// visitor(name, fullPath, parent, ext)
	template <class Fn>
	void TraverseFiles(const TraverseOptions& options, Fn&& visitor) const {
		const auto extensions = detail::ToPointers(options.extensions);
		const auto excludeNames = detail::ToPointers(options.excludeNames);
		const auto excludeWords = detail::ToPointers(options.excludeWords);
		const auto renameSources = detail::ToPointers(options.renameSources);
		const auto renameTargets = detail::ToPointers(options.renameTargets);
		CL_TraverseOptions raw{};
		raw.size = sizeof(raw);
		raw.folder = options.folder.c_str();
		raw.extensions = extensions.data();
		raw.extension_count = extensions.size();
		raw.exclude_names = excludeNames.data();
		raw.exclude_name_count = excludeNames.size();
		raw.exclude_words = excludeWords.data();
		raw.exclude_word_count = excludeWords.size();
		raw.rename_sources = renameSources.data();
		raw.rename_targets = renameTargets.data();
		raw.rename_count = (std::min)(renameSources.size(), renameTargets.size());
		raw.recursive = options.recursive ? 1 : 0;
		raw.index_files_only = options.indexFilesOnly ? 1 : 0;
		using FnType = std::remove_reference_t<Fn>;
		m_api->traverse_files(m_handle, &raw,
							[](void* ctx, const wchar_t* name, const wchar_t* fullPath, const wchar_t* parent,
								const wchar_t* ext) {
								try {
									(*static_cast<FnType*>(ctx))(std::wstring(name), std::wstring(fullPath),
																std::wstring(parent), std::wstring(ext));
								} catch (...) {
								}
							}, &visitor);
	}

	std::vector<std::wstring> SearchFiles(const std::wstring& query, const uint32_t maxResults) const {
		std::vector<std::wstring> paths;
		m_api->search_files(m_handle, query.c_str(), maxResults, detail::PushToWStringVector, &paths);
		return paths;
	}

	bool BeginDrag(const DragData& data, const HWND sourceHwnd) const {
		const auto filePaths = detail::ToPointers(data.filePaths);
		CL_DragData raw{};
		raw.size = sizeof(raw);
		raw.file_paths = filePaths.data();
		raw.file_count = filePaths.size();
		raw.text = data.text.empty() ? nullptr : data.text.c_str();
		raw.url = data.url.empty() ? nullptr : data.url.c_str();
		raw.html = data.html.empty() ? nullptr : data.html.c_str();
		raw.rtf = data.rtf.empty() ? nullptr : data.rtf.c_str();
		raw.bitmap = data.bitmap;
		return m_api->begin_drag(m_handle, &raw, sourceHwnd) != 0;
	}

	HBITMAP LoadResIconBitmap(const int resId, const int cx, const int cy) const {
		return m_api->load_res_icon_bitmap(m_handle, resId, cx, cy);
	}

	HBITMAP LoadSvgBitmap(const std::wstring& path, const uint32_t width, const uint32_t height) const {
		return m_api->load_svg_bitmap(m_handle, path.c_str(), width, height);
	}

	// ---- added in 2.1: return false / do nothing on an older host ----

	bool SetClipboardText(const std::wstring& text) const {
		return Has(&CL_HostApi::set_clipboard_text) && m_api->set_clipboard_text(m_handle, text.c_str()) != 0;
	}

	// ---- added in 2.2: return false / do nothing on an older host ----

	// Routes on_user_input / on_intercept_input / on_query to this plugin only for inputs starting with
	// one of `keywords` (plus every input when CL_TRIGGER_ALSO_GLOBAL). Empty keywords + flags 0 = always.
	bool SetTriggerKeywords(const std::vector<std::wstring>& keywords, const uint32_t flags = 0) const {
		if (!Has(&CL_HostApi::set_trigger_keywords)) return false;
		const auto pointers = detail::ToPointers(keywords);
		m_api->set_trigger_keywords(m_handle, pointers.data(), pointers.size(), flags);
		return true;
	}

	// Delivers a batch of query results; see TransientSet::PostResults for the usual way to call it.
	bool PostResults(const uint64_t queryId, const CL_ActionDesc* items, const size_t count, const bool isFinal) const {
		return Has(&CL_HostApi::post_results) &&
			m_api->post_results(m_handle, queryId, items, count, isFinal ? 1 : 0) != 0;
	}

	bool NotifyActionsChanged() const {
		if (!Has(&CL_HostApi::notify_actions_changed)) return false;
		m_api->notify_actions_changed(m_handle);
		return true;
	}

	// 0 on an older host.
	uint64_t GetCurrentQueryId() const {
		return Has(&CL_HostApi::get_current_query_id) ? m_api->get_current_query_id(m_handle) : 0;
	}

private:
	const CL_HostApi* m_api = nullptr;
	CL_PluginHandle m_handle = nullptr;
};

// A set of actions owned by the plugin and mirrored to the host.
// user_data = set id (16 bit) | publish generation (16 bit) | index (32 bit),
// so stale items from an older publish are rejected by Find.
template <class T>
class ActionSet {
	static_assert(std::is_base_of_v<ActionBase, T>, "T must derive from candy::ActionBase");

public:
	explicit ActionSet(const uint16_t setId, const uint32_t setFlags = CL_SET_GLOBAL_SEARCH)
		: m_setId(setId), m_setFlags(setFlags) {
	}

	uint16_t Id() const { return m_setId; }

	static uint16_t SetIdOf(const uint64_t userData) { return static_cast<uint16_t>(userData >> 48); }

	// Replaces the set on the plugin side and on the host. Any thread.
	void Publish(const Host& host, std::vector<std::shared_ptr<T>> items) {
		std::lock_guard<std::mutex> lock(m_mutex);
		++m_generation;
		m_items = std::move(items);
		std::vector<CL_ActionDesc> descs;
		descs.reserve(m_items.size());
		for (size_t i = 0; i < m_items.size(); ++i) {
			descs.push_back(detail::MakeDesc(*m_items[i], MakeUserData(i)));
		}
		host.SetActions(m_setId, descs.data(), descs.size(), m_setFlags);
	}

	void SetFlags(const Host& host, const uint32_t setFlags) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_setFlags = setFlags;
		host.SetActionSetFlags(m_setId, setFlags);
	}

	void Clear(const Host& host) {
		std::lock_guard<std::mutex> lock(m_mutex);
		++m_generation;
		m_items.clear();
		host.ClearActions(m_setId);
	}

	// Drops the plugin side items only (e.g. on shutdown).
	void ClearLocal() {
		std::lock_guard<std::mutex> lock(m_mutex);
		++m_generation;
		m_items.clear();
	}

	std::shared_ptr<T> Find(const uint64_t userData) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (SetIdOf(userData) != m_setId) return nullptr;
		if (static_cast<uint16_t>(userData >> 32) != m_generation) return nullptr;
		const size_t index = static_cast<uint32_t>(userData);
		return index < m_items.size() ? m_items[index] : nullptr;
	}

	std::vector<std::shared_ptr<T>> Items() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_items;
	}

	size_t Size() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_items.size();
	}

private:
	uint64_t MakeUserData(const size_t index) const {
		return (static_cast<uint64_t>(m_setId) << 48) | (static_cast<uint64_t>(m_generation) << 32) |
			static_cast<uint32_t>(index);
	}

	const uint16_t m_setId;
	uint32_t m_setFlags;
	uint16_t m_generation = 0;
	std::vector<std::shared_ptr<T>> m_items;
	mutable std::mutex m_mutex;
};

// 2.2: items that exist only for the current input (intercept results, async query results).
// Unlike ActionSet nothing is stored on the host side; the plugin keeps the objects until the next
// Begin(). user_data uses the same layout as ActionSet, so set ids must not collide with ActionSets.
template <class T>
class TransientSet {
	static_assert(std::is_base_of_v<ActionBase, T>, "T must derive from candy::ActionBase");

public:
	explicit TransientSet(const uint16_t setId) : m_setId(setId) {
	}

	uint16_t Id() const { return m_setId; }

	// Starts a new generation: previous items become stale. Any thread.
	void Begin() {
		std::lock_guard<std::mutex> lock(m_mutex);
		++m_generation;
		m_items.clear();
	}

	// Keeps `items` and returns their descriptors (valid while the items stay in the set). Any thread.
	std::vector<CL_ActionDesc> Append(const std::vector<std::shared_ptr<T>>& items) {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<CL_ActionDesc> descs;
		descs.reserve(items.size());
		for (const auto& item : items) {
			m_items.push_back(item);
			descs.push_back(detail::MakeDesc(*item, MakeUserData(m_items.size() - 1)));
		}
		return descs;
	}

	// Begin() + Append() + post_results in one call. Returns false when the query is stale.
	bool PostResults(const Host& host, const uint64_t queryId, const std::vector<std::shared_ptr<T>>& items,
					const bool isFinal, const bool restart = false) {
		if (restart) Begin();
		const auto descs = Append(items);
		return host.PostResults(queryId, descs.data(), descs.size(), isFinal);
	}

	std::shared_ptr<T> Find(const uint64_t userData) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (static_cast<uint16_t>(userData >> 48) != m_setId) return nullptr;
		if (static_cast<uint16_t>(userData >> 32) != m_generation) return nullptr;
		const size_t index = static_cast<uint32_t>(userData);
		return index < m_items.size() ? m_items[index] : nullptr;
	}

	size_t Size() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_items.size();
	}

private:
	uint64_t MakeUserData(const size_t index) const {
		return (static_cast<uint64_t>(m_setId) << 48) | (static_cast<uint64_t>(m_generation) << 32) |
			static_cast<uint32_t>(index);
	}

	const uint16_t m_setId;
	uint16_t m_generation = 0;
	std::vector<std::shared_ptr<T>> m_items;
	mutable std::mutex m_mutex;
};

// Collects results inside Plugin::OnInterceptInput.
class ResultSink {
public:
	ResultSink(const Host& host, const CL_ResultSinkHandle sink) : m_sdkHost(host), m_sink(sink) {
	}

	// Items of the set matching query (empty query = all).
	size_t AddMatches(const uint32_t setId, const std::wstring& query) const {
		return m_sdkHost.Api()->sink_add_matches(m_sink, m_sdkHost.Handle(), setId, query.c_str());
	}

	size_t AddAll(const uint32_t setId) const {
		return m_sdkHost.Api()->sink_add_items(m_sink, m_sdkHost.Handle(), setId, nullptr, 0);
	}

	size_t AddItems(const uint32_t setId, const std::vector<uint64_t>& userData) const {
		return m_sdkHost.Api()->sink_add_items(m_sink, m_sdkHost.Handle(), setId, userData.data(), userData.size());
	}

	// 2.2: items that live only for this input; `set` keeps them for later callbacks. Begin() is called
	// first so items from the previous input become stale. Returns 0 on an older host.
	template <class T>
	size_t AddTransient(TransientSet<T>& set, const std::vector<std::shared_ptr<T>>& items) const {
		if (!m_sdkHost.Has(&CL_HostApi::sink_add_descs)) return 0;
		set.Begin();
		const auto descs = set.Append(items);
		return m_sdkHost.Api()->sink_add_descs(m_sink, m_sdkHost.Handle(), descs.data(), descs.size());
	}

private:
	const Host& m_sdkHost;
	CL_ResultSinkHandle m_sink;
};

// Base class for C++ plugins. Callbacks run on the UI thread unless noted.
class Plugin {
public:
	virtual ~Plugin() = default;

	const Host& GetHost() const { return m_sdkHost; }

	// Called once after construction; return false to abort loading.
	virtual bool Initialize() { return true; }
	// Called before the instance is deleted.
	virtual void Shutdown() {}

	virtual void OnUserSettingsLoaded() {}
	// Rebuild and publish action sets here, not in Initialize.
	// Runs on a worker thread when the plugin is exported with CL_PLUGIN_REFRESH_ANY_THREAD.
	virtual void RefreshActions() {}
	virtual void OnMainWindowShow(bool isShow) {}
	virtual void OnUserInput(const std::wstring& input) {}
	// Must be fast, it runs on every key stroke.
	virtual void OnInterceptInput(const std::wstring& input, ResultSink& sink) {}
	virtual bool OnActionExecute(uint64_t userData, const std::wstring& arg) { return false; }
	virtual bool OnHotKey(uint64_t userData, UINT vk, UINT modifiers, WPARAM wparam) { return false; }
	virtual bool OnItemRightClick(uint64_t userData, HWND parent, POINT screenPt) { return false; }
	virtual bool OnItemShiftRightClick(uint64_t userData, HWND parent, POINT screenPt) { return false; }
	virtual bool OnItemBeginDrag(uint64_t userData, HWND source, POINT screenPt) { return false; }
	virtual void OnSettingItemExecute(const std::wstring& key, HWND parent) {}
	// Ctrl+C on an item while the input box has no selection; fill text and return true to copy it.
	virtual bool GetItemCopyText(uint64_t userData, std::wstring& text) { return false; }
	// 2.2: start asynchronous work for `input`; deliver through TransientSet::PostResults(queryId, ...).
	virtual void OnQuery(uint64_t queryId, const std::wstring& input, const std::wstring& arg) {}
	virtual void OnQueryCancel(uint64_t queryId) {}
	// 2.2: one of this plugin's settings changed (called before OnUserSettingsLoaded).
	virtual void OnSettingChanged(const std::wstring& key) {}

private:
	template <class>
	friend struct PluginBridge;
	Host m_sdkHost;
};

// Converts CL_PluginApi calls into Plugin virtual calls; no exception escapes.
template <class T>
struct PluginBridge {
	static T* Self(void* self) { return static_cast<T*>(self); }

	static void ReportException(void* self, const char* where) {
		if (!self) return;
		const Host& host = Self(self)->GetHost();
		if (!host) return;
		std::wstring message = L"Unhandled exception in ";
		for (const char* p = where; *p; ++p) message += static_cast<wchar_t>(*p);
		host.Log(message, CL_LOG_ERROR);
	}

	static void* CL_CALL Create(const CL_PluginHandle handle, const CL_HostApi* api) {
		if (!api || api->api_major != CL_API_VERSION_MAJOR) return nullptr;
		T* plugin = nullptr;
		try {
			plugin = new T();
			static_cast<Plugin*>(plugin)->m_sdkHost = Host(api, handle);
			if (plugin->Initialize()) return plugin;
		} catch (...) {
			ReportException(plugin, "create");
		}
		delete plugin;
		return nullptr;
	}

	static void CL_CALL Destroy(void* self) {
		try {
			Self(self)->Shutdown();
		} catch (...) {
			ReportException(self, "destroy");
		}
		try {
			delete Self(self);
		} catch (...) {
		}
	}

	static void CL_CALL OnUserSettingsLoaded(void* self) {
		try { Self(self)->OnUserSettingsLoaded(); } catch (...) { ReportException(self, "on_user_settings_loaded"); }
	}

	static void CL_CALL RefreshActions(void* self) {
		try { Self(self)->RefreshActions(); } catch (...) { ReportException(self, "refresh_actions"); }
	}

	static void CL_CALL OnMainWindowShow(void* self, const int32_t isShow) {
		try { Self(self)->OnMainWindowShow(isShow != 0); } catch (...) { ReportException(self, "on_main_window_show"); }
	}

	static void CL_CALL OnUserInput(void* self, const wchar_t* input) {
		try { Self(self)->OnUserInput(input ? input : L""); } catch (...) { ReportException(self, "on_user_input"); }
	}

	static void CL_CALL OnInterceptInput(void* self, const wchar_t* input, const CL_ResultSinkHandle sinkHandle) {
		try {
			ResultSink sink(Self(self)->GetHost(), sinkHandle);
			Self(self)->OnInterceptInput(input ? input : L"", sink);
		} catch (...) {
			ReportException(self, "on_intercept_input");
		}
	}

	static int32_t CL_CALL OnActionExecute(void* self, const uint64_t userData, const wchar_t* arg) {
		try { return Self(self)->OnActionExecute(userData, arg ? arg : L"") ? 1 : 0; }
		catch (...) { ReportException(self, "on_action_execute"); }
		return 0;
	}

	static int32_t CL_CALL OnHotKey(void* self, const uint64_t userData, const uint32_t vk, const uint32_t modifiers,
									const WPARAM wparam) {
		try { return Self(self)->OnHotKey(userData, vk, modifiers, wparam) ? 1 : 0; }
		catch (...) { ReportException(self, "on_hotkey"); }
		return 0;
	}

	static int32_t CL_CALL OnItemRightClick(void* self, const uint64_t userData, const HWND parent, const POINT pt) {
		try { return Self(self)->OnItemRightClick(userData, parent, pt) ? 1 : 0; }
		catch (...) { ReportException(self, "on_item_right_click"); }
		return 0;
	}

	static int32_t CL_CALL OnItemShiftRightClick(void* self, const uint64_t userData, const HWND parent, const POINT pt) {
		try { return Self(self)->OnItemShiftRightClick(userData, parent, pt) ? 1 : 0; }
		catch (...) { ReportException(self, "on_item_shift_right_click"); }
		return 0;
	}

	static int32_t CL_CALL OnItemBeginDrag(void* self, const uint64_t userData, const HWND source, const POINT pt) {
		try { return Self(self)->OnItemBeginDrag(userData, source, pt) ? 1 : 0; }
		catch (...) { ReportException(self, "on_item_begin_drag"); }
		return 0;
	}

	static void CL_CALL OnSettingItemExecute(void* self, const wchar_t* key, const HWND parent) {
		try { Self(self)->OnSettingItemExecute(key ? key : L"", parent); }
		catch (...) { ReportException(self, "on_setting_item_execute"); }
	}

	static int32_t CL_CALL GetItemCopyText(void* self, const uint64_t userData, const CL_StrSink sink, void* ctx) {
		try {
			std::wstring text;
			if (!sink || !Self(self)->GetItemCopyText(userData, text)) return 0;
			sink(ctx, text.c_str(), text.size());
			return 1;
		} catch (...) {
			ReportException(self, "get_item_copy_text");
		}
		return 0;
	}

	static void CL_CALL OnQuery(void* self, const uint64_t queryId, const wchar_t* input, const wchar_t* arg) {
		try { Self(self)->OnQuery(queryId, input ? input : L"", arg ? arg : L""); }
		catch (...) { ReportException(self, "on_query"); }
	}

	static void CL_CALL OnQueryCancel(void* self, const uint64_t queryId) {
		try { Self(self)->OnQueryCancel(queryId); } catch (...) { ReportException(self, "on_query_cancel"); }
	}

	static void CL_CALL OnSettingChanged(void* self, const wchar_t* key) {
		try { Self(self)->OnSettingChanged(key ? key : L""); } catch (...) { ReportException(self, "on_setting_changed"); }
	}

	static CL_PluginApi MakeApi(const wchar_t* name, const wchar_t* description, const wchar_t* version,
								const wchar_t* packageName, const wchar_t* defaultSettingJson, const uint32_t flags) {
		CL_PluginApi api{};
		api.size = sizeof(api);
		api.api_major = CL_API_VERSION_MAJOR;
		api.api_minor = CL_API_VERSION_MINOR;
		api.name = name;
		api.description = description;
		api.version = version;
		api.package_name = packageName;
		api.default_setting_json = defaultSettingJson;
		api.create = Create;
		api.destroy = Destroy;
		api.on_user_settings_loaded = OnUserSettingsLoaded;
		api.refresh_actions = RefreshActions;
		api.on_main_window_show = OnMainWindowShow;
		api.on_user_input = OnUserInput;
		api.on_intercept_input = OnInterceptInput;
		api.on_action_execute = OnActionExecute;
		api.on_hotkey = OnHotKey;
		api.on_item_right_click = OnItemRightClick;
		api.on_item_shift_right_click = OnItemShiftRightClick;
		api.on_item_begin_drag = OnItemBeginDrag;
		api.on_setting_item_execute = OnSettingItemExecute;
		api.get_item_copy_text = GetItemCopyText;
		api.flags = flags;
		api.on_query = OnQuery;
		api.on_query_cancel = OnQueryCancel;
		api.on_setting_changed = OnSettingChanged;
		return api;
	}
};

}

// Exports the plugin entry point. Use exactly once per plugin DLL.
// All string arguments must be string literals or otherwise outlive the DLL.
#define CANDY_PLUGIN_EXPORT_EX(PluginClass, Name, Description, Version, PackageName, DefaultSettingJson, Flags) \
	extern "C" __declspec(dllexport) const CL_PluginApi* CL_CALL CandyPlugin_GetApi(void) { \
		static const CL_PluginApi api = candy::PluginBridge<PluginClass>::MakeApi( \
			Name, Description, Version, PackageName, DefaultSettingJson, Flags); \
		return &api; \
	}

#define CANDY_PLUGIN_EXPORT(PluginClass, Name, Description, Version, PackageName, DefaultSettingJson) \
	CANDY_PLUGIN_EXPORT_EX(PluginClass, Name, Description, Version, PackageName, DefaultSettingJson, 0u)
