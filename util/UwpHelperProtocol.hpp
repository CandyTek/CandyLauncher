#pragma once

// CandyUwpHelper.exe 与 Folder 插件之间的管道协议（小端二进制，写到助手进程的 stdout）：
//   命令行参数选择枚举源：uwp（默认）| control_panel | windows_settings
//   u32 magic "CUWP"  u32 version  u32 count
//   每项：u32 nameChars  wchar_t[nameChars]  u32 targetChars  wchar_t[targetChars]
//         u32 iconKeyChars  wchar_t[iconKeyChars]
//         i32 width  i32 height(符号同 biHeight)  u32 pixelBytes  u8[pixelBytes]
//   iconKey 非空表示多项共用一张图标：只有第一次出现该 key 的项带像素，之后的项 pixelBytes=0，
//   与主程序 CL_ActionDesc::icon_key 的语义一致（按像素内容哈希生成，跨次刷新不会串图）。
// 助手进程退出码：0 成功；非 0 时调用方忽略输出并回退到进程内枚举。

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "UwpAppsEnumerator.hpp"

namespace UwpHelper {

constexpr wchar_t kExeName[] = L"CandyUwpHelper.exe";
constexpr uint32_t kMagic = 0x50575543; // "CUWP"
constexpr uint32_t kVersion = 2;

constexpr wchar_t kModeUwp[] = L"uwp";
constexpr wchar_t kModeControlPanel[] = L"control_panel";
constexpr wchar_t kModeWindowsSettings[] = L"windows_settings";

struct Item {
	std::wstring name;    // 显示名（UWP：商店显示名）
	std::wstring target;  // UWP：AppsFolder 解析名；控制面板：桌面绝对解析名；设置：ms-settings:xxx
	std::wstring iconKey; // 空 = 独占图标或无图标
	UwpApps::DibPixels icon; // 可能为空（提取失败，或 iconKey 已在之前的项带过像素）
};

// 按像素内容生成图标共享键（FNV-1a 64），内容相同的图标在主程序里只保留一份
inline std::wstring IconKeyForPixels(const UwpApps::DibPixels& pixels, const wchar_t* prefix) {
	uint64_t hash = 1469598103934665603ULL;
	auto mix = [&](const uint8_t* data, const size_t size) {
		for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 1099511628211ULL;
	};
	mix(reinterpret_cast<const uint8_t*>(&pixels.width), sizeof(pixels.width));
	mix(reinterpret_cast<const uint8_t*>(&pixels.height), sizeof(pixels.height));
	mix(pixels.bytes.data(), pixels.bytes.size());
	wchar_t text[32]{};
	swprintf_s(text, L"%016llx", static_cast<unsigned long long>(hash));
	return std::wstring(prefix) + text;
}

namespace detail {

inline void Put32(std::vector<uint8_t>& out, const uint32_t value) {
	const auto* p = reinterpret_cast<const uint8_t*>(&value);
	out.insert(out.end(), p, p + sizeof(value));
}

inline void PutString(std::vector<uint8_t>& out, const std::wstring& s) {
	Put32(out, static_cast<uint32_t>(s.size()));
	const auto* p = reinterpret_cast<const uint8_t*>(s.data());
	out.insert(out.end(), p, p + s.size() * sizeof(wchar_t));
}

struct Reader {
	const uint8_t* data;
	size_t size;
	size_t pos = 0;

	bool Take(void* dst, const size_t n) {
		if (n > size - pos) return false;
		std::memcpy(dst, data + pos, n);
		pos += n;
		return true;
	}

	bool Get32(uint32_t& v) { return Take(&v, sizeof(v)); }

	bool GetString(std::wstring& s) {
		uint32_t chars = 0;
		if (!Get32(chars) || chars > (size - pos) / sizeof(wchar_t)) return false;
		s.resize(chars);
		return chars == 0 || Take(&s[0], chars * sizeof(wchar_t));
	}
};

} // namespace detail

inline void Serialize(const std::vector<Item>& items, std::vector<uint8_t>& out) {
	out.clear();
	detail::Put32(out, kMagic);
	detail::Put32(out, kVersion);
	detail::Put32(out, static_cast<uint32_t>(items.size()));
	for (const Item& item : items) {
		detail::PutString(out, item.name);
		detail::PutString(out, item.target);
		detail::PutString(out, item.iconKey);
		detail::Put32(out, static_cast<uint32_t>(item.icon.width));
		detail::Put32(out, static_cast<uint32_t>(item.icon.height));
		detail::Put32(out, static_cast<uint32_t>(item.icon.bytes.size()));
		out.insert(out.end(), item.icon.bytes.begin(), item.icon.bytes.end());
	}
}

// 解析失败（截断、魔数/版本不符）返回 false，out 内容未定义
inline bool Deserialize(const uint8_t* data, const size_t size, std::vector<Item>& out) {
	out.clear();
	detail::Reader r{data, size};
	uint32_t magic = 0, version = 0, count = 0;
	if (!r.Get32(magic) || !r.Get32(version) || !r.Get32(count)) return false;
	if (magic != kMagic || version != kVersion) return false;
	out.reserve(count);
	for (uint32_t i = 0; i < count; ++i) {
		Item item;
		uint32_t width = 0, height = 0, pixelBytes = 0;
		if (!r.GetString(item.name) || !r.GetString(item.target) || !r.GetString(item.iconKey) ||
			!r.Get32(width) || !r.Get32(height) || !r.Get32(pixelBytes)) {
			return false;
		}
		if (pixelBytes > size - r.pos) return false;
		item.icon.width = static_cast<int32_t>(width);
		item.icon.height = static_cast<int32_t>(height);
		item.icon.bytes.resize(pixelBytes);
		if (pixelBytes > 0 && !r.Take(item.icon.bytes.data(), pixelBytes)) return false;
		out.push_back(std::move(item));
	}
	return true;
}

} // namespace UwpHelper
