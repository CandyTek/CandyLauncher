/*
 * CandyLauncher plugin C ABI.
 *
 * This header is the only contract between CandyLauncher.exe and plugin DLLs.
 * It is pure C so that plugins may be built with any compiler / C runtime
 * (MSVC, MinGW, clang, Rust, Zig ...). Nothing that owns memory crosses the boundary.
 *
 * Rules
 * - All text is UTF-16 (wchar_t), NUL-terminated, unless a length is given.
 * - Strings passed into a call are only valid during that call; copy them if you keep them.
 * - Strings coming back from the host are delivered through CL_StrSink callbacks.
 * - Booleans are int32_t (0 = false, non-zero = true).
 * - C++ exceptions must never cross the boundary (the C++ SDK guards every entry point).
 * - Every struct starts with `size`. New fields are only ever appended; a reader must check
 *   CL_HAS_FIELD before touching a field that was added in a later minor version.
 * - A major version change means the ABI is incompatible and the host refuses the plugin.
 * - Unless stated otherwise, host functions must be called on the UI thread
 *   (the thread that calls into the plugin).
 */
#ifndef CANDY_PLUGIN_API_H
#define CANDY_PLUGIN_API_H

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CL_API_VERSION_MAJOR 2
#define CL_API_VERSION_MINOR 3

#define CL_CALL __cdecl

/* Name of the single function a plugin DLL must export. */
#define CL_PLUGIN_ENTRY_NAME "CandyPlugin_GetApi"

/* True if the struct instance `ptr` (of type `type`) is large enough to contain `field`. */
#define CL_HAS_FIELD(ptr, type, field) \
	((ptr) != NULL && (ptr)->size >= (uint32_t)(offsetof(type, field) + sizeof(((type*)0)->field)))

/* Opaque handle identifying the calling plugin. Pass it back unchanged to every host function. */
typedef struct CL_PluginContext* CL_PluginHandle;

/* Opaque result collector, only valid inside CL_PluginApi::on_intercept_input. */
typedef struct CL_ResultSink* CL_ResultSinkHandle;

/* Receives a string from the host. `str` is valid only during the callback, `len` excludes the NUL. */
typedef void (CL_CALL *CL_StrSink)(void* ctx, const wchar_t* str, size_t len);

/* ------------------------------------------------------------------------- */
/* Actions (list items)                                                       */
/* ------------------------------------------------------------------------- */

/* match_text is used exactly as given instead of being processed (lower case + pinyin) by the host. */
#define CL_ACTION_MATCH_TEXT_VERBATIM 0x1u

typedef struct CL_ActionDesc {
	uint32_t size;               /* sizeof(CL_ActionDesc) */
	uint32_t flags;              /* CL_ACTION_* */
	uint64_t user_data;          /* plugin defined, handed back in every action callback */
	const wchar_t* title;
	const wchar_t* subtitle;     /* may be NULL */
	const wchar_t* match_text;   /* NULL: host processes title; otherwise processed unless VERBATIM */
	const wchar_t* icon_path;    /* used when icon_index < 0: host resolves the system icon lazily */
	int32_t icon_index;          /* system image list index, -1 = none */
	HBITMAP icon_bitmap;         /* 32bpp premultiplied ARGB; the host copies it, caller keeps ownership */

	/* ---- added in 2.2, the host reads them only when `size` covers them ---- */
	/* Identity that survives republishing (path, URL, id ...). NULL = none. The host uses it to recognise
	 * the same item across refreshes (selection, usage ranking). */
	const wchar_t* stable_key;
	/* Items sharing an icon_key share one host-side copy of icon_bitmap. When icon_key is set and
	 * icon_bitmap is NULL the host reuses the copy made by an earlier item with the same key. */
	const wchar_t* icon_key;
} CL_ActionDesc;

/* The set takes part in the global search of the main input box. */
#define CL_SET_GLOBAL_SEARCH 0x1u

/* ---- trigger keywords (2.2) ----
 * on_user_input / on_intercept_input / on_query are only called when the input starts with one of
 * the plugin's keywords (case insensitive). A plugin that never calls set_trigger_keywords is called
 * for every input, as before 2.2. */
/* Also call the plugin when no keyword matches (e.g. the calculator auto detecting math). */
#define CL_TRIGGER_ALSO_GLOBAL 0x1u

/* ---- CL_PluginApi::flags (2.2) ---- */
/* refresh_actions may run on a worker thread (the host CoInitializes it, MTA). The plugin must only use
 * host functions marked "Any thread" from it. Without the flag refresh_actions runs on the host thread
 * that triggered the refresh, one plugin after another. */
#define CL_PLUGIN_REFRESH_ANY_THREAD 0x1u

/* ------------------------------------------------------------------------- */
/* Misc structs                                                               */
/* ------------------------------------------------------------------------- */

enum {
	CL_LOG_INFO = 0,
	CL_LOG_WARN = 1,
	CL_LOG_ERROR = 2
};

typedef struct CL_TraverseOptions {
	uint32_t size;
	const wchar_t* folder;                 /* environment variables are expanded */
	const wchar_t* const* extensions;      /* e.g. L".exe"; NULL/0 = all */
	size_t extension_count;
	const wchar_t* const* exclude_names;   /* exact, case sensitive file names */
	size_t exclude_name_count;
	const wchar_t* const* exclude_words;   /* lower case sub strings */
	size_t exclude_word_count;
	const wchar_t* const* rename_sources;  /* file stem -> display name */
	const wchar_t* const* rename_targets;
	size_t rename_count;
	int32_t recursive;
	int32_t index_files_only;
} CL_TraverseOptions;

/* name = display name (stem, after rename), ext includes the dot. */
typedef void (CL_CALL *CL_FileVisitor)(void* ctx, const wchar_t* name, const wchar_t* full_path,
										const wchar_t* parent, const wchar_t* ext);

typedef struct CL_DragData {
	uint32_t size;
	const wchar_t* const* file_paths;
	size_t file_count;
	const wchar_t* text;   /* may be NULL */
	const wchar_t* url;    /* may be NULL */
	const wchar_t* html;   /* may be NULL */
	const wchar_t* rtf;    /* may be NULL */
	HBITMAP bitmap;        /* copied by the host, caller keeps ownership */
} CL_DragData;

typedef struct CL_CatalogEntry {
	uint32_t size;
	const wchar_t* name;
	const wchar_t* version;
	const wchar_t* package_name;
	const wchar_t* file_path;
	const wchar_t* default_setting_json;
	int32_t enabled;
	/* ---- added in 2.3 ---- */
	const wchar_t* description;  /* localized, like `name` */
} CL_CatalogEntry;

typedef void (CL_CALL *CL_CatalogVisitor)(void* ctx, const CL_CatalogEntry* entry);

typedef void (CL_CALL *CL_AppCommandFn)(void* ctx);

/* ------------------------------------------------------------------------- */
/* Host -> plugin services                                                    */
/* ------------------------------------------------------------------------- */

typedef struct CL_HostApi {
	uint32_t size;
	uint32_t api_major;
	uint32_t api_minor;

	/* ---- logging / UI ---- */
	/* Any thread. */
	void (CL_CALL *log)(CL_PluginHandle h, int32_t level, const wchar_t* message);
	void (CL_CALL *show_message)(CL_PluginHandle h, const wchar_t* title, const wchar_t* message);
	void (CL_CALL *show_toast)(CL_PluginHandle h, const wchar_t* title, const wchar_t* message);
	int32_t (CL_CALL *show_main_window)(CL_PluginHandle h, int32_t cmd_show, int32_t foreground);
	/* Tell the host an action finished; it hides the window if the user prefers so. */
	void (CL_CALL *task_done)(CL_PluginHandle h);
	void (CL_CALL *set_edit_text)(CL_PluginHandle h, const wchar_t* text);
	void (CL_CALL *get_edit_text)(CL_PluginHandle h, CL_StrSink sink, void* ctx);
	/* Argument typed after the action keyword (see on_action_execute). */
	void (CL_CALL *get_action_arg)(CL_PluginHandle h, CL_StrSink sink, void* ctx);

	/* ---- matching ---- */
	/* Lower case + pinyin variants, the same processing the host applies to match_text. Any thread. */
	void (CL_CALL *process_match_text)(CL_PluginHandle h, const wchar_t* source, CL_StrSink sink, void* ctx);

	/* ---- action sets ----
	 * A plugin owns any number of action sets identified by set_id.
	 * set_actions replaces the whole set; the host copies everything it needs.
	 * Sets flagged CL_SET_GLOBAL_SEARCH are merged into the main search.
	 * These three functions may be called from any thread. */
	void (CL_CALL *set_actions)(CL_PluginHandle h, uint32_t set_id, const CL_ActionDesc* items, size_t count,
								uint32_t set_flags);
	void (CL_CALL *set_action_set_flags)(CL_PluginHandle h, uint32_t set_id, uint32_t set_flags);
	void (CL_CALL *clear_actions)(CL_PluginHandle h, uint32_t set_id);
	/* Immediately show the whole set as the result list (replaces the current results). */
	void (CL_CALL *show_action_set)(CL_PluginHandle h, uint32_t set_id);

	/* ---- result sink, only inside on_intercept_input ---- */
	/* Adds the items of set_id matching query (empty query = all). Returns the number added. */
	size_t (CL_CALL *sink_add_matches)(CL_ResultSinkHandle sink, CL_PluginHandle h, uint32_t set_id,
										const wchar_t* query);
	/* Adds specific items of set_id by user_data, in the given order (NULL/0 = whole set). */
	size_t (CL_CALL *sink_add_items)(CL_ResultSinkHandle sink, CL_PluginHandle h, uint32_t set_id,
									const uint64_t* user_data, size_t count);

	/* ---- settings (keys as in the plugin's default_setting_json). Any thread. ---- */
	int32_t (CL_CALL *get_setting_bool)(CL_PluginHandle h, const wchar_t* key, int32_t* out);
	int32_t (CL_CALL *get_setting_int)(CL_PluginHandle h, const wchar_t* key, int64_t* out);
	int32_t (CL_CALL *get_setting_double)(CL_PluginHandle h, const wchar_t* key, double* out);
	int32_t (CL_CALL *get_setting_string)(CL_PluginHandle h, const wchar_t* key, CL_StrSink sink, void* ctx);
	/* The sink is called once per element. */
	int32_t (CL_CALL *get_setting_string_array)(CL_PluginHandle h, const wchar_t* key, CL_StrSink sink, void* ctx);
	/* Only keys starting with "<package_name>." may be written. value_json is a JSON value, e.g. L"true". */
	int32_t (CL_CALL *set_setting_json)(CL_PluginHandle h, const wchar_t* key, const wchar_t* value_json);

	/* ---- plugins ---- */
	int32_t (CL_CALL *is_plugin_enabled)(CL_PluginHandle h, const wchar_t* package_name);
	/* Restricted to the plugin browser; returns 0 when not permitted. */
	int32_t (CL_CALL *enum_plugin_catalog)(CL_PluginHandle h, CL_CatalogVisitor visitor, void* ctx);
	int32_t (CL_CALL *set_plugin_enabled)(CL_PluginHandle h, const wchar_t* package_name, int32_t enabled);

	/* ---- app commands (named host / plugin callbacks such as L"quit", L"restart") ---- */
	int32_t (CL_CALL *has_app_command)(CL_PluginHandle h, const wchar_t* key);
	int32_t (CL_CALL *invoke_app_command)(CL_PluginHandle h, const wchar_t* key);
	/* Removed automatically when the plugin is unloaded. */
	int32_t (CL_CALL *register_app_command)(CL_PluginHandle h, const wchar_t* key, CL_AppCommandFn fn, void* ctx);
	void (CL_CALL *unregister_app_command)(CL_PluginHandle h, const wchar_t* key);

	/* ---- files (Everything SDK with file system fallback). Any thread. ---- */
	void (CL_CALL *traverse_files)(CL_PluginHandle h, const CL_TraverseOptions* options, CL_FileVisitor visitor,
									void* ctx);
	/* Raw Everything query; the sink receives existing full paths. */
	void (CL_CALL *search_files)(CL_PluginHandle h, const wchar_t* query, uint32_t max_results, CL_StrSink sink,
								void* ctx);

	/* ---- drag & drop ---- */
	int32_t (CL_CALL *begin_drag)(CL_PluginHandle h, const CL_DragData* data, HWND source_hwnd);

	/* ---- images; the caller owns the returned HBITMAP. Any thread. ---- */
	HBITMAP (CL_CALL *load_res_icon_bitmap)(CL_PluginHandle h, int32_t res_id, int32_t cx, int32_t cy);
	HBITMAP (CL_CALL *load_svg_bitmap)(CL_PluginHandle h, const wchar_t* path, uint32_t width, uint32_t height);

	/* ---- added in 2.1, check CL_HAS_FIELD before use ---- */
	/* Replaces the clipboard content with `text` (CF_UNICODETEXT). */
	int32_t (CL_CALL *set_clipboard_text)(CL_PluginHandle h, const wchar_t* text);

	/* ---- added in 2.2, check CL_HAS_FIELD before use ---- */
	/* Keywords that route input to this plugin, see CL_TRIGGER_*. NULL/0 keywords + flags 0 restores
	 * "call for every input". Any thread. */
	void (CL_CALL *set_trigger_keywords)(CL_PluginHandle h, const wchar_t* const* keywords, size_t count,
										uint32_t flags);
	/* Adds items that exist only for the current input (no set, no pinyin processing). The plugin must keep
	 * whatever user_data refers to alive until the next input. Only inside on_intercept_input. */
	size_t (CL_CALL *sink_add_descs)(CL_ResultSinkHandle sink, CL_PluginHandle h, const CL_ActionDesc* items,
									size_t count);
	/* Delivers results for the query started by on_query. The host copies the items, marshals them to the
	 * UI thread and appends them to the visible list (CL_RESULTS_REPLACE: replaces the list instead);
	 * results for a query that is no longer current are dropped. May be called several times per query;
	 * `flags` is a CL_RESULTS_* combination (plain 1 / 0 still mean final / not final).
	 * Any thread. Returns 0 when the query is stale (stop working on it). */
	int32_t (CL_CALL *post_results)(CL_PluginHandle h, uint64_t query_id, const CL_ActionDesc* items, size_t count,
									int32_t flags);
	/* Tells the host that sets published from a background thread changed, so the visible list is rebuilt
	 * from the current input. Coalesced. Any thread. */
	void (CL_CALL *notify_actions_changed)(CL_PluginHandle h);
	/* Current query id (the one the last on_query / on_intercept_input was called with). Any thread. */
	uint64_t (CL_CALL *get_current_query_id)(CL_PluginHandle h);
	/* Marks the intercept as taking over the result list even when nothing was added, so the host shows an
	 * empty list (results then arrive through post_results) instead of running the normal search. */
	void (CL_CALL *sink_take_over)(CL_ResultSinkHandle sink, CL_PluginHandle h);
	/* Asks the host to call refresh_actions again (on the plugin's worker when CL_PLUGIN_REFRESH_ANY_THREAD,
	 * otherwise on the UI thread) and to rebuild the visible list afterwards. Coalesced. Any thread. */
	void (CL_CALL *request_refresh)(CL_PluginHandle h);

	/* ---- added in 2.3: localization, see docs/DOC_PLUGIN_I18N.md ---- */
	/* Current UI language of the host, e.g. L"zh-CN" / L"en-US". Any thread. */
	void (CL_CALL *get_ui_language)(CL_PluginHandle h, CL_StrSink sink, void* ctx);
	/* Looks `key` up in the plugin's string tables (plugins/lang/<dll stem>/<language>.json and the
	 * manifest's "strings" object) for the current language, falling back to the same primary language,
	 * then en-US, then the first table that defines the key. Returns 0 when no table has it, so the plugin
	 * can use its own built-in text. Any thread. */
	int32_t (CL_CALL *get_string)(CL_PluginHandle h, const wchar_t* key, CL_StrSink sink, void* ctx);
} CL_HostApi;

/* post_results flags */
#define CL_RESULTS_FINAL 0x1      /* no more batches follow for this query */
#define CL_RESULTS_REPLACE 0x2    /* replace the visible list instead of appending */

/* ------------------------------------------------------------------------- */
/* Plugin -> host entry points                                                */
/* ------------------------------------------------------------------------- */

typedef struct CL_PluginApi {
	uint32_t size;               /* sizeof(CL_PluginApi) */
	uint32_t api_major;          /* CL_API_VERSION_MAJOR the plugin was built against */
	uint32_t api_minor;

	/* Static metadata, readable without creating the plugin. */
	const wchar_t* name;
	const wchar_t* description;
	const wchar_t* version;
	const wchar_t* package_name; /* required, e.g. L"com.candytek.bookmarkplugin" */
	const wchar_t* default_setting_json; /* may be NULL */

	/* Required. Returns the instance pointer passed as `self` below, or NULL on failure.
	 * `host` stays valid until destroy. Do not load actions here; wait for refresh_actions. */
	void* (CL_CALL *create)(CL_PluginHandle h, const CL_HostApi* host);
	/* Required. Release everything; the DLL is unloaded afterwards. */
	void (CL_CALL *destroy)(void* self);

	/* All of the following are optional (NULL). */
	void (CL_CALL *on_user_settings_loaded)(void* self);
	void (CL_CALL *refresh_actions)(void* self);
	void (CL_CALL *on_main_window_show)(void* self, int32_t is_show);
	void (CL_CALL *on_user_input)(void* self, const wchar_t* input);
	/* Add results to `sink` to take over the result list for this input. Keep it fast. */
	void (CL_CALL *on_intercept_input)(void* self, const wchar_t* input, CL_ResultSinkHandle sink);
	/* Return non-zero when the action ran (the host may then hide the window). */
	int32_t (CL_CALL *on_action_execute)(void* self, uint64_t user_data, const wchar_t* arg);
	/* Key pressed while one of this plugin's items is selected. Non-zero = handled. */
	int32_t (CL_CALL *on_hotkey)(void* self, uint64_t user_data, uint32_t vk, uint32_t modifiers, WPARAM wparam);
	int32_t (CL_CALL *on_item_right_click)(void* self, uint64_t user_data, HWND parent, POINT screen_pt);
	int32_t (CL_CALL *on_item_shift_right_click)(void* self, uint64_t user_data, HWND parent, POINT screen_pt);
	int32_t (CL_CALL *on_item_begin_drag)(void* self, uint64_t user_data, HWND source, POINT screen_pt);
	/* A button-like setting item declared in default_setting_json was clicked. */
	void (CL_CALL *on_setting_item_execute)(void* self, const wchar_t* key, HWND parent);

	/* ---- added in 2.1, the host checks CL_HAS_FIELD before calling ---- */
	/* Ctrl+C on one of this plugin's items while the input box has no selection.
	 * Deliver the text to copy through `sink` and return non-zero; return 0 to keep the default. */
	int32_t (CL_CALL *get_item_copy_text)(void* self, uint64_t user_data, CL_StrSink sink, void* ctx);

	/* ---- added in 2.2, the host checks CL_HAS_FIELD before calling ---- */
	uint32_t flags;              /* CL_PLUGIN_* */
	/* Asynchronous search: called after on_intercept_input for every input (subject to trigger keywords).
	 * Return quickly; deliver results later with post_results(query_id, ...) from any thread.
	 * `arg` is the {"arg":"..."} argument typed before the input, or empty. */
	void (CL_CALL *on_query)(void* self, uint64_t query_id, const wchar_t* input, const wchar_t* arg);
	/* The input changed before the query finished; stop working on query_id. */
	void (CL_CALL *on_query_cancel)(void* self, uint64_t query_id);
	/* One of this plugin's settings ("<package_name>.*") changed. Called before on_user_settings_loaded. */
	void (CL_CALL *on_setting_changed)(void* self, const wchar_t* key);

	/* ---- added in 2.3, the host checks CL_HAS_FIELD before calling ---- */
	/* The host UI language changed (user setting). get_string already answers in the new language; the host
	 * calls refresh_actions afterwards, so sets republished there pick up new titles by themselves. Use this
	 * for anything else that caches text (menus, open windows, keywords ...). */
	void (CL_CALL *on_language_changed)(void* self, const wchar_t* language);
} CL_PluginApi;

typedef const CL_PluginApi* (CL_CALL *CL_GetPluginApiFn)(void);

#ifdef __cplusplus
}
#endif

#endif /* CANDY_PLUGIN_API_H */
