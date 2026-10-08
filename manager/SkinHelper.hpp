#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "../common/GlobalState.hpp"
#include <gdiplus.h>
#include <atomic>
#include <cstring>

#include "SkinFileUtil.hpp"
#include "BackgroundLayerPainter.hpp"
#include "../util/StringUtil.hpp"

// 监听皮肤文件改动
inline std::thread g_skinFileWatcherThread;
inline std::atomic g_shouldStop{false}; // 停止标志

// Shared variables for skin settings
inline std::vector<std::wstring> g_skinFilePaths;
inline size_t g_prefSkinIndex;

static void clearBackgroundCachedBitmaps() {
	delete g_BgCachedBitmap;
	g_BgCachedBitmap = nullptr;
	delete g_editBgCachedBitmap;
	g_editBgCachedBitmap = nullptr;
	delete g_listViewBgCachedBitmap;
	g_listViewBgCachedBitmap = nullptr;
	delete g_BgScaledBitmap;
	g_BgScaledBitmap = nullptr;
	delete g_BgImage;
	g_BgImage = nullptr;
}

static Gdiplus::CachedBitmap* createBackgroundCachedBitmap(Gdiplus::Image* image, HWND hwnd,
														const int width, const int height) {
	if (!image || !hwnd || !IsWindow(hwnd) || width <= 0 || height <= 0) return nullptr;

	HDC hdc = GetDC(hwnd);
	if (!hdc) return nullptr;

	Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppPARGB);
	Gdiplus::Graphics bitmapGraphics(&bitmap);
	bitmapGraphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	bitmapGraphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	bitmapGraphics.DrawImage(image, 0, 0, width, height);
	bitmapGraphics.Flush(Gdiplus::FlushIntentionSync);

	Gdiplus::Graphics targetGraphics(hdc);
	auto* cachedBitmap = new Gdiplus::CachedBitmap(&bitmap, &targetGraphics);
	if (cachedBitmap->GetLastStatus() != Gdiplus::Ok) {
		delete cachedBitmap;
		cachedBitmap = nullptr;
	}
	ReleaseDC(hwnd, hdc);
	return cachedBitmap;
}

// 外部变量声明
//inline std::atomic<bool> g_shouldStop;

// 判断系统是否启用了深色模式
static bool IsSystemDarkMode() {
	HKEY hKey;
	DWORD value = 0;
	DWORD dataSize = sizeof(DWORD);

	// 打开注册表键
	LONG result = RegOpenKeyExW(
		HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		0,
		KEY_READ,
		&hKey
	);

	if (result != ERROR_SUCCESS) {
		return false; // 无法打开注册表，默认返回非深色模式
	}

	// 读取 AppsUseLightTheme 值（0 = 深色模式，1 = 浅色模式）
	result = RegQueryValueExW(
		hKey,
		L"AppsUseLightTheme",
		nullptr,
		nullptr,
		reinterpret_cast<LPBYTE>(&value),
		&dataSize
	);

	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS) {
		return false; // 读取失败，默认返回非深色模式
	}

	// 返回 true 表示深色模式（value == 0），false 表示浅色模式（value == 1）
	return value == 0;
}

static void getSkinPictureFile(Gdiplus::Image*& image, const std::string& skinKey, int width, int height) {
	std::wstring picturePath = utf8_to_wide(MyTrim(g_skinJson.value(skinKey, "")));
	if (StartsWith(picturePath, L"\\\\")) {
	} else if (StartsWith(picturePath, L"\\")) {
		picturePath = utf8_to_wide(g_skinJson["skin_folder"]) + picturePath;
	}
	if (image) {
		// 删除旧的图片对象
		delete image;
		image = nullptr;
	}
	if (!picturePath.empty()) {
		if (EndsWith(picturePath, L".9.png")) {
			image = RenderNinePatchToSize(picturePath.c_str(), width, height);
			//image =CreateNinePatchBitmap(utf8_to_wide(picturePath).c_str(),MAIN_WINDOW_WIDTH,MAIN_WINDOW_HEIGHT);
		} else {
			image = new Gdiplus::Image(picturePath.c_str());
		}
		if (image->GetLastStatus() != Gdiplus::Ok) {
			delete image;
			image = nullptr;
			ShowErrorMsgBox(L"加载背景图片失败");
		}
	}
}

static void getSkinPictureFile(Gdiplus::Image*& image, const std::string& skinKey) {
	const std::string picturePath = g_skinJson.value(skinKey, "");
	if (image) {
		// 删除旧的图片对象
		delete image;
		image = nullptr;
	}
	if (!picturePath.empty()) {
		image = new Gdiplus::Image(utf8_to_wide(picturePath).c_str());
		if (image->GetLastStatus() != Gdiplus::Ok) {
			delete image;
			image = nullptr;
			ShowErrorMsgBox(L"加载背景图片失败");
		}
	}
}

// Shape the window around the visible pixels of a skin background. Window regions
// keep the existing child edit/list controls usable while making empty pixels
// outside a character illustration transparent and click-through.
static void updateSkinWindowRegion() {
	if (!g_skinJson.value("window_shape_from_bg_alpha", false) || !g_BgImage) {
		SetWindowRgn(g_mainHwnd, nullptr, TRUE);
		return;
	}

	const int width = MAIN_WINDOW_WIDTH;
	const int height = MAIN_WINDOW_HEIGHT;
	if (width <= 0 || height <= 0) return;
	const int threshold = std::clamp(g_skinJson.value("window_shape_alpha_threshold", 6), 0, 255);
	Gdiplus::Bitmap mask(width, height, PixelFormat32bppARGB);
	Gdiplus::Graphics graphics(&mask);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.DrawImage(g_BgImage, Gdiplus::Rect(0, 0, width, height));

	Gdiplus::BitmapData bitmapData{};
	Gdiplus::Rect bounds(0, 0, width, height);
	if (mask.LockBits(&bounds, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok) {
		Loge(L"SkinHelper", L"无法读取背景图片的透明区域");
		return;
	}

	std::vector<RECT> runs;
	for (int y = 0; y < height; ++y) {
		const auto* row = static_cast<const BYTE*>(bitmapData.Scan0) + y * bitmapData.Stride;
		int start = -1;
		for (int x = 0; x <= width; ++x) {
			const bool visible = x < width && row[x * 4 + 3] > threshold;
			if (visible && start < 0) start = x;
			if (!visible && start >= 0) {
				runs.push_back(RECT{start, y, x, y + 1});
				start = -1;
			}
		}
	}
	mask.UnlockBits(&bitmapData);
	if (runs.empty()) {
		Loge(L"SkinHelper", L"背景图片没有可见像素，窗口形状未更新");
		return;
	}

	const size_t byteCount = sizeof(RGNDATAHEADER) + runs.size() * sizeof(RECT);
	std::vector<BYTE> regionBytes(byteCount);
	auto* regionData = reinterpret_cast<RGNDATA*>(regionBytes.data());
	regionData->rdh.dwSize = sizeof(RGNDATAHEADER);
	regionData->rdh.iType = RDH_RECTANGLES;
	regionData->rdh.nCount = static_cast<DWORD>(runs.size());
	regionData->rdh.nRgnSize = static_cast<DWORD>(runs.size() * sizeof(RECT));
	regionData->rdh.rcBound = RECT{0, 0, width, height};
	std::memcpy(regionData->Buffer, runs.data(), runs.size() * sizeof(RECT));
	HRGN region = ExtCreateRegion(nullptr, static_cast<DWORD>(byteCount), regionData);
	if (!region) {
		Loge(L"SkinHelper", L"无法创建背景图片形状区域");
		return;
	}
	if (!SetWindowRgn(g_mainHwnd, region, TRUE)) {
		DeleteObject(region);
		Loge(L"SkinHelper", L"无法设置背景图片形状区域");
	}
}

static void UpdateListViewScrollbarStyle()
{
	if (!g_listViewHwnd || !IsWindow(g_listViewHwnd))
		return;
}


static void refreshSkin(std::wstring& skinPath, const bool isShowWindow = true) {
	skinPath = getCurrectSkinPath(skinPath);
	// 黑夜模式功能
	std::string mode = g_settings_map["pref_night_mode"].stringValue;
	if (mode == "night_mode" || (mode == "auto_recognition" && IsSystemDarkMode())) {
		skinPath = NIGHT_SKIN_PATH;
	}

	if (false) {
		ShowWindow(g_editHwnd, SW_HIDE);
		ShowWindow(g_listViewHwnd, SW_HIDE);
	}
	std::ifstream in((skinPath.data()));
	if (!in) {
		Loge(L"SkinHelper", L"文件不存在: ", skinPath);
		return;
	}

	// 读取整个文件内容（UTF-8 编码）
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string utf8json = buffer.str();

	// 解析 JSON
	try {
		g_skinJson = nlohmann::json::parse(utf8json);
		in.close();
		// 转路径
		std::filesystem::path fullPath(skinPath);
		// 存进 json
		g_skinJson["skin_path"] = std::filesystem::absolute(fullPath).u8string();
		g_skinJson["skin_folder"] = std::filesystem::absolute(fullPath.parent_path()).u8string();
	} catch (const nlohmann::json::parse_error& e) {
		Loge(L"SkinHelper", L"JSON 解析错误: ", e.what());
		return;
	}
	// --- 1. 更新主窗口 ---
	MAIN_WINDOW_WIDTH = g_skinJson.value("window_width", DEFAULT_MAIN_WINDOW_WIDTH);
	MAIN_WINDOW_HEIGHT = g_skinJson.value("window_height", DEFAULT_MAIN_WINDOW_HEIGHT);
	SetWindowPos(g_mainHwnd, nullptr, 0, 0, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);

	int windowOpacity = g_skinJson.value("window_opacity", 255);
	if (windowOpacity < 0) windowOpacity = 0;
	if (windowOpacity > 255) windowOpacity = 255;
	if (g_lastWindowOpacity != windowOpacity) {
		g_lastWindowOpacity = windowOpacity;
		SetLayeredWindowAttributes(g_mainHwnd, 0, static_cast<BYTE>(windowOpacity), LWA_ALPHA);
	}

	// --- 2. 更新编辑框 (hEdit) ---
	if (g_settings_map["pref_search_box_placeholder_use_theme"].boolValue) {
		EDIT_HINT_TEXT = utf8_to_wide(g_skinJson.value("editbox_hint_text", g_settings_map["pref_search_box_placeholder"].stringValue));
	}
	int editX = g_skinJson.value("editbox_x", 10);
	int editY = g_skinJson.value("editbox_y", 10);
	int editWidth = g_skinJson.value("editbox_width", 580);
	int editHeight = g_skinJson.value("editbox_height", 35);
	g_listItemWidth = g_skinJson.value("item_width", 580);
	g_listItemHeight = g_skinJson.value("item_height", 35);
	if (g_listItemWidth <= 0) {
		g_listItemHeight = 580;
	}
	if (g_listItemHeight <= 0) {
		g_listItemHeight = 60;
	}

	// 更新编辑框字体
	std::wstring editFontFamily = utf8_to_wide(g_skinJson.value("editbox_font_family", "宋体"));
	double editFontSize = g_skinJson.value("editbox_font_size", 12.0);
	HDC hdc = GetDC(g_editHwnd);
	HFONT hEditFont = CreateFontW(
		-MulDiv(static_cast<int>(editFontSize), GetDeviceCaps(hdc, LOGPIXELSY), 72),
		0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, editFontFamily.c_str()
	);
	ReleaseDC(g_editHwnd, hdc);

	// 初始化GDI+图形对象


	// Gdiplus::Graphics graphics(hEdit);
	// graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	// Gdiplus::FontFamily fontFamily(L"微软雅黑");
	// Gdiplus::Font gdiFont(&fontFamily, 25, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	// LOGFONTW logfont;
	// Gdiplus::Status status = gdiFont.GetLogFontW(&graphics,&logfont);
	// HFONT hFont = CreateFontIndirect(&logfont);
	// SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

	// 注意：编辑框的背景色和前景色需要在 WM_CTLCOLOREDIT 中处理

	// --- 3. 更新列表视图 (ListView) ---

	int listX = g_skinJson.value("listview_x", 10);
	int listY = g_skinJson.value("listview_y", 45);
	g_listViewWidth = g_skinJson.value("listview_width", 580);
	g_listViewHeight = g_skinJson.value("listview_height", 380);

	// CachedBitmap is tied to its target graphics device, so discard all old
	// caches before replacing the source images or changing control sizes.
	clearBackgroundCachedBitmaps();

	// 处理背景图片
	getSkinPictureFile(g_BgImage, "window_bg_picture", MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT);
	//RenderNinePatchToSize
	getSkinPictureFile(g_editBgImage, "editbox_bg_picture", editWidth, editHeight);
	getSkinPictureFile(g_listViewBgImage, "listview_bg_picture", g_listViewWidth, g_listViewHeight);
	getSkinPictureFile(g_listItemBgImage, "item_bg_picture", g_listItemWidth, g_listItemHeight);
	getSkinPictureFile(g_listItemBgImageSelected, "item_bg_picture_selected", g_listItemWidth, g_listItemHeight);
	updateSkinWindowRegion();

	if (hEditFont) {
		SendMessage(g_editHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(hEditFont), TRUE);
	}
	SendMessage(g_editHwnd, WM_NOTIFY_HEDIT_REFRESH_SKIN, 0, TRUE);
	SetWindowPos(g_listViewHwnd, nullptr, listX, listY, g_listViewWidth, g_listViewHeight, SWP_NOZORDER);
	SetWindowPos(g_editHwnd, nullptr, editX, editY, editWidth, editHeight, SWP_NOZORDER);
	
	// Each control gets a cache created against its own DC. CachedBitmap is
	// device-specific and cannot safely be shared between these windows.
	g_BgCachedBitmap = createBackgroundCachedBitmap(g_BgImage, g_mainHwnd,
													 MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT);
	g_BgScaledBitmap = createScaledBackgroundBitmap(g_BgImage, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT);
	g_editBgCachedBitmap = createBackgroundCachedBitmap(g_editBgImage, g_editHwnd,
													  editWidth, editHeight);
	g_listViewBgCachedBitmap = createBackgroundCachedBitmap(g_listViewBgImage, g_listViewHwnd,
														g_listViewWidth, g_listViewHeight);
	
	g_listViewHideScrollbar = g_skinJson.value("listview_hide_scrollbar", false);
	// 这里是调节列表item宽度重要的地方，调好了横向滚动条就不存在
	int thumbWidth = GetWindowVScrollBarThumbWidth(g_listViewHwnd, true);
	ListView_SetColumnWidth(g_listViewHwnd, 0, g_listViewWidth - thumbWidth - 0);
	SendMessage(g_listViewHwnd, WM_LISTVIEW_REFRESH_RESOURCE, 0, 0);
	UpdateListViewScrollbarStyle();
	InvalidateRect(g_listViewHwnd, nullptr, TRUE);

	// 更新列表视图颜色
	// COLORREF listBgColor = HexToCOLORREF(g_skinJson.value("listview_bg_color", "#FFFFFF"));
	// COLORREF listFontColor = HexToCOLORREF(g_skinJson.value("listview_font_color", "#000000"));
	// ListView_SetBkColor(listViewHwnd, listBgColor);
	// ListView_SetTextBkColor(listViewHwnd, listBgColor); // 文本背景色通常和列表背景色一致
	// ListView_SetTextColor(listViewHwnd, listFontColor);

	// 更新列表视图字体 (与编辑框类似)
	// 注意: ListViewManager 的自定义绘制(WM_DRAWITEM)也需要使用新的字体和颜色信息
	// 你可能需要在 ListViewManager 类中添加方法来存储这些皮肤设置

	// ListView_SetBkColor(listViewHwnd, CLR_NONE);
	// ListView_SetTextBkColor(listViewHwnd, CLR_NONE);

	if (isShowWindow) {
		// 暴力强制渲染
		RECT rc;
		GetWindowRect(g_mainHwnd, &rc);
		// 临时缩小窗口
		SetWindowPos(g_mainHwnd, nullptr, rc.left, rc.top, rc.left + 1, rc.top + 1,
					SWP_NOZORDER);
		// 恢复原大小
		SetWindowPos(g_mainHwnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
					SWP_NOZORDER);
		ShowWindow(g_mainHwnd, SW_FORCEMINIMIZE);
		SetTimer(g_mainHwnd, TIMER_SHOW_WINDOW, 50, nullptr);
	}
}

static void RefreshSkinFile() {
	auto prefSkin = std::find_if(g_settings_ui_last_save.rbegin(), g_settings_ui_last_save.rend(),
								[](const SettingItem& item) {
									return item.key == "pref_skin";
								});
	if (prefSkin == g_settings_ui_last_save.rend()) {
		return;
	}
	g_prefSkinIndex = std::distance(prefSkin, g_settings_ui_last_save.rend()) - 1;
	g_skinFilePaths = FindSkinFiles();

	prefSkin->entries.clear();
	prefSkin->entryValues.clear();

	prefSkin->entryValues.emplace_back("default");
	prefSkin->entries.emplace_back("默认");
	prefSkin->entryValues.emplace_back("night_mode");
	prefSkin->entries.emplace_back("夜间模式");

	for (const auto& path : g_skinFilePaths) {
		prefSkin->entryValues.emplace_back(wide_to_utf8(path));
		prefSkin->entries.emplace_back(wide_to_utf8(path));
	}
}


// 监听皮肤文件
static void watchSkinFile() {
	static ULONGLONG lastRefreshTimeTick = 0;
	// 获取皮肤所在目录
	std::wstring directory(EXE_FOLDER_PATH + LR"(\skins)");

	HANDLE hDir = CreateFileW(
		directory.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		nullptr
	);

	if (hDir == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		Loge(L"SkinHelper", L"无法打开目录句柄，错误代码: ", err);
		return;
	}

	// 1. (推荐) 使用 BYTE 数组作为缓冲区
	char buffer[(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH) * 2]{};
	DWORD bytesReturned;

	while (!g_shouldStop) // 线程将在此循环，检查停止标志
	{
		BOOL success = ReadDirectoryChangesW(
			hDir,
			buffer,
			sizeof(buffer),
			FALSE, // 不递归
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			&bytesReturned,
			nullptr,
			nullptr
		);

		if (!success) {
			DWORD err = GetLastError();
			if (err != ERROR_OPERATION_ABORTED) {
				// 忽略取消操作的错误
				Loge(L"SkinHelper", L"ReadDirectoryChangesW 失败，错误代码: ", err);
			}
			break; // 发生错误，退出循环
		}

		// 2. (关键修复) 只有当确实返回了数据时才处理
		if (bytesReturned > 0) {
			FILE_NOTIFY_INFORMATION* pNotify;
			size_t offset = 0;

			do {
				pNotify = (FILE_NOTIFY_INFORMATION*)((BYTE*)buffer + offset);

				// FileNameLength 是以字节为单位的，需要转换为 WCHAR 的数量
				std::wstring changedFile(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));

				// 从当前皮肤路径中提取文件名
				std::wstring currentSkinFileName = getCurrectSkinPath(g_currectSkinFilePath);

				if (const size_t pos = currentSkinFileName.find_last_of(L"\\/"); pos != std::wstring::npos) {
					currentSkinFileName = currentSkinFileName.substr(pos + 1);
				}

				// 检查变更的文件是否是当前皮肤文件
				if (_wcsicmp(changedFile.c_str(), currentSkinFileName.c_str()) == 0) {
					// 时间防抖
					if (const ULONGLONG now = GetTickCount64(); now - lastRefreshTimeTick > 400) {
						lastRefreshTimeTick = now;
						Logi(L"SkinHelper", L"Skin file modification detected: ", changedFile);
						PostMessage(g_mainHwnd, WM_REFRESH_SKIN, 0, 0);
					}
				}
				Logi(L"SkinHelper", L"Action=", pNotify->Action, L", File=", changedFile);

				// 移动到下一个通知记录
				offset += pNotify->NextEntryOffset;
			} while (pNotify->NextEntryOffset != 0);
		}
	}

	CloseHandle(hDir);
}


static void stopThreadSkinFileWatcher() {
	if (g_skinFileWatcherThread.joinable()) {
		g_shouldStop = true;
		// Windows 平台使用 CancelSynchronousIo

		HANDLE hThread = reinterpret_cast<HANDLE>(g_skinFileWatcherThread.native_handle());
		if (!CancelSynchronousIo(hThread)) {
			DWORD err = GetLastError();
			Loge(L"SkinHelper", L"CancelSynchronousIo 失败，错误码: ", err);
		}
		g_skinFileWatcherThread.join();
	}
}
