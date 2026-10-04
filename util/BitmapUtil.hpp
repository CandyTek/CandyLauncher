#pragma once

#include <windows.h>
#include <string>
#include <ShlObj.h>
#include <vector>
#include <shellapi.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <mutex>
#include <unordered_map>
#include "util/ShortcutUtil.hpp"
#include <gdiplus.h>

[[deprecated(L"不要使用，获取shell32 缺少很多图标，30%")]]
static HICON LoadShell32Icon(int index, int cx = 16, int cy = 16) {
	HMODULE hMod = GetModuleHandleW(L"shell32.dll");
	return (HICON)LoadImageW(hMod, MAKEINTRESOURCEW(index), IMAGE_ICON, cx, cy, LR_SHARED);
}

// 从 shell32.dll 加载指定索引的图标，可以获取 16 和 32尺寸
static HICON LoadShell32IconProper(int index, int cx = 16, int cy = 16) {
	HICON hSmall = NULL;
	HICON hLarge = NULL;
	ExtractIconExW(L"shell32.dll", index, &hLarge, &hSmall, 1);
	if (cx <= 16) return hSmall;
	return hLarge;
}

// 从 shell32.dll 加载指定索引的图标，更好的方法，可以获取所有尺寸
static HICON LoadShell32IconHQ(int index, int cx = 48, int cy = 48) {
	HICON hIcon = NULL;
	HRESULT hr = SHDefExtractIconW(L"shell32.dll", index, 0, &hIcon, NULL, cx);
	if (SUCCEEDED(hr) && hIcon) return hIcon;

	// 回退方案
	HICON hSmall = NULL, hLarge = NULL;
	ExtractIconExW(L"shell32.dll", index, &hLarge, &hSmall, 1);
	if (cx <= 16 && hSmall) return hSmall;
	if (hLarge) {
		HICON hResized = (HICON)CopyImage(hLarge, IMAGE_ICON, cx, cy, LR_COPYFROMRESOURCE);
		DestroyIcon(hLarge);
		return hResized;
	}
	return NULL;
}

// 从 shell32.dll 加载指定索引的图标为 HBITMAP（48x48），没有保留透明度
inline HBITMAP LoadShell32Bitmap2(int index, int cx = 48, int cy = 48) {
	HICON hIcon = NULL;
	if (ExtractIconExW(L"shell32.dll", index, &hIcon, NULL, 1) > 0 && hIcon) {
		HDC hdc = GetDC(NULL);
		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP hbm = CreateCompatibleBitmap(hdc, cx, cy);
		HGDIOBJ old = SelectObject(memDC, hbm);

		// 背景透明
		RECT rc = {0, 0, cx, cy};
		FillRect(memDC, &rc, (HBRUSH)(COLOR_WINDOW + 1));

		DrawIconEx(memDC, 0, 0, hIcon, cx, cy, 0, NULL, DI_NORMAL);

		SelectObject(memDC, old);
		DeleteDC(memDC);
		ReleaseDC(NULL, hdc);
		DestroyIcon(hIcon);

		return hbm;
	}
	return NULL;
}

// 转换bitmap，保留透明度并更改尺寸
static HBITMAP IconToBitmap(HICON hIcon, int cx, int cy) {
	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = cx;
	bi.bmiHeader.biHeight = -cy; // 负数 => top-down DIB，绘制更直接
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void* pBits = nullptr;
	HDC hdc = GetDC(nullptr);
	HBITMAP hBmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	HDC hMem = CreateCompatibleDC(hdc);
	HGDIOBJ hOld = SelectObject(hMem, hBmp);

	// 清空为全透明
	if (pBits) std::memset(pBits, 0, cx * cy * 4);

	DrawIconEx(hMem, 0, 0, hIcon, cx, cy, 0, nullptr, DI_NORMAL);

	SelectObject(hMem, hOld);
	DeleteDC(hMem);
	ReleaseDC(nullptr, hdc);
	return hBmp;
}

// 保留透明度的转换
static HBITMAP IconToBitmap2(HICON hIcon) {
	ICONINFO ii{};
	if (!GetIconInfo(hIcon, &ii)) return nullptr;

	BITMAP bm{};
	GetObject(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof(bm), &bm);

	int cx = bm.bmWidth;
	int cy = bm.bmHeight;

	// 某些图标（特别是带 mask 的 1bpp 图标）bmHeight 可能是双倍（含掩码）
	if (ii.hbmMask && !ii.hbmColor) cy /= 2;

	// 清理资源
	if (ii.hbmMask) DeleteObject(ii.hbmMask);
	if (ii.hbmColor) DeleteObject(ii.hbmColor);

	BITMAPINFO bi{};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = cx;
	bi.bmiHeader.biHeight = -cy; // top-down DIB
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void* pBits = nullptr;
	HDC hdc = GetDC(nullptr);
	HBITMAP hBmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
	HDC hMem = CreateCompatibleDC(hdc);
	HGDIOBJ hOld = SelectObject(hMem, hBmp);

	// 清空为透明
	if (pBits) std::memset(pBits, 0, cx * cy * 4);

	DrawIconEx(hMem, 0, 0, hIcon, cx, cy, 0, nullptr, DI_NORMAL);

	SelectObject(hMem, hOld);
	DeleteDC(hMem);
	ReleaseDC(nullptr, hdc);
	return hBmp;
}


// 没有用到这个图标查询方法
static HICON GetFileIcon(const std::wstring& filePath, const bool largeIcon) {
	SHFILEINFOW sfi = {0};
	UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
	flags |= largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON;

	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
	return sfi.hIcon;
}

// 这个有时候会不保留透明度，需要注意
static HBITMAP IconToBitmap(HICON hIcon) {
	if (!hIcon) return nullptr;

	ICONINFO iconInfo;
	GetIconInfo(hIcon, &iconInfo);

	return iconInfo.hbmColor;
}

static HICON BitmapToIcon(HBITMAP hBitmap) {
	if (!hBitmap) return nullptr;

	ICONINFO ii = {};
	ii.fIcon = TRUE;
	ii.hbmColor = hBitmap;

	// 创建一个透明遮罩（1位黑白位图）
	BITMAP bm = {};
	GetObject(hBitmap, sizeof(BITMAP), &bm);

	ii.hbmMask = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, nullptr);

	HICON hIcon = CreateIconIndirect(&ii);

	// 清理 mask，color 位图由外部管理，不要删除
	if (ii.hbmMask) {
		DeleteObject(ii.hbmMask);
	}

	return hIcon;
}

static HBITMAP GetIconFromPathAsBitmap(const std::wstring& path) {
	std::wstring actualPath = path;

	if (EndsWith(path, L".lnk")) {
		actualPath = GetShortcutTarget(path);
	}

	HICON hIcon = GetFileIcon(actualPath, true);
	HBITMAP hBitmap = IconToBitmap(hIcon);
	DestroyIcon(hIcon);
	return hBitmap;
}

// 从 shell32.dll 加载指定索引的图标为 HBITMAP
inline HBITMAP LoadShell32IconAsBitmap(int index, int cx = 48, int cy = 48) {
	HICON hIcon = LoadShell32IconHQ(index, cx, cy);
	if (!hIcon) return nullptr;
	HBITMAP hBitmap = IconToBitmap2(hIcon); // 使用IconToBitmap2保留透明度
	DestroyIcon(hIcon); // 释放HICON资源
	return hBitmap;
}


// Helper: 获取 PNG 编码器 CLSID
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;

	Gdiplus::ImageCodecInfo* pImageCodecInfo = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
	if (!pImageCodecInfo) return -1;

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT i = 0; i < num; ++i) {
		if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[i].Clsid;
			free(pImageCodecInfo);
			return i;
		}
	}

	free(pImageCodecInfo);
	return -1;
}

static std::vector<BYTE> HBitmapToByteArray(HBITMAP hBitmap) {
	std::vector<BYTE> buffer;
	if (!hBitmap) return buffer;

	Gdiplus::Bitmap bmp(hBitmap, nullptr);
	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);

	IStream* pStream = nullptr;
	CreateStreamOnHGlobal(nullptr, TRUE, &pStream);

	bmp.Save(pStream, &pngClsid, nullptr);

	STATSTG stat;
	pStream->Stat(&stat, STATFLAG_NONAME);
	const ULONG size = static_cast<ULONG>(stat.cbSize.QuadPart);

	buffer.resize(size);
	const LARGE_INTEGER liZero = {};
	pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);
	ULONG read = 0;
	pStream->Read(buffer.data(), size, &read);
	pStream->Release();

	return buffer;
}


/**
 * 系统图像列表的索引
 */
static int GetSysImageIndex2(const std::wstring& filePath) {
	if (filePath.empty()) {
		return -1;
	}
	SHFILEINFOW sfi = {};
	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
					SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
	return sfi.iIcon;
}

static bool GetShortcutIconSource(const std::wstring& shortcut, std::wstring& iconPath, int& iconNumber) {
	const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	IShellLinkW* link = nullptr;
	bool found = false;
	if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
		IID_IShellLinkW, reinterpret_cast<void**>(&link)))) {
		IPersistFile* persist = nullptr;
		if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist)))) {
			if (SUCCEEDED(persist->Load(shortcut.c_str(), STGM_READ))) {
				WCHAR path[MAX_PATH] = {};
				if (SUCCEEDED(link->GetIconLocation(path, MAX_PATH, &iconNumber)) && path[0]) {
					iconPath = path;
					found = true;
				} else {
					WIN32_FIND_DATAW data = {};
					iconNumber = 0;
					if (SUCCEEDED(link->GetPath(path, MAX_PATH, &data, SLGP_RAWPATH)) && path[0]) {
						iconPath = path;
						found = true;
					}
				}
			}
			persist->Release();
		}
		link->Release();
	}
	if (SUCCEEDED(init)) CoUninitialize();
	if (found && iconPath.find(L'%') != std::wstring::npos) {
		WCHAR expanded[32768] = {};
		const DWORD length = ExpandEnvironmentStringsW(iconPath.c_str(), expanded, 32768);
		if (length > 0 && length <= 32768) iconPath = expanded;
	}
	return found;
}

static int AddExtractedIconToSystemLists(const std::wstring& source, const int iconNumber) {
	IImageList* lists[3] = {};
	constexpr int sizes[3] = {SHIL_SMALL, SHIL_LARGE, SHIL_EXTRALARGE};
	int count = -1;
	bool ready = true;
	for (int i = 0; i < 3; ++i) {
		if (FAILED(SHGetImageList(sizes[i], IID_IImageList, reinterpret_cast<void**>(&lists[i])))) {
			ready = false;
			break;
		}
		int currentCount = 0;
		if (FAILED(lists[i]->GetImageCount(&currentCount))) {
			ready = false;
			break;
		}
		if (count < 0) count = currentCount;
		if (currentCount != count) {
			ready = false;
			break;
		}
	}
	if (!ready || count < 0) {
		for (auto* list : lists) if (list) list->Release();
		return -1;
	}

	HICON icons[3] = {};
	constexpr UINT dimensions[3] = {16, 32, 48};
	bool extracted = true;
	for (int i = 0; i < 3; ++i) {
		extracted = SUCCEEDED(SHDefExtractIconW(source.c_str(), iconNumber, 0, &icons[i], nullptr, dimensions[i])) && icons[i];
		if (!extracted) break;
	}
	int result = -1;
	if (extracted) {
		int inserted[3] = {};
		bool added = true;
		for (int i = 0; i < 3; ++i) {
			added = SUCCEEDED(lists[i]->ReplaceIcon(-1, icons[i], &inserted[i])) && inserted[i] == count;
			if (!added) break;
		}
		if (added) result = count;
	}
	for (auto icon : icons) if (icon) DestroyIcon(icon);
	for (auto* list : lists) if (list) list->Release();
	return result;
}

static int GetSysImageIndex(const std::wstring& filePath) {
	if (filePath.empty()) return -1;

	static std::mutex iconMutex;
	static std::unordered_map<std::wstring, int> extractedIcons;

	SHFILEINFOW sfi = {};
	const DWORD attributes = GetFileAttributesW(filePath.c_str());
	const bool exists = attributes != INVALID_FILE_ATTRIBUTES;
	const UINT flags = SHGFI_SYSICONINDEX | (exists ? 0 : SHGFI_USEFILEATTRIBUTES);
	if (!SHGetFileInfoW(filePath.c_str(), exists ? attributes : FILE_ATTRIBUTE_NORMAL,
		&sfi, sizeof(sfi), flags)) return -1;

	const bool shortcut = EndsWithIgnoreCase(filePath, L".lnk");
	const bool executable = EndsWithIgnoreCase(filePath, L".exe");
	const bool iconFile = EndsWithIgnoreCase(filePath, L".ico");
	if (!exists || (!shortcut && !executable && !iconFile)) return sfi.iIcon;

	// Some machines cache an executable's real icon as the generic application icon.
	static const int genericIndex = [] {
		SHFILEINFOW generic = {};
		SHGetFileInfoW(L"missing_candylauncher_icon.exe", FILE_ATTRIBUTE_NORMAL,
			&generic, sizeof(generic), SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
		return generic.iIcon;
	}();
	if (sfi.iIcon != genericIndex) return sfi.iIcon;
	std::lock_guard<std::mutex> lock(iconMutex);
	if (const auto it = extractedIcons.find(filePath); it != extractedIcons.end()) return it->second;

	std::wstring source = filePath;
	int iconNumber = 0;
	if (shortcut && !GetShortcutIconSource(filePath, source, iconNumber)) return sfi.iIcon;
	if (GetFileAttributesW(source.c_str()) == INVALID_FILE_ATTRIBUTES) return sfi.iIcon;
	if (shortcut && iconNumber == 0) {
		SHFILEINFOW sourceInfo = {};
		if (SHGetFileInfoW(source.c_str(), FILE_ATTRIBUTE_NORMAL, &sourceInfo, sizeof(sourceInfo),
			SHGFI_SYSICONINDEX) && sourceInfo.iIcon != genericIndex) {
			return sourceInfo.iIcon;
		}
	}
	const int extractedIndex = AddExtractedIconToSystemLists(source, iconNumber);
	if (extractedIndex < 0) return sfi.iIcon;
	extractedIcons.emplace(filePath, extractedIndex);
	return extractedIndex;
}

inline void RefreshIconCache(const std::wstring& filePath) {
	// 通知系统刷新该文件路径的图标
	SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH, filePath.c_str(), NULL);
}

// 从 shell32.dll 加载指定索引的图标，可以获取 16 和 32尺寸
static HBITMAP LoadShell32IconProperAsBitmap(int index, int cx = 16, int cy = 16) {
	HICON hSmall = NULL;
	HICON hLarge = NULL;
	ExtractIconExW(L"shell32.dll", index, &hLarge, &hSmall, 1);
	if (cx <= 16) return IconToBitmap2(hSmall);
	return IconToBitmap2(hLarge);
}


// #include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")


static HBITMAP LoadPngAsHBITMAP(
    const wchar_t* absolutePath,
    UINT targetWidth,
    UINT targetHeight
) {
using Microsoft::WRL::ComPtr;
    if (!absolutePath || targetWidth == 0 || targetHeight == 0)
        return nullptr;

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        absolutePath,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder
    );
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICBitmapSource> source = frame;

    ComPtr<IWICBitmapScaler> scaler;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) return nullptr;

    hr = scaler->Initialize(
        source.Get(),
        targetWidth,
        targetHeight,
        WICBitmapInterpolationModeFant
    );
    if (FAILED(hr)) return nullptr;

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(
        scaler.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return nullptr;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(targetWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(targetHeight); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(
        nullptr,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0
    );

    if (!hBitmap || !bits)
        return nullptr;

    const UINT stride = targetWidth * 4;
    const UINT imageSize = stride * targetHeight;

    hr = converter->CopyPixels(
        nullptr,
        stride,
        imageSize,
        static_cast<BYTE*>(bits)
    );

    if (FAILED(hr)) {
        DeleteObject(hBitmap);
        return nullptr;
    }

    return hBitmap;
}

#include <lunasvg.h>

static HBITMAP LoadSvgAsHBITMAP(
    const wchar_t* absolutePath,
    UINT targetWidth,
    UINT targetHeight
) {
    if (!absolutePath || targetWidth == 0 || targetHeight == 0)
        return nullptr;

    auto document = lunasvg::Document::loadFromFile(wide_to_utf8(absolutePath));
    if (!document)
        return nullptr;

    auto bitmap = document->renderToBitmap(targetWidth, targetHeight);
    if (bitmap.isNull())
        return nullptr;

    // Create top-down 32bpp HBITMAP.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(targetWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(targetHeight); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(
        nullptr,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0
    );

    if (!hBitmap || !bits) {
        return nullptr;
    }

    // lunasvg uses ARGB32 premultiplied (BGRA in memory on little-endian)
    // which matches Windows DIB format.
    const int stride = bitmap.stride();
    const int dstStride = targetWidth * 4;
    for (UINT y = 0; y < targetHeight; ++y) {
        memcpy(static_cast<BYTE*>(bits) + y * dstStride, bitmap.data() + y * stride, dstStride);
    }

    return hBitmap;
}


static HBITMAP CopyHBitmap(HBITMAP src)
{
	if (!src) return nullptr;

	return static_cast<HBITMAP>(
		CopyImage(
			src,
			IMAGE_BITMAP,
			0,
			0,
			LR_CREATEDIBSECTION
		)
	);
}

static void LogBitmapInfo(HBITMAP iconBitmap) {
	if (iconBitmap) {
		BITMAP bmp = {};
		int ret = GetObject(iconBitmap, sizeof(BITMAP), &bmp);

		if (ret != 0) {
			Logi(L"BitmapUtil", L"Bitmap 有效\t宽度: \t", bmp.bmWidth, L"\t高度: \t", bmp.bmHeight, L"\t每像素位数: \t", bmp.bmBitsPixel);
		} else {
			Loge(L"BitmapUtil", L"GetObject 失败");
		}
	} else {
		Loge(L"BitmapUtil", L"iconBitmap 是 NULL");
	}
}
