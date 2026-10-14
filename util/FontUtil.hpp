#pragma once

// 字体解析：皮肤 / 界面请求的字体族在当前系统不存在时强制退回到一定存在的字体。
// Windows 7 没有 "Microsoft YaHei UI" 等 Windows 8 新增的 UI 变体；GDI 的 CreateFont 会自动替换，
// 但 GDI+ 的 FontFamily 不会（状态 FontFamilyNotFound，DrawString 什么都不画），所以构造前必须先解析。

#include <windows.h>
#include <gdiplus.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "LogUtil.hpp"

// 字体族是否已安装（EnumFontFamiliesExW 按 lfFaceName 精确匹配），结果按名称缓存
inline bool IsFontFamilyInstalled(const std::wstring& family) {
	static std::mutex mutex;
	static std::unordered_map<std::wstring, bool> cache;
	if (family.empty() || family.size() >= LF_FACESIZE) return false;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (const auto it = cache.find(family); it != cache.end()) return it->second;
	}
	LOGFONTW logFont{};
	logFont.lfCharSet = DEFAULT_CHARSET;
	wcsncpy_s(logFont.lfFaceName, family.c_str(), LF_FACESIZE - 1);
	bool found = false;
	if (const HDC hdc = GetDC(nullptr)) {
		EnumFontFamiliesExW(hdc, &logFont,
			[](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lParam) -> int {
				*reinterpret_cast<bool*>(lParam) = true;
				return 0;
			},
			reinterpret_cast<LPARAM>(&found), 0);
		ReleaseDC(nullptr, hdc);
	}
	std::lock_guard<std::mutex> lock(mutex);
	cache[family] = found;
	return found;
}

// 系统消息字体（SPI_GETNONCLIENTMETRICS），任何 Windows 上一定存在：Win7 中文为 Microsoft YaHei，英文为 Segoe UI
inline const std::wstring& SystemUiFontFamily() {
	static const std::wstring family = [] {
		NONCLIENTMETRICSW metrics{};
		metrics.cbSize = sizeof(metrics);
		if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) && metrics.lfMessageFont.lfFaceName[0]) {
			return std::wstring(metrics.lfMessageFont.lfFaceName);
		}
		return std::wstring(L"MS Shell Dlg 2");
	}();
	return family;
}

// 请求的字体存在则原样返回；"xxx UI" 不存在时先试去掉 " UI" 的同族字体（Microsoft YaHei UI → Microsoft YaHei），
// 再退回系统消息字体。每个缺失的字体只记录一次日志
inline std::wstring ResolveFontFamily(const std::wstring& requested) {
	if (requested.empty()) return SystemUiFontFamily();
	if (IsFontFamilyInstalled(requested)) return requested;
	static std::mutex mutex;
	static std::unordered_map<std::wstring, std::wstring> resolved;
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (const auto it = resolved.find(requested); it != resolved.end()) return it->second;
	}
	std::wstring result;
	const std::wstring suffix = L" UI";
	if (requested.size() > suffix.size() && requested.compare(requested.size() - suffix.size(), suffix.size(), suffix) == 0) {
		const std::wstring base = requested.substr(0, requested.size() - suffix.size());
		if (IsFontFamilyInstalled(base)) result = base;
	}
	if (result.empty()) result = SystemUiFontFamily();
	Logw(L"FontUtil", L"Font family not installed: ", requested, L", using: ", result);
	std::lock_guard<std::mutex> lock(mutex);
	resolved[requested] = result;
	return result;
}

// GDI+ 字体：先解析字体族，构造失败时最后退回 GDI+ 通用无衬线字体，保证返回的字体总能画出文字
inline std::unique_ptr<Gdiplus::Font> CreateGdiplusFont(const std::wstring& family, float size, INT style, Gdiplus::Unit unit) {
	Gdiplus::FontFamily fontFamily(ResolveFontFamily(family).c_str());
	auto font = std::make_unique<Gdiplus::Font>(&fontFamily, size, style, unit);
	if (fontFamily.GetLastStatus() == Gdiplus::Ok && font->GetLastStatus() == Gdiplus::Ok) return font;
	Logw(L"FontUtil", L"GDI+ font creation failed for: ", family, L", using generic sans serif");
	return std::make_unique<Gdiplus::Font>(Gdiplus::FontFamily::GenericSansSerif(), size, style, unit);
}
