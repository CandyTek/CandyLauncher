/*
 * Minimal CandyLauncher plugin written in pure C (C99), using only plugins/sdk/CandyPluginApi.h.
 * Not part of the main build, see CMakeLists.txt in this folder for how to build it.
 *
 * - Publishes two items into the global search.
 * - Typing the activation command (setting "start_str", default "cc ") shows only these items.
 * - Enter shows a message box, Ctrl+C copies the item's text (2.1 callback).
 */
#include <stdlib.h>
#include <string.h>
#include "plugins/sdk/CandyPluginApi.h"

#define PKG L"com.example.cplugin"
#define ITEM_SET 0u

typedef struct ExampleCPlugin {
	CL_PluginHandle h;
	const CL_HostApi* host;
	wchar_t startStr[64];
} ExampleCPlugin;

/* Items are static; user_data is simply index + 1. */
static const wchar_t* const ITEM_TITLES[] = {L"Hello from C", L"C plugin docs"};
static const wchar_t* const ITEM_SUBTITLES[] = {L"Press Enter to say hello", L"docs/DOC_PLUGIN_API_ABI_NOTES.md"};
#define ITEM_COUNT (sizeof(ITEM_TITLES) / sizeof(ITEM_TITLES[0]))

static const wchar_t* const DEFAULT_SETTING_JSON = L"{\"version\":1,\"prefList\":[{"
	L"\"key\":\"com.example.cplugin.start_str\",\"title\":\"Activation command\","
	L"\"type\":\"string\",\"subPage\":\"plugin\",\"defValue\":\"cc \"}]}";

/* Copies a host string into a fixed buffer (CL_StrSink). */
typedef struct FixedBuf {
	wchar_t* buf;
	size_t cap;
} FixedBuf;

static void CL_CALL copy_to_fixed(void* ctx, const wchar_t* str, size_t len) {
	FixedBuf* out = (FixedBuf*)ctx;
	if (!str || out->cap == 0) return;
	if (len >= out->cap) len = out->cap - 1;
	memcpy(out->buf, str, len * sizeof(wchar_t));
	out->buf[len] = 0;
}

static int starts_with(const wchar_t* s, const wchar_t* prefix) {
	const size_t n = wcslen(prefix);
	return n > 0 && wcsncmp(s, prefix, n) == 0;
}

static void* CL_CALL example_create(CL_PluginHandle h, const CL_HostApi* host) {
	ExampleCPlugin* self;
	if (!host || host->api_major != CL_API_VERSION_MAJOR) return NULL;
	self = (ExampleCPlugin*)calloc(1, sizeof(ExampleCPlugin));
	if (!self) return NULL;
	self->h = h;
	self->host = host;
	wcscpy(self->startStr, L"cc ");
	return self;
}

static void CL_CALL example_destroy(void* self) {
	free(self);
}

static void CL_CALL example_settings_loaded(void* p) {
	ExampleCPlugin* self = (ExampleCPlugin*)p;
	FixedBuf out;
	out.buf = self->startStr;
	out.cap = sizeof(self->startStr) / sizeof(self->startStr[0]);
	if (!self->host->get_setting_string(self->h, PKG L".start_str", copy_to_fixed, &out)) {
		wcscpy(self->startStr, L"cc ");
	}
}

static void CL_CALL example_refresh(void* p) {
	ExampleCPlugin* self = (ExampleCPlugin*)p;
	CL_ActionDesc items[ITEM_COUNT];
	size_t i;
	memset(items, 0, sizeof(items));
	for (i = 0; i < ITEM_COUNT; ++i) {
		items[i].size = sizeof(CL_ActionDesc);
		items[i].user_data = i + 1;
		items[i].title = ITEM_TITLES[i];
		items[i].subtitle = ITEM_SUBTITLES[i];
		items[i].icon_index = -1;
	}
	/* The host copies everything, the array may live on the stack. */
	self->host->set_actions(self->h, ITEM_SET, items, ITEM_COUNT, CL_SET_GLOBAL_SEARCH);
	self->host->log(self->h, CL_LOG_INFO, L"ExampleC published items");
}

static void CL_CALL example_intercept_input(void* p, const wchar_t* input, CL_ResultSinkHandle sink) {
	ExampleCPlugin* self = (ExampleCPlugin*)p;
	if (input && starts_with(input, self->startStr)) {
		self->host->sink_add_matches(sink, self->h, ITEM_SET, input + wcslen(self->startStr));
	}
}

static int32_t CL_CALL example_execute(void* p, uint64_t user_data, const wchar_t* arg) {
	ExampleCPlugin* self = (ExampleCPlugin*)p;
	(void)arg;
	if (user_data == 0 || user_data > ITEM_COUNT) return 0;
	self->host->show_message(self->h, L"ExampleC", ITEM_TITLES[user_data - 1]);
	return 1;
}

/* Added in 2.1: an older host never calls it because the table it reads is shorter. */
static int32_t CL_CALL example_copy_text(void* p, uint64_t user_data, CL_StrSink sink, void* ctx) {
	(void)p;
	if (user_data == 0 || user_data > ITEM_COUNT) return 0;
	sink(ctx, ITEM_SUBTITLES[user_data - 1], wcslen(ITEM_SUBTITLES[user_data - 1]));
	return 1;
}

__declspec(dllexport) const CL_PluginApi* CL_CALL CandyPlugin_GetApi(void) {
	static CL_PluginApi api;
	if (!api.size) {
		api.size = sizeof(api);
		api.api_major = CL_API_VERSION_MAJOR;
		api.api_minor = CL_API_VERSION_MINOR;
		api.name = L"ExampleC";
		api.description = L"Pure C plugin example";
		api.version = L"1.0.0";
		api.package_name = PKG;
		api.default_setting_json = DEFAULT_SETTING_JSON;
		api.create = example_create;
		api.destroy = example_destroy;
		api.on_user_settings_loaded = example_settings_loaded;
		api.refresh_actions = example_refresh;
		api.on_intercept_input = example_intercept_input;
		api.on_action_execute = example_execute;
		api.get_item_copy_text = example_copy_text;
	}
	return &api;
}
