#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <memory>
// #include "ListedRunnerPlugin.h"
#include "../util/ThreadPool.hpp"
#include "../util/MainTools.hpp"
#include <shlobj.h>
#include <shlguid.h>
#include <sstream>
#include <Shobjidl.h>    // For IImageList
#include <commoncontrols.h>
#include <gdiplus.h>
#include <rapidfuzz/fuzz.hpp>

#include "../common/GlobalState.hpp"
#include <optional>
#include <execution>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <functional> // For std::greater
#include <thread>     // For std::thread::hardware_concurrency
#include <future>     // For std::async and std::future

#include "TextMatchHelper.hpp"
#include "plugins/BaseLaunchAction.hpp"
#include "plugins/PluginManager.hpp"
#include "util/ColorUtil.hpp"
#include "util/ColorUtil.hpp"
#include "util/ColorUtil.hpp"
#include "util/ColorUtil.hpp"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

// Static variable definitions
inline HIMAGELIST g_listFileImageList = nullptr;
// 用于显示的列表
inline std::vector<std::shared_ptr<BaseAction>> allActions;
inline std::vector<std::shared_ptr<BaseAction>> baseAppLaunchActions;
inline std::vector<std::shared_ptr<BaseAction>> filteredActions;

// inline HBRUSH hNormalBrush = CreateSolidBrush(COLOR_UI_BG);
// inline HBRUSH hSelectedBrush = CreateSolidBrush(COLOR_UI_BG_DEEP);

// std::unique_ptr<Gdiplus::SolidBrush> whiteBrush;
// std::unique_ptr<Gdiplus::SolidBrush> grayBrush;
// std::unique_ptr<Gdiplus::SolidBrush> blackBrush;
inline bool listViewFontsInitialized = false;

typedef HRESULT (WINAPI *SHGetImageListPtr)(int iImageList, REFIID riid, void** ppv);

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


	if (g_skinJson != nullptr) {
		g_itemIconSelectedX = g_skinJson.value("item_icon_selected_x", 4);
		g_itemIconSelectedY = g_skinJson.value("item_icon_selected_y", 4);
		g_itemIconX = g_skinJson.value("item_icon_x", 4);
		g_itemIconY = g_skinJson.value("item_icon_y", 4);
		g_itemTextPosX1 = g_skinJson.value("item_text_x_1", 60);
		g_itemTextPosY1 = g_skinJson.value("item_text_y_1", 4);
		g_itemTextPosX2 = g_skinJson.value("item_text_x_2", 60);
		g_itemTextPosY2 = g_skinJson.value("item_text_y_2", 22);

		g_itemTextPosSelectedX1 = g_skinJson.value("item_text_selected_x_1", 60);
		g_itemTextPosSelectedY1 = g_skinJson.value("item_text_selected_y_1", 4);
		g_itemTextPosSelectedX2 = g_skinJson.value("item_text_selected_x_2", 60);
		g_itemTextPosSelectedY2 = g_skinJson.value("item_text_selected_y_2", 22);

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
	} else {
	}

	Gdiplus::Color itemBgGdiplusColor = HexToGdiplusColor(itemBgColor);
	Gdiplus::Color itemBgGdiplusColorSelected = HexToGdiplusColor(itemBgColorSelected);

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

	Gdiplus::FontFamily fontFamily1(itemFontFamily1.c_str());
	Gdiplus::FontFamily fontFamily2(itemFontFamily2.c_str());
	Gdiplus::FontFamily fontFamilySelected1(itemFontFamilySelected1.c_str());
	Gdiplus::FontFamily fontFamilySelected2(itemFontFamilySelected2.c_str());
	g_listItemFont1 = std::make_unique<Gdiplus::Font>(&fontFamily1, static_cast<float>(g_item_font_size_1),
													Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	g_listItemFont2 = std::make_unique<Gdiplus::Font>(&fontFamily2, static_cast<float>(g_item_font_size_2),
													Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	g_listItemFontSelected1 = std::make_unique<Gdiplus::Font>(&fontFamilySelected1,
															static_cast<float>(g_item_font_size_selected_1),
															Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	g_listItemFontSelected2 = std::make_unique<Gdiplus::Font>(&fontFamilySelected2,
															static_cast<float>(g_item_font_size_selected_2),
															Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
}

static void refreshAppLaunchAction() {
	baseAppLaunchActions.clear();
	auto base_launch_action1 = std::make_shared<BaseLaunchAction>(L"刷新列表", L"刷新索引列表", L"",
																appLaunchActionCallBacks["refreshList"]);
	base_launch_action1->iconBitmap = g_pluginManager->LoadResIconAsBitmap(IDI_REFRESH, 48, 48);
	base_launch_action1->matchText = PinyinHelper::GetPinyinWithVariants(MyToLower(base_launch_action1->getTitle()));
	auto base_launch_action2 = std::make_shared<BaseLaunchAction>(L"刷新插件", L"重新加载所有插件动作", L"",
																appLaunchActionCallBacks["refreshPlugins"]);
	base_launch_action2->matchText = PinyinHelper::GetPinyinWithVariants(MyToLower(base_launch_action1->getTitle()));
	base_launch_action2->iconBitmap = g_pluginManager->LoadResIconAsBitmap(IDI_REFRESH, 48, 48);

	baseAppLaunchActions.push_back(base_launch_action1);
	baseAppLaunchActions.push_back(base_launch_action2);
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

	listViewFontsInitialized = false;
}

// 使listview监听esc键
static LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam,
											UINT_PTR, const DWORD_PTR dwRefData) {
	switch (message) {
	case WM_KEYDOWN: break;
	case WM_MBUTTONUP: TimerIDSetFocusEdit = SetTimer(GetParent(hWnd), TIMER_SETFOCUS_EDIT, 10, nullptr); // 10 毫秒延迟
		break;
	case WM_LISTVIEW_REFRESH_RESOURCE: listViewCleanupGraphicsResources();
		listViewInitializeGraphicsResources(); // 确保字体和画刷已初始化
		break;
	case WM_MOUSEWHEEL:
		{
			// 在Shift+滚动上键时，会触发列表的NM_DBLCLK,不知怎么回事，干脆直接屏蔽这个组合
			const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if (shiftDown) {
				return 0;
			}
		}
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
		WS_VSCROLL |
		LVS_NOCOLUMNHEADER | LVS_OWNERDATA;
	g_listViewHwnd = CreateWindowExW(0, WC_LISTVIEW, L"", style,
									x, y, width, height, parent, reinterpret_cast<HMENU>(2), hInstance, nullptr);

	// 设置列 0（主标题），即使我们不显示它，也必须添加
	LVCOLUMN col = {LVCF_TEXT | LVCF_WIDTH};
	col.pszText = const_cast<LPWSTR>(L"Title");
	col.cx = 500;
	ListView_InsertColumn(g_listViewHwnd, 0, &col);

	//hImageList = GetSystemImageList(true);
	g_listFileImageList = GetSystemImageListEx(SHIL_EXTRALARGE); // 使用 48x48 图标

	ListView_SetImageList(g_listViewHwnd, g_listFileImageList, LVSIL_NORMAL);
	listViewInitializeGraphicsResources(); // 确保字体和画刷已初始化
}


static void addActionDone() {
	ListView_SetItemCountEx(g_listViewHwnd, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	if (!filteredActions.empty()) {
		// 清除所有项的选中状态
		//ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		// 设置第一项为选中状态
		ListView_SetItemState(g_listViewHwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		// 确保第一项可见
		ListView_EnsureVisible(g_listViewHwnd, 0, FALSE);
	}
	InvalidateRect(g_listViewHwnd, nullptr, TRUE);
}

// 编辑框筛选列表内容
inline void actionFilter(const std::wstring& keyword) {
	filteredActions.clear();
	allActions.clear();
	allActions = baseAppLaunchActions;
	PluginManager::GetAllPluginActions(allActions);

	ConsolePrintln(L"项目数量" + std::to_wstring(allActions.size()));
	TextMatch(keyword, allActions, filteredActions);
	// 关键方法，用于提交列表数据更新
	addActionDone();
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
		pdi->item.iImage = (action->getIconFilePathIndex() == -1) ? unknown_file_icon_index : action->getIconFilePathIndex();
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
	HDC hdc = lpDrawItem->hDC;
	const RECT& rc = lpDrawItem->rcItem;

	// 0. 首先获取 GDI+ 图形对象，后续所有操作都基于它
	Gdiplus::Graphics graphics(hdc);
	graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
	graphics.SetInterpolationMode(Gdiplus::InterpolationMode::InterpolationModeHighQualityBicubic);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHint::TextRenderingHintAntiAliasGridFit);

	// 1. ✨ 改用 GDI+ 填充背景
	// 定义不透明的颜色 (A, R, G, B)，A=255 代表完全不透明
	// Gdiplus::Color normalBgColor(255, 255, 255, 255);       // 不透明白色
	// Gdiplus::Color selectedBgColor(255, 220, 235, 255);   // 不透明淡蓝色作为选中色

	// 创建 GDI+ 画刷
	// Gdiplus::SolidBrush bgBrush(listItemBgColor);
	// 用 GDI+ 的 FillRectangle 填充背景
	const int itemWidth = g_listItemWidth > 0 ? g_listItemWidth : (rc.right - rc.left);
	const Gdiplus::Rect rect(rc.left, rc.top, itemWidth, rc.bottom - rc.top);
	UINT isSelected = lpDrawItem->itemState & ODS_SELECTED;
	if (isSelected) {
		if (g_listItemBgImageSelected) {
			graphics.DrawImage(g_listItemBgImageSelected, rect);
		} else {
			graphics.FillRectangle(g_listItemBgColorBrushSelected.get(), rect);
		}
	} else {
		if (g_listItemBgImage) {
			graphics.DrawImage(g_listItemBgImage, rect);
		} else {
			graphics.FillRectangle(g_listItemBgColorBrush.get(), rect);
		}
	}

	// 3. 绘制文字 (你的代码已是 GDI+，保持即可)
	Gdiplus::RectF titleRect;
	Gdiplus::RectF subRect;
	if (isSelected) {
		const float titleHeight = g_listItemFontSelected1->GetHeight(&graphics);
		const float subHeight = g_listItemFontSelected2->GetHeight(&graphics) * 2.2f; // 限制为两行
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosSelectedX1),
									static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosSelectedY1),
									static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosSelectedX1),
									titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosSelectedX2),
								static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosSelectedY2),
								static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosSelectedX2), subHeight);
	} else {
		const float titleHeight = g_listItemFont1->GetHeight(&graphics);
		const float subHeight = g_listItemFont2->GetHeight(&graphics) * 2.2f; // 限制为两行
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosX1),
									static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosY1),
									static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosX1), titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosX2),
								static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosY2),
								static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosX2), subHeight);
	}
	const std::wstring& title = action->getTitle();
	const std::wstring& subtitle = action->getSubTitle();

	// 创建 StringFormat 对象来控制文本换行和省略号
	Gdiplus::StringFormat stringFormat;
	stringFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisWord); // 以单词为单位的省略号
	stringFormat.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit); // 限制行数

	// 确保你的文字画刷也是不透明的
	if (isSelected) {
		graphics.DrawString(title.c_str(), -1, g_listItemFontSelected1.get(), titleRect, nullptr,
							g_listItemTextColorBrushSelected1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFontSelected2.get(), subRect, &stringFormat,
							g_listItemTextColorBrushSelected2.get());
	} else {
		graphics.DrawString(title.c_str(), -1, g_listItemFont1.get(), titleRect, nullptr,
							g_listItemTextColorBrush1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFont2.get(), subRect, &stringFormat,
							g_listItemTextColorBrush2.get());
	}


	// 图标绘制

	if (action->getIconBitmap()) {
		// 假设 action->hIcon 是一个 32-bpp 的 HBITMAP
		// 目标绘制位置
		int iconX;
		int iconY;
		if (isSelected) {
			iconX = rc.left + g_itemIconSelectedX;
			iconY = rc.top + g_itemIconSelectedY;
		} else {
			iconX = rc.left + g_itemIconX;
			iconY = rc.top + g_itemIconY;
		}

		// 创建一个与源位图兼容的内存 DC
		HDC hMemDC = CreateCompatibleDC(hdc);
		HGDIOBJ oldBmp = SelectObject(hMemDC, action->getIconBitmap());

		// 设置 AlphaBlend 的混合参数
		BLENDFUNCTION blendFunc = {0};
		blendFunc.BlendOp = AC_SRC_OVER; // 指定混合操作为“源覆盖目标”
		blendFunc.BlendFlags = 0; // 必须为 0
		blendFunc.SourceConstantAlpha = 255; // 使用源位图自身的 Alpha 通道，255 表示不使用全局 Alpha
		blendFunc.AlphaFormat = AC_SRC_ALPHA; // 指明源位图带有 Alpha 通道

		// 使用 AlphaBlend 进行绘制
		AlphaBlend(
			hdc, // 目标 DC
			iconX, // 目标 X 坐标
			iconY, // 目标 Y 坐标
			LISTITEM_ICON_SIZE, // 目标宽度
			LISTITEM_ICON_SIZE, // 目标高度
			hMemDC, // 源 DC
			0, // 源 X 坐标
			0, // 源 Y 坐标
			LISTITEM_ICON_SIZE, // 源宽度
			LISTITEM_ICON_SIZE, // 源高度
			blendFunc // 混合函数参数
		);

		// 清理资源
		SelectObject(hMemDC, oldBmp);
		DeleteDC(hMemDC);
	}
	// else
	// {
	// 	if (isSelected)
	// 	{
	// 		ImageList_Draw(g_listFileImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconSelectedX,
	// 						rc.top + g_itemIconSelectedY,
	// 						ILD_TRANSPARENT);
	// 	}
	// 	else
	// 	{
	// 		ImageList_Draw(g_listFileImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconX,
	// 						rc.top + g_itemIconY,
	// 						ILD_TRANSPARENT);
	// 	}
	// }
	// }
	else {
		if (g_listFileImageList && action->getIconFilePathIndex() >= 0) {
			if (isSelected) {
				ImageList_Draw(g_listFileImageList, action->getIconFilePathIndex(), hdc, rc.left + g_itemIconSelectedX,
								rc.top + g_itemIconSelectedY,
								ILD_TRANSPARENT);
			} else {
				ImageList_Draw(g_listFileImageList, action->getIconFilePathIndex(), hdc, rc.left + g_itemIconX,
								rc.top + g_itemIconY,
								ILD_TRANSPARENT);
			}
			// ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
			// ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
		} else {
			if (isSelected) {
				ImageList_Draw(g_listFileImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconSelectedX,
								rc.top + g_itemIconSelectedY,
								ILD_TRANSPARENT);
			} else {
				ImageList_Draw(g_listFileImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconX,
								rc.top + g_itemIconY,
								ILD_TRANSPARENT);
			}
		}
	}
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

		Gdiplus::Graphics graphics(hdc);
		graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
		// 不知道为什么 列表的左边和上边会多空出一个像素
		if (g_skinJson != nullptr) {
			const Gdiplus::Rect rect(rcClient.left - 1, rcClient.top - 1, rcClient.right - rcClient.left + 1,
									rcClient.bottom - rcClient.top + 1);
			if (g_listViewBgImage) {
				graphics.DrawImage(g_listViewBgImage, rect);
			} else {
				const std::string colorString = g_skinJson.value("listview_bg_color", "");
				const Gdiplus::Color bgColor = HexToGdiplusColor(validateHexColor(colorString, "#FFFFFF"));
				const Gdiplus::SolidBrush bgBrush(bgColor);
				graphics.FillRectangle(&bgBrush, rect);
			}
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
	if (MyStartsWith2(editTextBuffer, L"{\"arg\":\"")) {
		if (const size_t end = find_json_end(editTextBuffer); end != std::wstring::npos) {
			const std::wstring json_part = editTextBuffer.substr(0, end + 1);
			bool isSuccess = false;
			try {
				nlohmann::json j = nlohmann::json::parse(wide_to_utf8(json_part));
				const std::string arg = j["arg"];
				currectActionArg = utf8_to_wide(arg);
				isSuccess = true;
			} catch (const nlohmann::json::parse_error& e) {
				std::wcerr << L"JSON 解析错误：" << utf8_to_wide(e.what()) << std::endl;
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
		if (const auto list = PluginManager::IsInterceptInputShowResultsDirectly(editTextBuffer2); !list.empty()) {
			filteredActions = list;
			addActionDone();
			return;
		}
		g_pluginManager->DispatchUserInput(editTextBuffer2);
	}
	actionFilter(editTextBuffer2);
}

// void ExecutePluginAction(const std::wstring& actionId)
// {
// 	if (g_pluginManager)
// 	{
// 		std::wstring actionIdStr = (actionId);
// 		g_pluginManager->DispatchActionExecute(actionIdStr);
// 	}
// }

inline void editTextInput() {
	editTextBuffer.resize(1000, L'\0');
	editTextBuffer.resize(GetWindowTextW(g_editHwnd, &editTextBuffer[0], 1000));
	if (editTextBuffer.empty()) {
		ListView_SetItemCountEx(g_listViewHwnd, 0, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		return;
	}
	SendMessage(g_listViewHwnd, WM_SETREDRAW, FALSE, 0);
	textMatching();
	SendMessage(g_listViewHwnd, WM_SETREDRAW, TRUE, 0);
}
