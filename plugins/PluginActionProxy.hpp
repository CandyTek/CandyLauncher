#pragma once
// Host side objects behind the plugin C ABI (plugins/sdk/CandyPluginApi.h).
// Plugins never see these types; they only get opaque CL_PluginHandle / CL_ResultSinkHandle pointers.

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "BaseAction.hpp"
#include "sdk/CandyPluginApi.h"
#include "../util/BitmapUtil.hpp"

// One GDI bitmap shared by every proxy that carries the same icon_key (2.2), freed with the last proxy.
struct SharedIconBitmap {
	HBITMAP bitmap = nullptr;

	explicit SharedIconBitmap(const HBITMAP bitmap) : bitmap(bitmap) {
	}

	SharedIconBitmap(const SharedIconBitmap&) = delete;
	SharedIconBitmap& operator=(const SharedIconBitmap&) = delete;

	~SharedIconBitmap() {
		if (bitmap) DeleteObject(bitmap);
	}
};

// A list item published by a plugin. Owns copies of everything, holds no pointer into plugin code,
// so it stays safe to draw after the plugin DLL was unloaded.
class PluginActionProxy final : public BaseAction {
public:
	std::wstring title;
	std::wstring subtitle;
	// 2.2: identity that survives republishing; empty when the plugin gave none.
	std::wstring stableKey;
	int iconIndex = -1;
	std::shared_ptr<LazySysImageIndex> lazyIcon;
	std::shared_ptr<SharedIconBitmap> iconBitmap;
	uint64_t userData = 0;
	// Serial of the plugin instance that published this item, see CL_PluginContext::serial.
	uint64_t ownerSerial = 0;

	std::wstring& getTitle() override {
		return title;
	}

	std::wstring& getSubTitle() override {
		return subtitle;
	}

	std::wstring& getIconFilePath() override {
		static std::wstring empty;
		return lazyIcon ? lazyIcon->path : empty;
	}

	int getIconFilePathIndex() override {
		if (iconIndex >= 0) return iconIndex;
		return lazyIcon ? lazyIcon->get() : -1;
	}

	HBITMAP getIconBitmap() override {
		return iconBitmap ? iconBitmap->bitmap : nullptr;
	}
};

// Immutable once published: set_actions replaces the whole object, readers keep a shared_ptr snapshot
// and never need the context mutex while matching.
struct PluginActionSet {
	uint32_t flags = 0;
	std::vector<std::shared_ptr<BaseAction>> actions;
	// user_data -> index into actions, for sink_add_items.
	std::unordered_map<uint64_t, size_t> byUserData;
};

// Completes the opaque CL_PluginHandle type. One per loaded plugin instance, address stays stable.
struct CL_PluginContext {
	// Unique per load, so items of an unloaded / reloaded plugin are never dispatched to the new instance.
	uint64_t serial = 0;
	uint16_t pluginId = 65535;
	std::wstring pkgName;
	std::wstring logTag;
	// Mirrors the "<pkg>.priority" setting; stamped onto every proxy so sorting needs no lookup.
	std::atomic<int32_t> priority{0};

	std::mutex mutex;
	std::map<uint32_t, std::shared_ptr<const PluginActionSet>> sets;
	std::vector<std::string> appCommands;
	// 2.2 trigger keywords (lower case). Empty + flags 0 = the plugin receives every input.
	std::vector<std::wstring> keywords;
	uint32_t triggerFlags = 0;
	bool keywordsConfigured = false;
	// 2.2 icon_key -> shared bitmap; weak so icons vanish with the last proxy using them.
	std::unordered_map<std::wstring, std::weak_ptr<SharedIconBitmap>> iconCache;
	// 2.3 key -> text for the current UI language (PluginStringTable::Resolve). Replaced as a whole on language
	// change; read with std::atomic_load so get_string works from any thread.
	std::shared_ptr<const std::unordered_map<std::wstring, std::wstring>> strings;
};

// Completes the opaque CL_ResultSinkHandle type.
struct CL_ResultSink {
	std::vector<std::shared_ptr<BaseAction>> actions;
	// sink_take_over: show these actions (even none) instead of running the normal search.
	bool takeOver = false;
};
