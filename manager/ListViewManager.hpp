#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <memory>
#include <sstream>
#include <array>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <commoncontrols.h>
#include <gdiplus.h>

#include "../common/GlobalState.hpp"
#include <string>
#include <functional>

#include "TextMatchHelper.hpp"
#include "plugins/BaseLaunchAction.hpp"
#include "plugins/PluginManager.hpp"
#include "util/ColorUtil.hpp"
#include "util/FontUtil.hpp"
#include "util/PaintProfiler.hpp"
#include "util/IconScaleUtil.hpp"
#include "BackgroundLayerPainter.hpp"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

// Static variable definitions
inline HIMAGELIST g_listFileImageList = nullptr;
inline bool g_measureInputDisplayPending = false;
inline Gdiplus::Color g_listViewBackgroundColor(255, 255, 255, 255);

constexpr UINT WM_LISTVIEW_SYSTEM_ICONS_READY = WM_USER + 108;

// 把图像列表中的某一项分别画到纯白和纯黑背景上，两边都与背景完全相同则该项是空白（全透明）
static bool IsImageListEntryBlank(HIMAGELIST list, const int index, const int cx, const int cy) {
	if (!list || index < 0 || cx <= 0 || cy <= 0) return false;
	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = cx;
	info.bmiHeader.biHeight = -cy;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	const int pixelCount = cx * cy;
	for (const BYTE fill : {static_cast<BYTE>(0xFF), static_cast<BYTE>(0x00)}) {
		void* bits = nullptr;
		HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!bitmap || !bits) {
			if (bitmap) DeleteObject(bitmap);
			return false;
		}
		HDC dc = CreateCompatibleDC(nullptr);
		if (!dc) {
			DeleteObject(bitmap);
			return false;
		}
		HGDIOBJ old = SelectObject(dc, bitmap);
		memset(bits, fill, static_cast<size_t>(pixelCount) * 4);
		ImageList_Draw(list, index, dc, 0, 0, ILD_TRANSPARENT);
		GdiFlush();
		const DWORD background = fill ? 0x00FFFFFF : 0x00000000;
		const auto* pixels = static_cast<const DWORD*>(bits);
		bool differs = false;
		for (int i = 0; i < pixelCount; ++i) {
			if ((pixels[i] & 0x00FFFFFF) != background) {
				differs = true;
				break;
			}
		}
		SelectObject(dc, old);
		DeleteDC(dc);
		DeleteObject(bitmap);
		if (differs) return false;
	}
	return true;
}

// 个别 exe 的 48px 图标资源本身是空白的（例如 MDAC SDK 的 RowsetViewer.exe，其 48x48 项整张被蒙版遮住），
// ForceImagePresent 成功但 SHIL_EXTRALARGE 画出来全透明，而 16/32px 正常（索引管理器用小图标所以能显示）。
// 主列表只画 SHIL_EXTRALARGE，所以发现空白时用 SHIL_LARGE / SHIL_SMALL 的图标拉伸替换该索引（只影响本进程的图像列表）
static void RepairBlankExtraLargeSystemIcon(IImageList2* extraLarge, const int index) {
	if (!extraLarge || index < 0) return;
	static std::mutex repairMutex;
	static std::unordered_set<int> checkedIndices;
	{
		std::lock_guard<std::mutex> lock(repairMutex);
		if (!checkedIndices.insert(index).second) return;
	}
	int cx = 0, cy = 0;
	if (FAILED(extraLarge->GetIconSize(&cx, &cy))) return;
	// HIMAGELIST 就是 IImageList*，项目里 GetSystemImageListEx 也是这样转换的
	if (!IsImageListEntryBlank(reinterpret_cast<HIMAGELIST>(static_cast<IImageList*>(extraLarge)), index, cx, cy)) return;

	for (const int smaller : {SHIL_LARGE, SHIL_SMALL}) {
		IImageList* source = nullptr;
		if (FAILED(SHGetImageList(smaller, IID_IImageList, reinterpret_cast<void**>(&source))) || !source) continue;
		int sx = 0, sy = 0;
		HICON icon = nullptr;
		const bool usable = SUCCEEDED(source->GetIconSize(&sx, &sy)) &&
			!IsImageListEntryBlank(reinterpret_cast<HIMAGELIST>(source), index, sx, sy) &&
			SUCCEEDED(source->GetIcon(index, ILD_TRANSPARENT, &icon)) && icon;
		source->Release();
		if (!usable) {
			if (icon) DestroyIcon(icon);
			continue;
		}
		int replaced = -1;
		const HRESULT hr = extraLarge->ReplaceIcon(index, icon, &replaced);
		DestroyIcon(icon);
		if (SUCCEEDED(hr)) {
			Logi(L"ListView", L"blank 48px system icon repaired from SHIL ", smaller, L", index=", index);
			return;
		}
	}
	Loge(L"ListView", L"blank 48px system icon could not be repaired, index=", index);
}

// Shell image lists resolve some entries lazily. Resolving them from WM_DRAWITEM
// makes the first result paint much slower, so warm them on a background thread.
// 主窗口 DPI 不是 96 时，系统图标还要放大到 LISTITEM_ICON_SIZE：也在这里的工作线程上生成缩放位图并缓存
//（util/IconScaleUtil.hpp），绘制线程只做一次 AlphaBlend。
class ListViewSystemIconWarmer {
public:
	ListViewSystemIconWarmer() {
		for (size_t i = 0; i < workers.size(); ++i)
			workers[i] = std::thread(&ListViewSystemIconWarmer::run, this, i);
	}

	~ListViewSystemIconWarmer() {
		{
			std::lock_guard<std::mutex> lock(mutex);
			stopping = true;
			++requestedGeneration;
			tasks.clear();
		}
		condition.notify_all();
		waitCondition.notify_all();
		for (auto& worker : workers) {
			if (worker.joinable()) worker.join();
		}
		clearScaledIconsLocked();
	}

	// 列表图标的绘制尺寸变化（DPI 变化）时调用：丢掉旧尺寸的缩放缓存，并让可见项重新排队生成
	void setIconSize(const int size) {
		std::lock_guard<std::mutex> lock(mutex);
		if (size == iconSize) return;
		iconSize = size;
		clearScaledIconsLocked();
		++requestedGeneration;
		tasks.clear();
		requestedActions.clear();
		readyIndices.clear();
		pendingVisibleCount = 0;
	}

	void reset(HWND listView, const std::vector<std::shared_ptr<BaseAction>>& actions) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			++requestedGeneration;
			listViewWindow = listView;
			tasks.clear();
			requestedActions.clear();
			readyIndices.clear();
			pendingVisibleCount = 0;
		}
		requestVisible(listView, actions);
	}

	void requestVisible(HWND listView, const std::vector<std::shared_ptr<BaseAction>>& actions) {
		if (!listView || actions.empty()) return;

		const int topIndex = std::max(0, ListView_GetTopIndex(listView));
		const int countPerPage = std::max(1, ListView_GetCountPerPage(listView));
		const size_t begin = std::min(static_cast<size_t>(topIndex), actions.size());
		const size_t end = std::min(begin + static_cast<size_t>(countPerPage) + 1, actions.size());
		bool queued = false;
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (stopping || listView != listViewWindow) return;
			for (size_t i = begin; i < end; ++i) {
				const auto& action = actions[i];
				if (!action || action->getIconBitmap() || !requestedActions.insert(action).second) continue;
				// 标记需要等待的可见项数量
				tasks.push_back({requestedGeneration, listView, static_cast<int>(i), action, true});
				++pendingVisibleCount;
				queued = true;
			}
		}
		if (queued) condition.notify_all();
	}

	// 阻塞 UI 线程最多 timeoutMs 毫秒，等待当前首屏可见项图标就绪
	void waitForVisible(int timeoutMs) {
		std::unique_lock<std::mutex> lock(mutex);
		if (pendingVisibleCount == 0 || tasks.empty()) return;

		waitCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
			return pendingVisibleCount == 0 || stopping;
		});
	}

	void cancel(HWND listView) {
		std::lock_guard<std::mutex> lock(mutex);
		if (listView != listViewWindow) return;
		++requestedGeneration;
		listViewWindow = nullptr;
		tasks.clear();
		requestedActions.clear();
		readyIndices.clear();
		pendingVisibleCount = 0;
		waitCondition.notify_all();
	}

	// index：系统图标索引（-1 = 未知文件图标）；scaled：按当前 LISTITEM_ICON_SIZE 缩放好的位图，
	// 只在需要缩放且已生成时非空，由本对象持有，仅在 UI 线程绘制期间使用
	bool tryGet(const std::shared_ptr<BaseAction>& action, int& index, HBITMAP& scaled) const {
		std::lock_guard<std::mutex> lock(mutex);
		const auto found = readyIndices.find(action);
		if (found == readyIndices.end()) return false;
		index = found->second;
		scaled = nullptr;
		if (!scaledIcons.empty()) {
			const auto bitmap = scaledIcons.find(index >= 0 ? index : unknown_file_icon_index);
			if (bitmap != scaledIcons.end()) scaled = bitmap->second;
		}
		return true;
	}

	bool isCurrent(const UINT generation) const {
		std::lock_guard<std::mutex> lock(mutex);
		return generation == requestedGeneration;
	}

private:
	struct Task {
		UINT generation;
		HWND listView;
		int itemIndex;
		std::shared_ptr<BaseAction> action;
		bool isVisibleItem = false;
	};

	void clearScaledIconsLocked() {
		for (auto& entry : scaledIcons) {
			if (entry.second) DeleteObject(entry.second);
		}
		scaledIcons.clear();
	}

	// 系统图标需要缩放且尚未缓存时生成缩放位图（在工作线程上），并放入缓存
	void ensureScaledIcon(IImageList2* extraLarge, IImageList* jumbo, const int entrySize, const int index) {
		if (!extraLarge || index < 0 || entrySize <= 0) return;
		int wantSize = 0;
		{
			std::lock_guard<std::mutex> lock(mutex);
			wantSize = iconSize;
			if (wantSize == entrySize || scaledIcons.count(index) != 0) return;
		}
		HBITMAP scaled = CreateScaledSystemIconBitmap(jumbo, extraLarge, index, wantSize);
		if (!scaled) {
			Logw(L"ListView", L"scaled system icon failed, index=", index, L", size=", wantSize);
			return;
		}
		std::lock_guard<std::mutex> lock(mutex);
		if (wantSize != iconSize || scaledIcons.count(index) != 0) {
			DeleteObject(scaled);
			return;
		}
		// 缓存无上限会随搜索过的条目不断增长，超过阈值时整体清空（可见项会在下次绘制时重新排队）
		if (scaledIcons.size() >= kMaxScaledIcons) clearScaledIconsLocked();
		scaledIcons.emplace(index, scaled);
	}

	void run(const size_t workerIndex) {
		// Shell 快捷方式的图标解析依赖 STA；MTA 下 SHGetFileInfoW 可能只返回空白文件图标。
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		// 工作线程在静态初始化阶段就启动了，此时 SHGetImageList 可能失败（实测两个线程里有一个拿不到列表），
		// 所以列表在处理任务时按需获取，失败则下次再试
		IImageList2* imageList = nullptr;
		// 缩放用：48px 列表的实际项尺寸，以及 256px 的 jumbo 列表（缩小比放大清晰）
		int entryWidth = 0, entryHeight = 0;
		IImageList* jumboList = nullptr;
		bool unknownIconForced = false;
		const auto acquireImageLists = [&]() {
			if (!imageList) SHGetImageList(SHIL_EXTRALARGE, IID_IImageList2, reinterpret_cast<void**>(&imageList));
			if (imageList && entryWidth <= 0) imageList->GetIconSize(&entryWidth, &entryHeight);
			if (!jumboList) SHGetImageList(SHIL_JUMBO, IID_IImageList, reinterpret_cast<void**>(&jumboList));
			if (workerIndex == 0 && imageList && !unknownIconForced && unknown_file_icon_index >= 0) {
				imageList->ForceImagePresent(unknown_file_icon_index, ILFIP_ALWAYS);
				unknownIconForced = true;
			}
		};
		acquireImageLists();

		for (;;) {
			Task task{};
			{
				std::unique_lock<std::mutex> lock(mutex);
				condition.wait(lock, [this] { return stopping || !tasks.empty(); });
				if (stopping) break;
				task = std::move(tasks.front());
				tasks.pop_front();
			}

			acquireImageLists();
			const int index = task.action->getIconFilePathIndex();
			if (imageList && index >= 0) {
				imageList->ForceImagePresent(index, ILFIP_ALWAYS);
				RepairBlankExtraLargeSystemIcon(imageList, index);
			}
			ensureScaledIcon(imageList, jumboList, entryWidth, index >= 0 ? index : unknown_file_icon_index);

			bool notifyWait = false;
			{
				std::lock_guard<std::mutex> lock(mutex);
				if (task.isVisibleItem && pendingVisibleCount > 0) {
					--pendingVisibleCount;
					if (pendingVisibleCount == 0) notifyWait = true;
				}
				if (stopping || task.generation != requestedGeneration || task.listView != listViewWindow) {
					if (notifyWait) waitCondition.notify_all();
					continue;
				}
				readyIndices.emplace(task.action, index);
			}

			if (notifyWait) {
				waitCondition.notify_all();
			}

			if (IsWindow(task.listView))
				PostMessageW(task.listView, WM_LISTVIEW_SYSTEM_ICONS_READY, task.generation,
							 static_cast<LPARAM>(task.itemIndex));
		}
		if (jumboList) jumboList->Release();
		if (imageList) imageList->Release();
		if (SUCCEEDED(comResult)) CoUninitialize();
	}

	static constexpr size_t kMaxScaledIcons = 2048;

	mutable std::mutex mutex;
	std::condition_variable condition;
	std::condition_variable waitCondition; // 用于 16ms 等待的条件变量
	std::array<std::thread, 2> workers;
	std::deque<Task> tasks;
	bool stopping = false;
	UINT requestedGeneration = 0;
	size_t pendingVisibleCount = 0;        // 当前首屏未完成的任务计数
	HWND listViewWindow = nullptr;
	std::unordered_set<std::shared_ptr<BaseAction>> requestedActions;
	std::unordered_map<std::shared_ptr<BaseAction>, int> readyIndices;
	// 当前列表图标绘制尺寸；与系统图像列表项尺寸不同时，scaledIcons 按系统图标索引缓存缩放后的预乘 DIB
	int iconSize = LISTITEM_ICON_BASE_SIZE;
	std::unordered_map<int, HBITMAP> scaledIcons;
};

inline ListViewSystemIconWarmer g_listViewSystemIconWarmer;

inline bool listViewFontsInitialized = false;
// 字体行高在资源初始化时测量一次，避免每个 item 绘制时都调用 Font::GetHeight
inline float g_listItemFontHeight1 = 0.f;
inline float g_listItemFontHeight2 = 0.f;
inline float g_listItemFontHeightSelected1 = 0.f;
inline float g_listItemFontHeightSelected2 = 0.f;
// 副标题的换行/省略号格式，所有 item 共用
inline std::unique_ptr<Gdiplus::StringFormat> g_listItemSubtitleFormat;

// 列表项离屏缓冲（item 大小，所有行复用）：同一块预乘像素内存既选入 GDI 内存 DC（BitBlt/AlphaBlend/图标），
// 也被 GDI+ Bitmap 包装（文字、纯色混合），两者交替使用时要分别 GdiFlush / Graphics::Flush
struct ListItemBackBuffer {
	HDC memDc = nullptr;
	HBITMAP bitmap = nullptr;
	void* bits = nullptr;
	int width = 0;
	int height = 0;
	std::unique_ptr<Gdiplus::Bitmap> wrapper;

	bool ensure(const int w, const int h) {
		if (bitmap && width == w && height == h) return true;
		reset();
		if (w <= 0 || h <= 0) return false;
		BITMAPINFO info{};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = w;
		info.bmiHeader.biHeight = -h; // 自上而下
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!bitmap || !bits) {
			reset();
			return false;
		}
		memDc = CreateCompatibleDC(nullptr);
		if (!memDc) {
			reset();
			return false;
		}
		SelectObject(memDc, bitmap);
		wrapper = std::make_unique<Gdiplus::Bitmap>(w, h, w * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
		if (wrapper->GetLastStatus() != Gdiplus::Ok) {
			reset();
			return false;
		}
		width = w;
		height = h;
		return true;
	}

	void reset() {
		wrapper.reset();
		if (memDc) {
			DeleteDC(memDc);
			memDc = nullptr;
		}
		if (bitmap) {
			DeleteObject(bitmap);
			bitmap = nullptr;
		}
		bits = nullptr;
		width = 0;
		height = 0;
	}
};

inline ListItemBackBuffer g_listItemBackBuffer;

typedef HRESULT (WINAPI *SHGetImageListPtr)(int iImageList, REFIID riid, void** ppv);

static int getListItemFontStyle(const std::string& styleKey,
								const std::string& boldKey,
								const std::string& italicKey,
								const std::string& underlineKey,
								const bool defaultBold = true) {
	int style = Gdiplus::FontStyleRegular;
	if (g_skinJson.contains(styleKey)) {
		std::string styleValue = MyToLower(g_skinJson.value(styleKey, ""));
		std::stringstream ss(styleValue);
		std::string token;
		while (std::getline(ss, token, '|')) {
			token = MyTrim(token);
			if (token == "bold") {
				style |= Gdiplus::FontStyleBold;
			} else if (token == "it" || token == "italic") {
				style |= Gdiplus::FontStyleItalic;
			} else if (token == "underline") {
				style |= Gdiplus::FontStyleUnderline;
			}
		}
	} else {
		if (g_skinJson.value(boldKey, defaultBold)) {
			style |= Gdiplus::FontStyleBold;
		}
		if (g_skinJson.value(italicKey, false)) {
			style |= Gdiplus::FontStyleItalic;
		}
		if (g_skinJson.value(underlineKey, false)) {
			style |= Gdiplus::FontStyleUnderline;
		}
	}
	return style;
}

static void listViewInitializeGraphicsResources() {
	listViewFontsInitialized = true;
	std::wstring itemFontFamilySelected1 = L"Segoe UI";
	std::wstring itemFontFamilySelected2 = L"Segoe UI";
	std::wstring itemFontFamily1 = L"Segoe UI";
	std::wstring itemFontFamily2 = L"Segoe UI";
	std::string itemFontColorSelected1 = "#000000";
	std::string itemFontColorSelected2 = "#666666";
	std::string itemFontColor1 = "#000000";
	std::string itemFontColor2 = "#666666";
	std::string itemBgColor = "#FFFFFF";
	std::string itemBgColorSelected = "#DDDDDD";
	int itemFontStyle1 = Gdiplus::FontStyleBold;
	int itemFontStyle2 = Gdiplus::FontStyleBold;
	int itemFontStyleSelected1 = Gdiplus::FontStyleBold;
	int itemFontStyleSelected2 = Gdiplus::FontStyleBold;


	// 列表图标尺寸随主窗口 DPI 放大；图标缩放缓存与之同步（尺寸没变时是空操作）
	LISTITEM_ICON_SIZE = MainDpiScale(LISTITEM_ICON_BASE_SIZE);
	g_listViewSystemIconWarmer.setIconSize(LISTITEM_ICON_SIZE);

	if (g_skinJson != nullptr) {
		g_listViewBackgroundColor = ApplySkinWindowOpacity(HexToGdiplusColor(
			validateHexColor(g_skinJson.value("listview_bg_color", ""), "#FFFFFF")));
		// 皮肤里的位置都是 96 DPI 下的值，按主窗口 DPI 换算
		g_itemIconSelectedX = MainDpiScale(g_skinJson.value("item_icon_selected_x", 4));
		g_itemIconSelectedY = MainDpiScale(g_skinJson.value("item_icon_selected_y", 4));
		g_itemIconX = MainDpiScale(g_skinJson.value("item_icon_x", 4));
		g_itemIconY = MainDpiScale(g_skinJson.value("item_icon_y", 4));
		g_itemTextPosX1 = MainDpiScale(g_skinJson.value("item_text_x_1", 60));
		g_itemTextPosY1 = MainDpiScale(g_skinJson.value("item_text_y_1", 4));
		g_itemTextPosX2 = MainDpiScale(g_skinJson.value("item_text_x_2", 60));
		g_itemTextPosY2 = MainDpiScale(g_skinJson.value("item_text_y_2", 22));
		g_windowBgColor = g_skinJson.value("window_bg_color", "#FFFFFF");

		g_itemTextPosSelectedX1 = MainDpiScale(g_skinJson.value("item_text_selected_x_1", 60));
		g_itemTextPosSelectedY1 = MainDpiScale(g_skinJson.value("item_text_selected_y_1", 4));
		g_itemTextPosSelectedX2 = MainDpiScale(g_skinJson.value("item_text_selected_x_2", 60));
		g_itemTextPosSelectedY2 = MainDpiScale(g_skinJson.value("item_text_selected_y_2", 22));

		itemBgColor = g_skinJson.value("item_bg_color", "#FFFFFF");
		itemBgColorSelected = g_skinJson.value("item_bg_color_selected", "#DDDDDD");

		itemFontColor1 = g_skinJson.value("item_font_color_1", "#FFFFFF");
		itemFontColor2 = g_skinJson.value("item_font_color_2", "#DDDDDD");
		itemFontColorSelected1 = g_skinJson.value("item_font_color_selected_1", "#FFFFFF");
		itemFontColorSelected2 = g_skinJson.value("item_font_color_selected_2", "#DDDDDD");

		itemFontFamily1 = utf8_to_wide(g_skinJson.value("item_font_family_1", "Segoe UI"));
		itemFontFamily2 = utf8_to_wide(g_skinJson.value("item_font_family_2", "Segoe UI"));
		itemFontFamilySelected1 = utf8_to_wide(g_skinJson.value("item_font_family_selected_1", "Segoe UI"));
		itemFontFamilySelected2 = utf8_to_wide(g_skinJson.value("item_font_family_selected_2", "Segoe UI"));

		g_item_font_size_1 = g_skinJson.value("item_font_size_1", 14.0);
		g_item_font_size_2 = g_skinJson.value("item_font_size_2", 12.0);
		g_item_font_size_selected_1 = g_skinJson.value("item_font_size_selected_1", 14.0);
		g_item_font_size_selected_2 = g_skinJson.value("item_font_size_selected_2", 12.0);

		itemFontStyle1 = getListItemFontStyle("item_font_style_1", "item_font_bold_1", "item_font_italic_1",
											"item_font_underline_1");
		itemFontStyle2 = getListItemFontStyle("item_font_style_2", "item_font_bold_2", "item_font_italic_2",
											"item_font_underline_2");
		itemFontStyleSelected1 = getListItemFontStyle("item_font_style_selected_1", "item_font_bold_selected_1",
													"item_font_italic_selected_1",
													"item_font_underline_selected_1");
		itemFontStyleSelected2 = getListItemFontStyle("item_font_style_selected_2", "item_font_bold_selected_2",
													"item_font_italic_selected_2",
													"item_font_underline_selected_2");
	} else {
	}

	Gdiplus::Color itemBgGdiplusColor = ApplySkinWindowOpacity(HexToGdiplusColor(itemBgColor));
	Gdiplus::Color itemBgGdiplusColorSelected = ApplySkinWindowOpacity(HexToGdiplusColor(itemBgColorSelected));

	Gdiplus::Color itemFontGdiplusColor1 = HexToGdiplusColor(itemFontColor1);
	Gdiplus::Color itemFontGdiplusColor2 = HexToGdiplusColor(itemFontColor2);
	Gdiplus::Color itemFontGdiplusColorSelected1 = HexToGdiplusColor(itemFontColorSelected1);
	Gdiplus::Color itemFontGdiplusColorSelected2 = HexToGdiplusColor(itemFontColorSelected2);

	g_listItemBgColorBrush = std::make_unique<Gdiplus::SolidBrush>(itemBgGdiplusColor);
	g_listItemBgColorBrushSelected = std::make_unique<Gdiplus::SolidBrush>(itemBgGdiplusColorSelected);

	g_listItemTextColorBrush1 = std::make_unique<Gdiplus::SolidBrush>(itemFontGdiplusColor1);
	g_listItemTextColorBrush2 = std::make_unique<Gdiplus::SolidBrush>(itemFontGdiplusColor2);
	g_listItemTextColorBrushSelected1 = std::make_unique<Gdiplus::SolidBrush>(itemFontGdiplusColorSelected1);
	g_listItemTextColorBrushSelected2 = std::make_unique<Gdiplus::SolidBrush>(itemFontGdiplusColorSelected2);

	// 皮肤字体在当前系统不存在时（如 Windows 7 没有 Microsoft YaHei UI）强制退回到一定存在的字体，
	// GDI+ 不会自己替换缺失字体。字号是 96 DPI 下的像素值，按主窗口 DPI 放大
	g_listItemFont1 = CreateGdiplusFont(itemFontFamily1, MainDpiScaleF(g_item_font_size_1),
										itemFontStyle1, Gdiplus::UnitPixel);
	g_listItemFont2 = CreateGdiplusFont(itemFontFamily2, MainDpiScaleF(g_item_font_size_2),
										itemFontStyle2, Gdiplus::UnitPixel);
	g_listItemFontSelected1 = CreateGdiplusFont(itemFontFamilySelected1, MainDpiScaleF(g_item_font_size_selected_1),
												itemFontStyleSelected1, Gdiplus::UnitPixel);
	g_listItemFontSelected2 = CreateGdiplusFont(itemFontFamilySelected2, MainDpiScaleF(g_item_font_size_selected_2),
												itemFontStyleSelected2, Gdiplus::UnitPixel);

	// 行高与屏幕 DPI 相关，用屏幕 DC 测一次即可（列表窗口与屏幕 DPI 一致）
	if (const HDC screenDc = GetDC(nullptr)) {
		Gdiplus::Graphics graphics(screenDc);
		g_listItemFontHeight1 = g_listItemFont1->GetHeight(&graphics);
		g_listItemFontHeight2 = g_listItemFont2->GetHeight(&graphics);
		g_listItemFontHeightSelected1 = g_listItemFontSelected1->GetHeight(&graphics);
		g_listItemFontHeightSelected2 = g_listItemFontSelected2->GetHeight(&graphics);
		ReleaseDC(nullptr, screenDc);
	}

	g_listItemSubtitleFormat = std::make_unique<Gdiplus::StringFormat>();
	g_listItemSubtitleFormat->SetTrimming(Gdiplus::StringTrimmingEllipsisWord); // 以单词为单位的省略号
	g_listItemSubtitleFormat->SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit); // 限制行数
}

// 皮肤更新后重新合成列表的背景图层（主窗口背景裁剪 + 列表背景图/背景色）。
// 必须在 listViewInitializeGraphicsResources 和控件定位之后调用
static void RebuildListViewBackgroundLayer(const HWND hWnd) {
	if (g_skinJson == nullptr || !hWnd || !IsWindow(hWnd)) {
		g_listViewBgLayer.reset();
		return;
	}
	RECT windowRect{};
	GetWindowRect(hWnd, &windowRect);
	createChildBackgroundLayer(g_listViewBgLayer, hWnd, windowRect.right - windowRect.left,
								windowRect.bottom - windowRect.top, g_listViewBgImage, g_listViewBackgroundColor);
}

static void refreshAppLaunchAction() {
	baseAppLaunchActions.clear();
	auto base_launch_action1 = std::make_shared<BaseLaunchAction>(LoadI18nString(IDS_I18N_ACTION_REFRESH_LIST_TITLE, L"Refresh List").c_str(),
																						LoadI18nString(IDS_I18N_ACTION_REFRESH_LIST_DESC, L"Rebuild the index list").c_str(), L"",
																appLaunchActionCallBacks["refreshList"]);
	// 直接按当前列表图标尺寸加载资源图标，高 DPI 下不需要再拉伸
	base_launch_action1->iconBitmap = g_pluginManager->LoadResIconAsBitmap(IDI_REFRESH, LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE);
	g_matchTextCache.FillAll(*base_launch_action1, base_launch_action1->getTitle());
	base_launch_action1->usageKey = ComputeUsageKey(L"host", L"", base_launch_action1->getTitle(),
													base_launch_action1->getSubTitle());
	// auto base_launch_action2 = std::make_shared<BaseLaunchAction>(L"刷新插件", L"重新加载所有插件动作", L"",
																// appLaunchActionCallBacks["refreshPlugins"]);
	// base_launch_action2->matchText = PinyinHelper::GetPinyinWithVariants(MyToLower(base_launch_action2->getTitle()));
	// base_launch_action2->iconBitmap = g_pluginManager->LoadResIconAsBitmap(IDI_REFRESH, 48, 48);

	baseAppLaunchActions.push_back(base_launch_action1);
	// baseAppLaunchActions.push_back(base_launch_action2);
	// 主程序自己的条目变了，合并列表需要重建
	PluginManager::BumpActionsVersion();
}

// 列表图标尺寸变化（DPI 变化）后，把主程序自己条目的资源图标按新尺寸重新加载；条目对象本身不变，列表无需重建
static void reloadHostActionIcons() {
	if (!g_pluginManager) return;
	for (const auto& action : baseAppLaunchActions) {
		const auto launchAction = std::dynamic_pointer_cast<BaseLaunchAction>(action);
		if (!launchAction || !launchAction->iconBitmap) continue;
		DeleteObject(launchAction->iconBitmap);
		launchAction->iconBitmap = g_pluginManager->LoadResIconAsBitmap(IDI_REFRESH, LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE);
	}
}

static void listViewCleanupGraphicsResources() {
	// 重置所有 unique_ptr 资源
	g_listItemBgColorBrush.reset();
	g_listItemBgColorBrushSelected.reset();

	g_listItemTextColorBrush1.reset();
	g_listItemTextColorBrush2.reset();
	g_listItemTextColorBrushSelected1.reset();
	g_listItemTextColorBrushSelected2.reset();

	g_listItemFont1.reset();
	g_listItemFont2.reset();
	g_listItemFontSelected1.reset();
	g_listItemFontSelected2.reset();
	g_listItemSubtitleFormat.reset();

	listViewFontsInitialized = false;
}

static void NavigateListView(const UINT vk) {
	const int count = ListView_GetItemCount(g_listViewHwnd);
	if (count == 0) return;

	int selected = ListView_GetNextItem(g_listViewHwnd, -1, LVNI_SELECTED);
	if (selected == -1) selected = 0;
	selected = vk == VK_DOWN ? (selected + 1) % count : (selected - 1 + count) % count;

	ListView_SetItemState(g_listViewHwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	ListView_SetItemState(g_listViewHwnd, selected, LVIS_SELECTED | LVIS_FOCUSED,
							LVIS_SELECTED | LVIS_FOCUSED);
	ListView_EnsureVisible(g_listViewHwnd, selected, FALSE);
}

static bool HandleListNavigationHotkey(const UINT vk, const UINT modifiers) {
	if (MatchesListNavigationHotkey("pref_hotkey_list_up", vk, modifiers)) {
		NavigateListView(VK_UP);
		return true;
	}
	if (MatchesListNavigationHotkey("pref_hotkey_list_down", vk, modifiers)) {
		NavigateListView(VK_DOWN);
		return true;
	}
	return false;
}

static ULONGLONG g_lastScrollTick = 0;
static bool g_scrollbarForceVisible = false;

constexpr UINT_PTR TIMER_TRIGGER_HIDE_PAINT = 0x9003;
constexpr ULONGLONG HIDE_DELAY_MS = 2000;

// 更新时间并标记当前处于可见激活期
static void OnUserScroll(HWND hWnd) {
	if (!g_listViewHideScrollbar)
	{
		return;
	}
	g_lastScrollTick = GetTickCount64();
	g_scrollbarForceVisible = true;
	ShowScrollBar(hWnd, SB_VERT, TRUE);
	SetTimer(hWnd, TIMER_TRIGGER_HIDE_PAINT, HIDE_DELAY_MS, nullptr);
}

// 使listview监听esc键
static LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam,
											UINT_PTR, const DWORD_PTR dwRefData) {
	switch (message) {
	case WM_PAINT:
		g_listViewSystemIconWarmer.requestVisible(hWnd, filteredActions);
		if (g_measureInputDisplayPending) {
			MethodTimerEnd(L"test_input_to_paint");
			MethodTimerStart(L"test_list_paint");
		}
		if (g_listViewHideScrollbar && !g_scrollbarForceVisible &&
			GetTickCount64() - g_lastScrollTick >= HIDE_DELAY_MS) {
			ShowScrollBar(hWnd, SB_VERT, FALSE);
		}
		{
			g_listPaintStats.reset();
			const PaintStopwatch stopwatch;
			const LRESULT result = DefSubclassProc(hWnd, message, wParam, lParam);
			PaintProfileLog(L"list paint total ", PaintProfileFormatMs(stopwatch.elapsedMs()),
							L" ms, bg ", PaintProfileFormatMs(g_listPaintStats.backgroundMs),
							L" ms, items ", g_listPaintStats.items,
							L" (itemBg ", PaintProfileFormatMs(g_listPaintStats.itemBgMs),
							L" ms, text ", PaintProfileFormatMs(g_listPaintStats.textMs),
							L" ms, icon ", PaintProfileFormatMs(g_listPaintStats.iconMs), L" ms)");
			if (g_measureInputDisplayPending) {
				g_measureInputDisplayPending = false;
				MethodTimerEnd(L"test_list_paint");
				MethodTimerEnd(L"===test_input_duration");
			}
			return result;
		}
	case WM_TIMER:
		if (wParam == TIMER_TRIGGER_HIDE_PAINT) {
			g_scrollbarForceVisible = false;
			KillTimer(hWnd, TIMER_TRIGGER_HIDE_PAINT);
			InvalidateRect(hWnd, nullptr, FALSE);
			return 0;
		}
		break;
	case WM_KEYDOWN: break;
	case WM_MBUTTONUP: TimerIDSetFocusEdit = SetTimer(GetParent(hWnd), TIMER_SETFOCUS_EDIT, 10, nullptr); // 10 毫秒延迟
		break;
	case WM_LISTVIEW_REFRESH_RESOURCE:
		{
			const int oldIconSize = LISTITEM_ICON_SIZE;
			listViewCleanupGraphicsResources();
			listViewInitializeGraphicsResources(); // 确保字体和画刷已初始化
			if (oldIconSize != LISTITEM_ICON_SIZE) reloadHostActionIcons();
			RebuildListViewBackgroundLayer(hWnd);
		}
		break;
	case WM_LISTVIEW_SYSTEM_ICONS_READY:
		if (g_listViewSystemIconWarmer.isCurrent(static_cast<UINT>(wParam))) {
			const int itemIndex = static_cast<int>(lParam);
			RECT itemRect{};
			if (ListView_GetItemRect(hWnd, itemIndex, &itemRect, LVIR_BOUNDS)) {
				const bool selected = (ListView_GetItemState(hWnd, itemIndex, LVIS_SELECTED) & LVIS_SELECTED) != 0;
				const int iconX = selected ? g_itemIconSelectedX : g_itemIconX;
				const int iconY = selected ? g_itemIconSelectedY : g_itemIconY;
				const RECT iconRect = {
					itemRect.left + iconX,
					itemRect.top + iconY,
					itemRect.left + iconX + LISTITEM_ICON_SIZE,
					itemRect.top + iconY + LISTITEM_ICON_SIZE
				};
				InvalidateRect(hWnd, &iconRect, FALSE);
			}
		}
		return 0;
	case WM_MOUSEWHEEL:
		{
			// 在Shift+滚动上键时，会触发列表的NM_DBLCLK,不知怎么回事，干脆直接屏蔽这个组合
			const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if (shiftDown) {
				return 0;
			}
			OnUserScroll(hWnd);
		}
		break;
	case WM_VSCROLL:
		OnUserScroll(hWnd);
		break;
	case WM_NCDESTROY:
		KillTimer(hWnd, TIMER_TRIGGER_HIDE_PAINT);
		g_listViewSystemIconWarmer.cancel(hWnd);
		break;
	default: break;
	}
	return DefSubclassProc(hWnd, message, wParam, lParam);
}

static HIMAGELIST GetSystemImageListEx(const int size) {
	SHFILEINFO sfi = {};
	SHGetFileInfo(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

	HMODULE hShell32 = LoadLibraryW(L"Shell32.dll");
	if (!hShell32) return nullptr;

	SHGetImageListPtr pSHGetImageList = reinterpret_cast<SHGetImageListPtr>(GetProcAddress(hShell32, "SHGetImageList"));
	if (!pSHGetImageList) {
		FreeLibrary(hShell32);
		return nullptr;
	}

	IImageList* pImageList = nullptr;
	if (const HRESULT hr = pSHGetImageList(size, __uuidof(IImageList), reinterpret_cast<void**>(&pImageList)); FAILED(hr)) {
		FreeLibrary(hShell32);
		return nullptr;
	}

	// Release handle after use — or detach to HIMAGELIST
	HIMAGELIST hImageList = nullptr;
	pImageList->QueryInterface(__uuidof(IImageList), reinterpret_cast<void**>(&hImageList));

	pImageList->Release();
	FreeLibrary(hShell32);
	return hImageList;
}

// 未用到
static HIMAGELIST GetSystemImageList(bool largeIcon) {
	SHFILEINFOW sfi = {};
	//UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | (largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON);
	constexpr UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_OPENICON;

	// SHGetFileInfo 返回值是图像列表句柄，sfi.hIcon 是图标句柄，不要搞错
	return reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags));
}


static void listViewInitialize(HWND parent, HINSTANCE hInstance, const int x, const int y, const int width,
								const int height) {
	// 初始化 Common Controls
	INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
	InitCommonControlsEx(&icex);

	const DWORD style = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_OWNERDRAWFIXED |
		WS_VSCROLL |ES_AUTOVSCROLL|
		LVS_NOCOLUMNHEADER | LVS_OWNERDATA;
	g_listViewHwnd = CreateWindowExW(0, WC_LISTVIEW, L"", style,
									x, y, width, height, parent, reinterpret_cast<HMENU>(2), hInstance, nullptr);
	// Do not enable LVS_EX_DOUBLEBUFFER here. This list view is owner-drawn and
	// mixes GDI+ item painting with GDI/AlphaBlend icon painting. The native
	// double-buffer path loses those icons when selection invalidates individual
	// rows; a full repaint (for example, paging) only makes them reappear.

	// 设置列 0（主标题），即使我们不显示它，也必须添加
	LVCOLUMN col = {LVCF_TEXT | LVCF_WIDTH};
	col.pszText = const_cast<LPWSTR>(L"Title");
	col.cx = 0;
	ListView_InsertColumn(g_listViewHwnd, 0, &col);

	//hImageList = GetSystemImageList(true);
	g_listFileImageList = GetSystemImageListEx(SHIL_EXTRALARGE); // 使用 48x48 图标

	ListView_SetImageList(g_listViewHwnd, g_listFileImageList, LVSIL_NORMAL);
	listViewInitializeGraphicsResources(); // 确保字体和画刷已初始化
}



static void addActionDone() {
	// 连续输入没有匹配项时，列表内容没有变化，无需每次重置图标队列并整窗重绘。
	if (filteredActions.empty() && ListView_GetItemCount(g_listViewHwnd) == 0) return;
	ListView_SetItemCountEx(g_listViewHwnd, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	if (!filteredActions.empty()) {
		// 清除所有项的选中状态
		//ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		// 设置第一项为选中状态
		ListView_SetItemState(g_listViewHwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		// 确保第一项可见
		ListView_EnsureVisible(g_listViewHwnd, 0, FALSE);
	}
	g_listViewSystemIconWarmer.reset(g_listViewHwnd, filteredActions);
	// 强行等待最多 16ms，能拿到多少图标就拿多少
	g_listViewSystemIconWarmer.waitForVisible(20);
	InvalidateRect(g_listViewHwnd, nullptr, FALSE);
}

// PluginManager::ActionsVersion() the merged allActions list was built from; 0 = must rebuild.
inline uint64_t g_allActionsBuiltVersion = 0;

static void clearVisibleActionReferences() {
	filteredActions.clear();
	allActions.clear();
	g_allActionsBuiltVersion = 0;
	if (g_listViewHwnd) {
		ListView_SetItemCountEx(g_listViewHwnd, 0, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		g_listViewSystemIconWarmer.reset(g_listViewHwnd, filteredActions);
		InvalidateRect(g_listViewHwnd, nullptr, FALSE);
	}
}

// 编辑框筛选列表内容
inline void actionFilter(const std::wstring& keyword) {
	filteredActions.clear();
	// 合并列表只在插件发布/优先级/加载状态变化后重建，而不是每次按键
	const uint64_t version = PluginManager::ActionsVersion();
	if (version != g_allActionsBuiltVersion || allActions.empty()) {
		allActions.clear();
		allActions = baseAppLaunchActions;
		if (g_pluginManager) g_pluginManager->AppendPluginActions(allActions);
		g_allActionsBuiltVersion = version;
	}

	// ConsolePrintln(L"项目数量" + std::to_wstring(allActions.size()));
	TextMatch(keyword, allActions, filteredActions);
#if defined(DEBUG) || defined(_DEBUG)
	{
		std::wstring top;
		for (size_t i = 0; i < filteredActions.size() && i < 8; ++i) {
			if (i) top += L" | ";
			top += filteredActions[i]->getTitle();
		}
		Logi(L"TextMatch", L"query='", keyword, L"' candidates=", allActions.size(), L" results=",
			filteredActions.size(), L" top: ", top);
	}
#endif
	// 关键方法，用于提交列表数据更新
	addActionDone();
}

// Results a plugin delivered through post_results for the query currently shown (UI thread).
// CL_RESULTS_REPLACE swaps the whole list (e.g. a "searching..." placeholder), otherwise the batch is appended.
inline void appendQueryResults(const uint64_t queryId, std::vector<std::shared_ptr<BaseAction>>&& results, const int32_t flags) {
	if (queryId != PluginManager::CurrentQueryId() || !g_listViewHwnd) return;
	const bool replace = (flags & CL_RESULTS_REPLACE) != 0;
	if (results.empty() && !replace) return;
	const bool wasEmpty = filteredActions.empty() || replace;
	if (replace) filteredActions = std::move(results);
	else {
		filteredActions.insert(filteredActions.end(), std::make_move_iterator(results.begin()),
								std::make_move_iterator(results.end()));
	}
	ListView_SetItemCountEx(g_listViewHwnd, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	if (wasEmpty && !filteredActions.empty()) {
		ListView_SetItemState(g_listViewHwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		ListView_EnsureVisible(g_listViewHwnd, 0, FALSE);
	}
	g_listViewSystemIconWarmer.reset(g_listViewHwnd, filteredActions);
	if (IsWindowVisible(g_listViewHwnd)) InvalidateRect(g_listViewHwnd, nullptr, FALSE);
}

static void listViewMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem) {
	lpMeasureItem->itemHeight = g_listItemHeight;
}

// listview自定义数据时，必须提供的数据分配方法
static void listViewCustomDataOnGetDispInfo(NMLVDISPINFOW* pdi) {
	const int index = pdi->item.iItem;

	// 安全检查，确保索引在 filteredActions 的范围内
	if (index < 0 || static_cast<size_t>(index) >= filteredActions.size()) {
		return; // 无效索引，不处理
	}

	// 获取对应的数据项
	const auto& action = filteredActions[index];

	// 根据 ListView 请求的信息 (mask) 来提供数据
	// 注意：因为我们是 OwnerDraw，ListView 不会请求 LVIF_TEXT，但为了完整性，最好写上
	if (pdi->item.mask & LVIF_TEXT) {
		// 如果你需要显示文本（比如在非OwnerDraw的列中），就这样做：
		// wcsncpy_s(pdi->item.pszText, pdi->item.cchTextMax, action->GetTitle().c_str(), _TRUNCATE);
	}

	// ListView 会请求图像索引
	if (pdi->item.mask & LVIF_IMAGE) {
		int iconIndex = -1;
		HBITMAP scaledIcon = nullptr;
		pdi->item.iImage = g_listViewSystemIconWarmer.tryGet(action, iconIndex, scaledIcon) && iconIndex >= 0
			? iconIndex
			: I_IMAGECALLBACK;
	}

	// 如果有其他状态（如选中），也可以在这里设置
	// if (pdi->item.mask & LVIF_STATE) { ... }
}

// 通过一个 thread_local 存储 Gdiplus::Graphics 缓存，避免每次都重新创建
static Gdiplus::Graphics* GetCachedGraphics(HDC hdc) {
	thread_local std::unique_ptr<Gdiplus::Graphics> cachedGraphics;
	static HDC lastDC = nullptr;

	if (!cachedGraphics || lastDC != hdc) {
		cachedGraphics = std::make_unique<Gdiplus::Graphics>(hdc);
		lastDC = hdc;
		cachedGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
	}

	return cachedGraphics.get();
}

// 在合适的地方释放资源，如在窗口销毁时调用
static void listViewCleanup() {
	// 释放 ListView 的图像列表
	if (g_listFileImageList) {
		ImageList_Destroy(g_listFileImageList);
		g_listFileImageList = nullptr;
	}

	// 清理 GDI+ 资源
	listViewCleanupGraphicsResources();
	g_listItemBackBuffer.reset();
}

static void setUnknownFileIcon(HDC hdc, const RECT rc) {
	static HICON hIcon = nullptr;

	// 获取系统的未知文件图标
	if (hIcon == nullptr) {
		SHFILEINFOW shfi = {0};
		const UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON;
		if (SHGetFileInfoW(L"non_existent.ico", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), flags)) {
			hIcon = shfi.hIcon;
		}
	}

	if (hIcon) {
		DrawIconEx(hdc, rc.left + 4, rc.top + 4, hIcon, LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE, 0, nullptr, DI_NORMAL);
	} else {
		// 获取失败，绘制一个灰色占位图标
		RECT iconRect = {rc.left + 4, rc.top + 4, rc.left + 20, rc.top + 20};
		FillRect(hdc, &iconRect, (HBRUSH)(COLOR_GRAYTEXT + 1));
	}
}

static void listViewDrawItem(const DRAWITEMSTRUCT* lpDrawItem) {
	const UINT index = lpDrawItem->itemID;
	if (index < 0 || index >= static_cast<int>(filteredActions.size())) return;

	std::shared_ptr<BaseAction> action = filteredActions[index];
	const HDC hdc = lpDrawItem->hDC;
	const RECT& rc = lpDrawItem->rcItem;

	const int itemWidth = g_listItemWidth > 0 ? g_listItemWidth : (rc.right - rc.left);
	const int itemHeight = g_listItemHeight > 0 ? g_listItemHeight : (rc.right - rc.left);
	const UINT isSelected = lpDrawItem->itemState & ODS_SELECTED;
	++g_listPaintStats.items;
	PaintStopwatch stageStopwatch;

	// 离屏合成：窗口表面是 DWM 重定向表面，GDI+ 在上面做 alpha 混合和文字抗锯齿都要回读像素，很慢。
	// 所以先把列表背景在本行位置的那一块复制到系统内存缓冲，在缓冲里叠项背景、文字、图标，最后一次 BitBlt 回窗口。
	// 没有列表背景图层（皮肤尚未加载）时退回直接在窗口上绘制。
	const bool offscreen = g_listViewBgLayer && g_listItemBackBuffer.ensure(itemWidth, itemHeight);
	const HDC drawDc = offscreen ? g_listItemBackBuffer.memDc : hdc;
	const int originX = offscreen ? 0 : rc.left;
	const int originY = offscreen ? 0 : rc.top;
	if (offscreen) {
		// 先清零再复制，行比列表窗口宽时超出的部分保持透明
		std::memset(g_listItemBackBuffer.bits, 0, static_cast<size_t>(itemWidth) * itemHeight * 4);
		BitBlt(drawDc, 0, 0, itemWidth, itemHeight, g_listViewBgLayer.memDc, rc.left, rc.top, SRCCOPY);
	}
	// 函数任何一处返回前都把缓冲复制回窗口（图标分支里有提前 return）
	struct BackBufferPresenter {
		const bool enabled;
		const HDC target;
		const RECT& rc;
		const int width;
		const int height;

		~BackBufferPresenter() {
			if (!enabled) return;
			BitBlt(target, rc.left, rc.top, width, height, g_listItemBackBuffer.memDc, 0, 0, SRCCOPY);
		}
	} presenter{offscreen, hdc, rc, itemWidth, itemHeight};

	// 0. GDI+ 图形对象：离屏时建在与缓冲共享内存的 PARGB 位图上（这样混合和文字 alpha 才正确），否则建在窗口 DC 上
	std::unique_ptr<Gdiplus::Graphics> graphicsHolder = offscreen
		? std::make_unique<Gdiplus::Graphics>(g_listItemBackBuffer.wrapper.get())
		: std::make_unique<Gdiplus::Graphics>(hdc);
	Gdiplus::Graphics& graphics = *graphicsHolder;
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintAntiAliasGridFit);

	// 1. 项背景：背景图已在皮肤加载时缩放到 item 大小并转成预乘 DIB，这里只做一次 GDI AlphaBlend
	const Gdiplus::Rect rect(originX, originY, itemWidth, itemHeight);
	const PremultipliedDibLayer& itemBgLayer = isSelected ? g_listItemBgLayerSelected : g_listItemBgLayer;
	Gdiplus::Image* itemBgImage = isSelected ? g_listItemBgImageSelected : g_listItemBgImage;
	if (itemBgLayer) {
		alphaBlendDibLayer(drawDc, itemBgLayer, originX, originY);
	} else if (itemBgImage) {
		// 兜底：图片已经是 item 大小，1:1 复制即可
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
		graphics.DrawImage(itemBgImage, rect);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeDefault);
	} else {
		graphics.FillRectangle(isSelected ? g_listItemBgColorBrushSelected.get() : g_listItemBgColorBrush.get(), rect);
	}
	// GDI 对 DIB 段的绘制会被批处理，切到 GDI+ 之前先刷掉，保证文字画在背景之上
	GdiFlush();
	if constexpr (kPaintProfileEnabled) {
		graphics.Flush(Gdiplus::FlushIntentionSync);
		g_listPaintStats.itemBgMs += stageStopwatch.elapsedMs();
		stageStopwatch = PaintStopwatch();
	}

	// 2. 文字
	Gdiplus::RectF titleRect;
	Gdiplus::RectF subRect;
	if (isSelected) {
		const float titleHeight = g_listItemFontHeightSelected1;
		const float subHeight = g_listItemFontHeightSelected2 * 2.2f; // 限制为两行
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(originX + g_itemTextPosSelectedX1),
									static_cast<Gdiplus::REAL>(originY + g_itemTextPosSelectedY1),
									static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosSelectedX1),
									titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(originX + g_itemTextPosSelectedX2),
								static_cast<Gdiplus::REAL>(originY + g_itemTextPosSelectedY2),
								static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosSelectedX2), subHeight);
	} else {
		const float titleHeight = g_listItemFontHeight1;
		const float subHeight = g_listItemFontHeight2 * 2.2f; // 限制为两行
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(originX + g_itemTextPosX1),
									static_cast<Gdiplus::REAL>(originY + g_itemTextPosY1),
									static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosX1), titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(originX + g_itemTextPosX2),
								static_cast<Gdiplus::REAL>(originY + g_itemTextPosY2),
								static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosX2), subHeight);
	}
	const std::wstring& title = action->getTitle();
	const std::wstring& subtitle = action->getSubTitle();

	// 副标题换行/省略号格式在资源初始化时创建（g_listItemSubtitleFormat）
	const Gdiplus::StringFormat* subtitleFormat = g_listItemSubtitleFormat.get();

	if (isSelected) {
		graphics.DrawString(title.c_str(), -1, g_listItemFontSelected1.get(), titleRect, nullptr,
							g_listItemTextColorBrushSelected1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFontSelected2.get(), subRect, subtitleFormat,
							g_listItemTextColorBrushSelected2.get());
	} else {
		graphics.DrawString(title.c_str(), -1, g_listItemFont1.get(), titleRect, nullptr,
							g_listItemTextColorBrush1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFont2.get(), subRect, subtitleFormat,
							g_listItemTextColorBrush2.get());
	}

	// 图标用 GDI 绘制，先把 GDI+ 的待处理命令刷掉，否则延迟刷新的文字/背景会盖住图标
	graphics.Flush(Gdiplus::FlushIntentionSync);
	if constexpr (kPaintProfileEnabled) {
		g_listPaintStats.textMs += stageStopwatch.elapsedMs();
		stageStopwatch = PaintStopwatch();
	}
	struct IconStageTimer {
		const PaintStopwatch& stopwatch;
		~IconStageTimer() { g_listPaintStats.iconMs += stopwatch.elapsedMs(); }
	} iconStageTimer{stageStopwatch};

	// 3. 图标
	const int iconX = originX + (isSelected ? g_itemIconSelectedX : g_itemIconX);
	const int iconY = originY + (isSelected ? g_itemIconSelectedY : g_itemIconY);
	// 32-bpp 带 alpha 的 HBITMAP（插件图标或已缩放的系统图标），用 AlphaBlend 按像素混合；
	// 源尺寸取位图实际大小，与 LISTITEM_ICON_SIZE 不一致时由 AlphaBlend 拉伸（缩放缓存就绪后两者相等）
	const auto drawIconBitmap = [&](const HBITMAP bitmap) {
		BITMAP bm{};
		if (!bitmap || !GetObject(bitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) return;
		HDC hMemDC = CreateCompatibleDC(drawDc);
		HGDIOBJ oldBmp = SelectObject(hMemDC, bitmap);
		BLENDFUNCTION blendFunc{};
		blendFunc.BlendOp = AC_SRC_OVER;
		blendFunc.SourceConstantAlpha = 255; // 使用源位图自身的 alpha 通道
		blendFunc.AlphaFormat = AC_SRC_ALPHA;
		AlphaBlend(drawDc, iconX, iconY, LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE,
					hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, blendFunc);
		SelectObject(hMemDC, oldBmp);
		DeleteDC(hMemDC);
	};
	if (const HBITMAP iconBitmap = action->getIconBitmap()) {
		const HBITMAP scaledBitmap = action->getIconBitmapScaled(LISTITEM_ICON_SIZE);
		drawIconBitmap(scaledBitmap ? scaledBitmap : iconBitmap);
	} else {
		int iconIndex = -1;
		HBITMAP scaledIcon = nullptr;
		if (!g_listViewSystemIconWarmer.tryGet(action, iconIndex, scaledIcon)) {
			return;
		}
		if (scaledIcon) {
			drawIconBitmap(scaledIcon);
		} else if (g_listFileImageList && iconIndex >= 0) {
			ImageList_Draw(g_listFileImageList, iconIndex, drawDc, iconX, iconY, ILD_TRANSPARENT);
		} else {
			ImageList_Draw(g_listFileImageList, unknown_file_icon_index, drawDc, iconX, iconY, ILD_TRANSPARENT);
		}
	}
}

// 选中状态变化时，ListView 只会重绘列宽范围内的区域；
// item 宽度可能大于列宽（列宽扣除了滚动条宽度），需要把整行宽度一起重绘，
// 否则右侧滚动条区域会残留旧的选中/未选中背景
static void listViewOnItemChanged(const NMLISTVIEW* pnmv) {
	if (!(pnmv->uChanged & LVIF_STATE) || ((pnmv->uOldState ^ pnmv->uNewState) & LVIS_SELECTED) == 0) return;
	HWND hWnd = pnmv->hdr.hwndFrom;
	if (pnmv->iItem < 0) {
		InvalidateRect(hWnd, nullptr, FALSE);
		return;
	}
	RECT itemRect{};
	if (!ListView_GetItemRect(hWnd, pnmv->iItem, &itemRect, LVIR_BOUNDS)) return;
	RECT clientRect{};
	GetClientRect(hWnd, &clientRect);
	itemRect.left = clientRect.left;
	itemRect.right = clientRect.right;
	InvalidateRect(hWnd, &itemRect, FALSE);
}

static LRESULT listViewOnCustomDraw(LPNMLVCUSTOMDRAW lplvcd) {
	// We are only interested in the stage before the whole control is painted.
	if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
		// 这个判断很重要，用来判断这次是要重绘listview背景，还是重绘item，用来解决item和listview绘制冲突
		if (lplvcd->dwItemType != 2) {
			return CDRF_NOTIFYITEMDRAW;
		}
		HDC hdc = lplvcd->nmcd.hdc;
		HWND hWnd = lplvcd->nmcd.hdr.hwndFrom;
		RECT rcClient;
		GetClientRect(hWnd, &rcClient);
		const PaintStopwatch stopwatch;

		if (g_listViewBgLayer) {
			// 预合成好的背景（主窗口背景裁剪 + 列表背景）一次 BitBlt 覆盖，含 alpha，超出客户区的部分被裁掉
			blitDibLayer(hdc, g_listViewBgLayer, rcClient.left, rcClient.top,
						g_listViewBgLayer.width, g_listViewBgLayer.height, 0, 0);
			g_listPaintStats.backgroundMs += stopwatch.elapsedMs();
			return CDRF_NOTIFYITEMDRAW;
		}

		Gdiplus::Graphics graphics(hdc);
		graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
		// 不知道为什么 列表的左边和上边会多空出一个像素
		if (g_skinJson != nullptr) {
			const Gdiplus::Rect rect(rcClient.left - 1, rcClient.top - 1, rcClient.right - rcClient.left + 1,
									rcClient.bottom - rcClient.top + 1);
			// 纯色不透明列表背景已经覆盖主窗口，无需先合成下方背景。
			if (g_listViewBgCachedBitmap || g_listViewBgImage || g_listViewBackgroundColor.GetA() != 255)
				paintMainWindowBackgroundUnderChild(hWnd, graphics, rcClient);
			if (g_listViewBgCachedBitmap) {
				graphics.DrawCachedBitmap(g_listViewBgCachedBitmap, rcClient.left, rcClient.top);
			} else if (g_listViewBgImage) {
				graphics.DrawImage(g_listViewBgImage, rect);
			} else {
				const Gdiplus::SolidBrush bgBrush(g_listViewBackgroundColor);
				graphics.FillRectangle(&bgBrush, rect);
			}
		}
		if constexpr (kPaintProfileEnabled) {
			graphics.Flush(Gdiplus::FlushIntentionSync);
			g_listPaintStats.backgroundMs += stopwatch.elapsedMs();
		}

		// 告诉我们已经处理了图纸，它不应进行默认背景绘画。它仍将发送有关项目的通知。
		return CDRF_NOTIFYITEMDRAW;
	}

	// 对于其他阶段，让默认处理发生。
	return CDRF_DODEFAULT;
}

static void refreshPluginRunner() {
	refreshAppLaunchAction();
	// loadRunnerJsonConfig();

	// 添加插件动作
	if (g_pluginManager) {
		// g_pluginManager->RefreshAllActions();
		// auto pluginActions = g_pluginManager->GetAllActions();
		// for (const auto& action : pluginActions)
		// {
		// auto pluginRunAction = std::make_shared<RunCommandAction>(
		// 	(action.title),
		// 	(action.id),
		// 	(action.subTitle),
		// 	PLUGIN_ACTION
		// );

		// 调试输出：显示插件动作的标题和匹配文本
		// ConsolePrintln(
		// 	L"Plugin action added - title: " + action.title + L", matchText: " + pluginRunAction->matchText);

		// runnerActionSource.push_back(pluginRunAction);
		// }
	}

	// 保存引用，便于后续点击等操作
	// runnerActions = runnerActionSource;

	// for (size_t i = 0; i < runnerActionSource.size(); ++i) {
	// 	const std::shared_ptr<RunCommandAction> &action = runnerActionSource[i];
	// 	if (action->itemType==UWP_APP) {
	// 		// 用这个方法来加载图标的话（把hicon添加到列表，再获取索引给action），在listview显示的时候顺序会乱，不知道为什么
	// 		// if (action->iImageIndex == -1)
	// 		// {
	// 		// 	int imageIndex = ImageList_Add(hImageList, action->hIcon, nullptr);
	// 		// 	action->iImageIndex = imageIndex;
	// 		// }
	// 	} else {
	// 		// 获取图标
	// 		if (action->iImageIndex == -1)
	// 			action->iImageIndex = GetSysImageIndex(action->GetTargetPath());
	// 	}
	// }
	unknown_file_icon_index = GetSysImageIndex((EXE_FOLDER_PATH + L"\\non_existent.ico"));

	// filteredActions = runnerActionSource;
}

inline void textMatching() {
	// 通知插件用户输入变化
	std::wstring editTextBuffer2;
	if (StartsWith(editTextBuffer, LR"({"arg":")")) {
		if (const size_t end = find_json_end(editTextBuffer); end != std::wstring::npos) {
			const std::wstring json_part = editTextBuffer.substr(0, end + 1);
			bool isSuccess = false;
			try {
				nlohmann::json j = nlohmann::json::parse(wide_to_utf8(json_part));
				const std::string arg = j["arg"];
				currectActionArg = utf8_to_wide(arg);
				isSuccess = true;
			} catch (const nlohmann::json::parse_error& e) {
				Loge(L"ListViewManager", L"JSON 解析错误: ", e.what());
			}
			if (isSuccess) {
				editTextBuffer2 = editTextBuffer.substr(end + 1);
			} else {
				editTextBuffer2 = (editTextBuffer);
				currectActionArg = L"";
			}
		} else {
			editTextBuffer2 = (editTextBuffer);
			currectActionArg = L"";
		}
	} else {
		editTextBuffer2 = (editTextBuffer);
		currectActionArg = L"";
	}
	if (g_pluginManager) {
		// 每次按键是一个新的查询；之前的异步查询被取消，其迟到的结果会被丢弃
		const uint64_t queryId = g_pluginManager->BeginQuery();
		if (auto intercept = g_pluginManager->DispatchInterceptInput(editTextBuffer2);
			intercept.tookOver || !intercept.actions.empty()) {
			filteredActions = std::move(intercept.actions);
			addActionDone();
			g_pluginManager->DispatchQuery(queryId, editTextBuffer2, currectActionArg);
			return;
		}
		g_pluginManager->DispatchUserInput(editTextBuffer2);
		actionFilter(editTextBuffer2);
		g_pluginManager->DispatchQuery(queryId, editTextBuffer2, currectActionArg);
		return;
	}
	actionFilter(editTextBuffer2);
}

inline void editTextInput() {
#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG) || defined(REL_WITH_DEB_INFO_DEBUG) 
	g_measureInputDisplayPending = true;
	MethodTimerStart(L"===test_input_duration");
	MethodTimerStart(L"test_input_processing");
	MethodTimerStart(L"test_input_to_paint");
#endif

	const int textLength = GetWindowTextLengthW(g_editHwnd);
	editTextBuffer.resize(static_cast<size_t>(textLength) + 1);
	const int copiedLength = GetWindowTextW(g_editHwnd, editTextBuffer.data(), textLength + 1);
	editTextBuffer.resize(static_cast<size_t>(copiedLength));
	if (editTextBuffer.empty()) {
		clearVisibleActionReferences();
		if (IsWindowVisible(g_listViewHwnd)) {
			InvalidateRect(g_listViewHwnd, nullptr, FALSE); // 比updatewindow 快多了，完全没有多字符键入延迟问题
		}
		return;
	}
	textMatching();
	if (g_measureInputDisplayPending) {
		MethodTimerEnd(L"test_input_processing");
	}
	if (IsWindowVisible(g_listViewHwnd)) {
		InvalidateRect(g_listViewHwnd, nullptr, FALSE);
	}
}

inline void refreshVisibleActionsFromCurrentInput() {
	if (!g_editHwnd || !g_listViewHwnd) {
		return;
	}
	refreshPluginRunner();
	editTextInput();
}
