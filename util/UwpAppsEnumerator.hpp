#pragma once

// UWP（打包应用）枚举与图标提取的最小实现，不依赖主程序的任何全局状态，
// 同时被独立进程 CandyUwpHelper.exe（uwp_helper/UwpHelperMain.cpp）和进程内回退路径
// （util/UWPAppsTraverser.hpp）编译。
//
// 枚举 shell:AppsFolder 会把 windows.storage / Windows.StateRepository / CoreUIComponents 等
// WinRT 运行库拉进进程并常驻约 10 MB，所以默认放在短命的助手进程里做，结果连同 32bpp 图标像素
// 一起通过管道传回（util/UwpHelperProtocol.hpp）。

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace UwpApps {

struct Entry {
	std::wstring displayName; // 商店显示名
	std::wstring parsingName; // AppsFolder 解析名（PackageFamilyName!AppId）
};

// 枚举 shell:AppsFolder 中的打包应用（解析名含 '!'）。调用线程须已初始化 COM。
// enumeratedTotal 返回 AppsFolder 中的条目总数（含非 UWP 项），仅用于日志。
inline std::vector<Entry> Enumerate(size_t* enumeratedTotal = nullptr) {
	std::vector<Entry> result;
	size_t total = 0;

	IShellItem* appsFolderItem = nullptr;
	if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&appsFolderItem)))) {
		IShellFolder* appsFolder = nullptr;
		if (SUCCEEDED(appsFolderItem->BindToHandler(nullptr, BHID_SFObject, IID_PPV_ARGS(&appsFolder)))) {
			IEnumIDList* enumIdList = nullptr;
			if (appsFolder->EnumObjects(nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &enumIdList) == S_OK) {
				PITEMID_CHILD pidl = nullptr;
				while (enumIdList->Next(1, &pidl, nullptr) == S_OK) {
					++total;
					// 直接从文件夹读取名称，避免为每个非 UWP 条目创建 ShellItem
					STRRET parsingRet{}, displayRet{};
					LPWSTR parsingName = nullptr;
					LPWSTR displayName = nullptr;
					if (SUCCEEDED(appsFolder->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &parsingRet)) &&
						SUCCEEDED(StrRetToStrW(&parsingRet, pidl, &parsingName)) &&
						// 打包应用的解析名形如 PackageFamilyName!AppId
						wcschr(parsingName, L'!') != nullptr &&
						SUCCEEDED(appsFolder->GetDisplayNameOf(pidl, SHGDN_NORMAL, &displayRet)) &&
						SUCCEEDED(StrRetToStrW(&displayRet, pidl, &displayName))) {
						result.push_back({displayName, parsingName});
					}
					CoTaskMemFree(displayName);
					CoTaskMemFree(parsingName);
					CoTaskMemFree(pidl);
				}
				enumIdList->Release();
			}
			appsFolder->Release();
		}
		appsFolderItem->Release();
	}
	if (enumeratedTotal) *enumeratedTotal = total;
	return result;
}

// 按解析名提取图标（size x size，32bpp 预乘 ARGB DIB）。Shell 图标提取较慢且串行，
// 这里开多个 STA 线程并行；实测 8 线程最快，更多线程会在 Shell 内部锁上竞争。
inline void LoadIcons(const std::vector<std::wstring>& parsingNames, std::vector<HBITMAP>& bitmaps, const int size = 48) {
	bitmaps.assign(parsingNames.size(), nullptr);
	if (parsingNames.empty()) return;
	const size_t threadCount = (std::min<size_t>)(8, parsingNames.size());
	std::atomic<size_t> next{0};
	auto worker = [&]() {
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		for (size_t i = next++; i < parsingNames.size(); i = next++) {
			IShellItemImageFactory* imageFactory = nullptr;
			const std::wstring itemPath = L"shell:AppsFolder\\" + parsingNames[i];
			if (FAILED(SHCreateItemFromParsingName(itemPath.c_str(), nullptr, IID_PPV_ARGS(&imageFactory)))) continue;
			// SIIGBF_RESIZETOFIT 保证即使没有精确尺寸也能拿到图像；SIIGBF_ICONONLY 避免拿到缩略图
			imageFactory->GetImage({size, size}, SIIGBF_RESIZETOFIT | SIIGBF_ICONONLY, &bitmaps[i]);
			imageFactory->Release();
		}
		if (SUCCEEDED(comResult)) CoUninitialize();
	};
	std::vector<std::thread> threads;
	threads.reserve(threadCount - 1);
	for (size_t i = 1; i < threadCount; ++i) threads.emplace_back(worker);
	worker();
	for (auto& thread : threads) thread.join();
}

// 可跨进程传输的 32bpp 像素块。height 沿用 BITMAPINFOHEADER::biHeight 的符号约定
//（正数自下而上、负数自上而下），这样重建出的 DIB 与 Shell 返回的逐位相同。
struct DibPixels {
	int32_t width = 0;
	int32_t height = 0;
	std::vector<uint8_t> bytes; // width * |height| * 4，BGRA，行紧密排列

	bool empty() const { return width <= 0 || height == 0 || bytes.empty(); }
};

// 复制位图像素。Shell 返回的是 32bpp DIB 节，直接读位指针；其他位图退回 GetDIBits（自上而下）
inline bool CopyDibPixels(const HBITMAP bitmap, DibPixels& out) {
	out = DibPixels{};
	if (!bitmap) return false;

	DIBSECTION section{};
	if (GetObjectW(bitmap, sizeof(section), &section) == sizeof(section) && section.dsBm.bmBits &&
		section.dsBm.bmBitsPixel == 32 && section.dsBmih.biCompression == BI_RGB) {
		const int32_t width = section.dsBmih.biWidth;
		const int32_t rows = section.dsBmih.biHeight < 0 ? -section.dsBmih.biHeight : section.dsBmih.biHeight;
		if (width <= 0 || rows <= 0) return false;
		const size_t rowBytes = static_cast<size_t>(width) * 4;
		const size_t stride = static_cast<size_t>(section.dsBm.bmWidthBytes);
		out.width = width;
		out.height = section.dsBmih.biHeight;
		out.bytes.resize(rowBytes * rows);
		const auto* src = static_cast<const uint8_t*>(section.dsBm.bmBits);
		for (int32_t y = 0; y < rows; ++y) {
			std::memcpy(out.bytes.data() + rowBytes * y, src + stride * y, rowBytes);
		}
		return true;
	}

	BITMAP bm{};
	if (GetObjectW(bitmap, sizeof(bm), &bm) != sizeof(bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) return false;
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = bm.bmWidth;
	info.bmiHeader.biHeight = -bm.bmHeight; // 自上而下
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	out.bytes.resize(static_cast<size_t>(bm.bmWidth) * bm.bmHeight * 4);
	const HDC dc = CreateCompatibleDC(nullptr);
	const int copied = dc ? GetDIBits(dc, bitmap, 0, bm.bmHeight, out.bytes.data(), &info, DIB_RGB_COLORS) : 0;
	if (dc) DeleteDC(dc);
	if (copied != bm.bmHeight) {
		out = DibPixels{};
		return false;
	}
	out.width = bm.bmWidth;
	out.height = -bm.bmHeight;
	return true;
}

// 由像素块重建 32bpp DIB 节；调用方拥有返回的位图
inline HBITMAP CreateDibFromPixels(const DibPixels& pixels) {
	if (pixels.empty()) return nullptr;
	const int32_t rows = pixels.height < 0 ? -pixels.height : pixels.height;
	const size_t expected = static_cast<size_t>(pixels.width) * rows * 4;
	if (pixels.bytes.size() != expected) return nullptr;

	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = pixels.width;
	info.bmiHeader.biHeight = pixels.height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	const HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!bitmap || !bits) {
		if (bitmap) DeleteObject(bitmap);
		return nullptr;
	}
	std::memcpy(bits, pixels.bytes.data(), expected);
	return bitmap;
}

} // namespace UwpApps
