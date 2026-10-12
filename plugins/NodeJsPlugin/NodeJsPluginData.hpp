#pragma once
#include "util/LogUtil.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <windows.h>

#include "plugins/sdk/CandyPluginSdk.hpp"

inline const candy::Host* m_host = nullptr;

struct HBitmapDeleter {
    void operator()(HBITMAP h) const {
        if (h) DeleteObject(h);
    }
};

using UniqueHBitmap = std::unique_ptr<std::remove_pointer<HBITMAP>::type, HBitmapDeleter>;

// JS 插件自身的图标（plugin.json 的 IcoPath），key 为 JS 插件 id；扫描完成后只读
inline std::unordered_map<std::wstring, UniqueHBitmap> pluginIconMap;
