#pragma once
#include <string>

struct NodeJsPluginData {
    // Any global data for the plugin
};

struct HBitmapDeleter {
    void operator()(HBITMAP h) const {
        if (h) DeleteObject(h);
    }
};

using UniqueHBitmap = std::unique_ptr<std::remove_pointer<HBITMAP>::type, HBitmapDeleter>;

inline std::unordered_map<std::wstring, UniqueHBitmap> pluginIconMap;
