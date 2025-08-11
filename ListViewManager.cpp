#include "ListViewManager.h"
#include <shlobj.h>
#include <shlguid.h>
#include <sstream>
#include <Shobjidl.h>    // For IImageList
#include <commoncontrols.h>
#include <gdiplus.h>
#include <rapidfuzz/fuzz.hpp>

#include "DataKeeper.hpp"
#include <optional>
#include <execution>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <functional> // For std::greater
#include <thread>     // For std::thread::hardware_concurrency
#include <future>     // For std::async and std::future

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

ListViewManager::ListViewManager() = default;

typedef HRESULT (WINAPI *SHGetImageListPtr)(int iImageList, REFIID riid, void **ppv);

ThreadPool ListViewManager::pool(std::thread::hardware_concurrency());

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

	IImageList *pImageList = nullptr;
	if (const HRESULT hr = pSHGetImageList(size, __uuidof(IImageList), reinterpret_cast<void **>(&pImageList));
			FAILED(hr)) {
		FreeLibrary(hShell32);
		return nullptr;
	}

	// Release handle after use — or detach to HIMAGELIST
	HIMAGELIST hImageList = nullptr;
	pImageList->QueryInterface(__uuidof(IImageList), reinterpret_cast<void **>(&hImageList));

	pImageList->Release();
	FreeLibrary(hShell32);
	return hImageList;
}


/**
 * 系统图像列表的索引
 */
static int GetSysImageIndex(const std::wstring &filePath) {
	SHFILEINFOW sfi = {};
	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				   SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
	return sfi.iIcon;
}


static HIMAGELIST GetSystemImageList(bool largeIcon) {
	SHFILEINFOW sfi = {};
	//UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | (largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON);
	constexpr UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_OPENICON;

	// SHGetFileInfo 返回值是图像列表句柄，sfi.hIcon 是图标句柄，不要搞错
	return reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags));
}

void ListViewManager::Initialize(HWND parent, HINSTANCE hInstance, const int x, const int y, const int width,
								 const int height) {
	// 初始化 Common Controls
	INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
	InitCommonControlsEx(&icex);

	const DWORD style = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_OWNERDRAWFIXED |
						WS_VSCROLL |
						LVS_NOCOLUMNHEADER | LVS_OWNERDATA;
	hListView = CreateWindowExW(0, WC_LISTVIEW, L"", style,
								x, y, width, height, parent, reinterpret_cast<HMENU>(2), hInstance, nullptr);

	// 设置列 0（主标题），即使我们不显示它，也必须添加
	LVCOLUMN col = {LVCF_TEXT | LVCF_WIDTH};
	col.pszText = const_cast<LPWSTR>(L"Title");
	col.cx = 500;
	ListView_InsertColumn(hListView, 0, &col);

	//hImageList = GetSystemImageList(true);
	hImageList = GetSystemImageListEx(SHIL_EXTRALARGE); // 使用 48x48 图标

	ListView_SetImageList(hListView, hImageList, LVSIL_NORMAL);
	InitializeGraphicsResources(); // 确保字体和画刷已初始化
}

/**
 * 加载快捷方式项到列表里
 */
void ListViewManager::LoadActions(const std::vector<std::shared_ptr<RunCommandAction>> &actions) {
	// 保存引用，便于后续点击等操作
	allActions = actions;

	for (size_t i = 0; i < actions.size(); ++i) {
		const std::shared_ptr<RunCommandAction> &action = actions[i];
		if (action->IsUwpItem) {
			// 用这个方法来加载图标的话（把hicon添加到列表，再获取索引给action），在listview显示的时候顺序会乱，不知道为什么
			// if (action->iImageIndex == -1)
			// {
			// 	int imageIndex = ImageList_Add(hImageList, action->hIcon, NULL);
			// 	action->iImageIndex = imageIndex;
			// }
		} else {
			// 获取图标
			if (action->iImageIndex == -1)
				action->iImageIndex = GetSysImageIndex(action->GetTargetPath());
		}
	}
	unknown_file_icon_index = GetSysImageIndex((EXE_FOLDER_PATH + L"\\non_existent.ico"));

	filteredActions = actions;
}

void ListViewManager::Exactmatch(const std::wstring &keyword) {
	int matchedCount = 0;

	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) words.push_back(word);

	for (const auto &action: allActions) {
		const std::wstring &runCommand = action->RunCommand;

		bool allMatch = true;
		for (const auto &w: words) {
			if (runCommand.find(w) == std::wstring::npos) {
				allMatch = false;
				break;
			}
		}

		if (allMatch) {
			filteredActions.push_back(action); // <-- 保存筛选后的 action 顺序
			if (pref_max_search_results > 0) {
				matchedCount++;
				if (matchedCount >= pref_max_search_results)
					break;
			}
		}
	}
}

/**
 * 定义一个结构体来存储 action 及其匹配分数
 */
struct ScoredAction {
	std::shared_ptr<RunCommandAction> action;
	double score;

	/**
	 * 用于排序的比较运算符
	 */
	bool operator>(const ScoredAction &other) const {
		return score > other.score;
	}
};

/**
 * 正常匹配，通过空格分隔每个关键字，通过 contain 来筛选项
 * 如果项达到上百万条，还可以进一步使用多线程方案
 * @param keyword 搜索词 
 */
void ListViewManager::Exactmatch_Optimized(const std::wstring &keyword) {
	// 分隔关键字
	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) {
		if (!word.empty())
			words.push_back(word);
	}

	if (words.empty()) {
		return;
	}

	for (const auto &action: allActions) {
		const std::wstring &runCommand = action->RunCommand;

		bool isMatch = true;
		for (const auto &w: words) {
			if (runCommand.find(w) == std::wstring::npos) {
				isMatch = false;
				break;
			}
		}

		if (isMatch) {
			// 只将匹配项添加到 vector 中
			filteredActions.push_back(action);
			if (pref_max_search_results > 0 && filteredActions.size() >= pref_max_search_results) {
				break; // 达到最大结果数，停止筛选
			}
		}
	}
}


/**
 * 线程分配逻辑完全可控，可以做更精细的分块优化
 * @param keyword 搜索词
 */
void ListViewManager::Fuzzymatch_MultiThreaded(const std::wstring &keyword) {
	const std::wstring lowerKeyword = MyToLower(keyword);

	// 并行计算分数
	// 确定要使用的线程数，通常基于硬件核心数
	// hardware_concurrency() 可能返回0，所以至少保证1个线程
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0) {
		numThreads = 1;
	}

	std::vector<std::future<std::vector<ScoredAction>>> futures;
	const size_t totalSize = allActions.size();
	const size_t chunkSize = (totalSize + numThreads - 1) / numThreads;

	// 创建并分发任务给多个线程
	for (unsigned int i = 0; i < numThreads; ++i) {
		// 1. 先计算索引，而不是直接操作迭代器
		const size_t start_index = i * chunkSize;

		// 2. 检查起始索引是否已超出范围，如果超出则后续已无数据可分，直接退出循环
		if (start_index >= totalSize) {
			break;
		}
		// 3. 计算结束索引，并确保它不会超过容器的实际大小
		const size_t end_index = std::min(start_index + chunkSize, totalSize);

		// 4. 使用安全的索引来创建迭代器
		const auto start_iterator = allActions.begin() + start_index;
		const auto end_iterator = allActions.begin() + end_index;

		// std::async 启动一个异步任务
		// 注意 lambda 的捕获列表，使用 start_iterator 和 end_iterator
		futures.push_back(pool.enqueue([&, start_iterator, end_iterator]()
											   // futures.push_back(std::async(std::launch::async, [start_iterator, end_iterator, &lowerKeyword, this]() 
									   {
										   std::vector<ScoredAction> localScoredActions;

										   // 每个线程在自己的数据块上进行计算
										   for (auto it = start_iterator; it != end_iterator; ++it) {
											   const auto &action = *it;
											   if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword,
																								action->RunCommand);
													   score >=
													   pref_fuzzy_match_score_threshold) {
												   localScoredActions.push_back({action, score});
											   }
										   }
										   return localScoredActions; // 返回该线程的处理结果
									   }));
	}

	// 合并所有线程的结果 (在主线程中完成)
	std::vector<ScoredAction> scoredActions;
	try {
		for (auto &f: futures) {
			// f.get() 会等待线程完成，并返回其结果
			std::vector<ScoredAction> localResult = f.get();
			// 将子线程的结果移动到主列表中，提高效率
			scoredActions.insert(scoredActions.end(),
								 std::make_move_iterator(localResult.begin()),
								 std::make_move_iterator(localResult.end()));
		}
	}
	catch (const std::exception &e) {
		// 记录错误或适当处理
		std::cerr << "An exception occurred during parallel fuzzy matching: " << e.what() << std::endl;
		// Clear any partial results and return
		return;
	}

	// 排序：按分数从高到低排序 (在主线程中完成)
	if (pref_max_search_results > 0 && scoredActions.size() > pref_max_search_results) {
		// 只对前 pref_max_search_results 个元素进行部分排序
		std::partial_sort(scoredActions.begin(),
						  scoredActions.begin() + pref_max_search_results,
						  scoredActions.end(),
						  std::greater<>()); // 依然是降序

		// 调整大小，丢弃后面不需要的元素
		scoredActions.resize(pref_max_search_results);
	} else {
		// 如果匹配项不多，或者不需要限制数量，则进行全排序
		std::sort(scoredActions.begin(), scoredActions.end(), std::greater<>());
	}
	std::transform(scoredActions.begin(), scoredActions.end(),
				   std::back_inserter(filteredActions),
				   [](const ScoredAction &s) { return s.action; });
}

/**
 * 模糊匹配方法2，和方法1耗时上没区别
 * @param keyword 搜索词
 */
void ListViewManager::Fuzzymatch_MultiThreaded2(const std::wstring &keyword) {
	const std::wstring lowerKeyword = MyToLower(keyword);

	// 拷贝快照，保证并行只读
	std::vector<std::shared_ptr<RunCommandAction>> snapshot = allActions;

	// 1) 并行打分到等长缓冲区
	std::vector<std::optional<ScoredAction>> tmp(snapshot.size());
	std::vector<size_t> idx(snapshot.size());
	std::iota(idx.begin(), idx.end(), 0);

	std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t i) {
		const auto &action = snapshot[i];
		const std::wstring &searchableText = action->RunCommand;

		if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword, searchableText); score >=
																						pref_fuzzy_match_score_threshold) {
			tmp[i] = ScoredAction{action, score};
		}
	});

	// 2) 压缩有效项
	std::vector<ScoredAction> scored;
	for (auto &o: tmp)
		if (o) scored.push_back(std::move(*o));

	// Top-K 用 nth_element 避免了不必要的全量排序
	// 3) Top-K + 排序（如果需要）
	size_t limit = (pref_max_search_results > 0)
				   ? static_cast<size_t>(pref_max_search_results)
				   : scored.size();

	if (scored.size() > limit) {
		auto nth = scored.begin() + limit;
		std::nth_element(scored.begin(), nth, scored.end(),
						 [](const ScoredAction &a, const ScoredAction &b) {
							 return a.score > b.score;
						 });
		scored.erase(nth, scored.end());
	}
	std::sort(scored.begin(), scored.end(), std::greater<>());

	std::transform(scored.begin(), scored.end(),
				   std::back_inserter(filteredActions),
				   [](const ScoredAction &s) { return s.action; });
}


void ListViewManager::Fuzzymatch(const std::wstring &keyword) {

	// 1. 准备数据：将关键字转为小写
	const std::wstring lowerKeyword = MyToLower(keyword);

	std::vector<ScoredAction> scoredActions;

	// 2. 计算分数：为所有 action 计算匹配度分数
	for (const std::shared_ptr<RunCommandAction> &action: allActions) {
		// 使用 rapidfuzz::fuzz::WRatio 计算加权比率分数。
		// WRatio 对于不同长度的字符串和乱序的单词有很好的效果，是通用的优选。
		if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword, action->RunCommand); score >=
																							pref_fuzzy_match_score_threshold) {
			scoredActions.push_back({action, score});
		}
	}

	// 3. 排序：按分数从高到低排序
	std::sort(scoredActions.begin(), scoredActions.end(), std::greater<>());

	int matchedCount = 0;
	for (const auto &[action, score]: scoredActions) {
		filteredActions.push_back(action);
		if (pref_max_search_results > 0) {
			matchedCount++;
			if (matchedCount >= pref_max_search_results)
				break;
		}
	}
}

// 编辑框筛选列表内容
void ListViewManager::Filter(const std::wstring &keyword) {
	filteredActions.clear();

	if (keyword.empty()) {
		ListView_SetItemCountEx(hListView, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		return;
	}

	if (pref_fuzzy_match) {
		MethodTimerStart();
#if defined(DEBUG) || _DEBUG
		Fuzzymatch(keyword);
#else
		// Fuzzymatch_MultiThreaded(keyword);
		Fuzzymatch_MultiThreaded2(keyword);
#endif
		MethodTimerEnd(L"fuzzymatch");
	} else {
		MethodTimerStart();
		// Exactmatch(keyword);
		Exactmatch_Optimized(keyword);
		MethodTimerEnd(L"Exactmatch");
	}
	// 关键方法，用于提交列表数据更新
	ListView_SetItemCountEx(hListView, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	if (!filteredActions.empty()) {
		// 清除所有项的选中状态
		//ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		// 设置第一项为选中状态
		ListView_SetItemState(hListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		// 确保第一项可见
		ListView_EnsureVisible(hListView, 0, FALSE);
	}
	InvalidateRect(hListView, NULL, TRUE);
}


void ListViewManager::MeasureItem(MEASUREITEMSTRUCT *lpMeasureItem) {
	lpMeasureItem->itemHeight = 60;
}

HBRUSH hNormalBrush = CreateSolidBrush(COLOR_UI_BG);
HBRUSH hSelectedBrush = CreateSolidBrush(COLOR_UI_BG_DEEP);

// std::unique_ptr<Gdiplus::SolidBrush> whiteBrush;
// std::unique_ptr<Gdiplus::SolidBrush> grayBrush;
// std::unique_ptr<Gdiplus::SolidBrush> blackBrush;
bool fontsInitialized = false;

void ListViewManager::OnGetDispInfo(NMLVDISPINFOW *pdi) {
	const int index = pdi->item.iItem;

	// 安全检查，确保索引在 filteredActions 的范围内
	if (index < 0 || static_cast<size_t>(index) >= filteredActions.size()) {
		return; // 无效索引，不处理
	}

	// 获取对应的数据项
	const auto &action = filteredActions[index];

	// 根据 ListView 请求的信息 (mask) 来提供数据
	// 注意：因为我们是 OwnerDraw，ListView 不会请求 LVIF_TEXT，但为了完整性，最好写上
	if (pdi->item.mask & LVIF_TEXT) {
		// 如果你需要显示文本（比如在非OwnerDraw的列中），就这样做：
		// wcsncpy_s(pdi->item.pszText, pdi->item.cchTextMax, action->GetTitle().c_str(), _TRUNCATE);
	}

	// ListView 会请求图像索引
	if (pdi->item.mask & LVIF_IMAGE) {
		pdi->item.iImage = (action->iImageIndex == -1) ? unknown_file_icon_index : action->iImageIndex;
	}

	// 如果有其他状态（如选中），也可以在这里设置
	// if (pdi->item.mask & LVIF_STATE) { ... }
}

void ListViewManager::InitializeGraphicsResources() {
	fontsInitialized = true;
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

static void CleanupGraphicsResources() {
	// 如果不需要，可以直接让 unique_ptr 自动销毁
	// whiteBrush.reset();
	// grayBrush.reset();
	// blackBrush.reset();
	fontsInitialized = false;
}


// 通过一个 thread_local 存储 Gdiplus::Graphics 缓存，避免每次都重新创建
static Gdiplus::Graphics *GetCachedGraphics(HDC hdc) {
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
void ListViewManager::Cleanup() {
	// 释放 ListView 的图像列表
	if (hImageList) {
		ImageList_Destroy(hImageList);
		hImageList = nullptr;
	}

	// 清理 GDI+ 资源
	CleanupGraphicsResources();
}


void ListViewManager::setUnknownFileIcon(HDC hdc, const RECT rc) {
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
		FillRect(hdc, &iconRect, (HBRUSH) (COLOR_GRAYTEXT + 1));
	}
}

void ListViewManager::DrawItem(const DRAWITEMSTRUCT *lpDrawItem) const {
	const UINT index = lpDrawItem->itemID;
	if (index < 0 || index >= static_cast<int>(filteredActions.size()))
		return;

	std::shared_ptr<RunCommandAction> action = filteredActions[index];
	HDC hdc = lpDrawItem->hDC;
	const RECT &rc = lpDrawItem->rcItem;

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
	const Gdiplus::Rect rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
	UINT isSelected = lpDrawItem->itemState & ODS_SELECTED;
	if (isSelected) {
		graphics.FillRectangle(g_listItemBgColorBrushSelected.get(), rect);
	} else {
		graphics.FillRectangle(g_listItemBgColorBrush.get(), rect);
	}

	// 3. 绘制文字 (你的代码已是 GDI+，保持即可)
	Gdiplus::RectF titleRect;
	Gdiplus::RectF subRect;
	if (isSelected) {
		const float titleHeight = g_listItemFontSelected1->GetHeight(&graphics);
		const float subHeight = g_listItemFontSelected2->GetHeight(&graphics) + 30;
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosSelectedX1),
								   static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosSelectedY1),
								   static_cast<Gdiplus::REAL>(rc.right - rc.left - g_itemTextPosSelectedX1),
								   titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosSelectedX2),
								 static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosSelectedY2),
								 static_cast<Gdiplus::REAL>(rc.right - rc.left - g_itemTextPosSelectedX2), subHeight);
	} else {
		const float titleHeight = g_listItemFont1->GetHeight(&graphics);
		const float subHeight = g_listItemFont2->GetHeight(&graphics) + 30;
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosX1),
								   static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosY1),
								   static_cast<Gdiplus::REAL>(rc.right - rc.left - g_itemTextPosX1), titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosX2),
								 static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosY2),
								 static_cast<Gdiplus::REAL>(rc.right - rc.left - g_itemTextPosX2), subHeight);
	}
	const std::wstring &title = action->GetTitle();
	const std::wstring &subtitle = action->GetSubtitle();

	// 确保你的文字画刷也是不透明的
	if (isSelected) {
		graphics.DrawString(title.c_str(), -1, g_listItemFontSelected1.get(), titleRect, nullptr,
							g_listItemTextColorBrushSelected1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFontSelected2.get(), subRect, nullptr,
							g_listItemTextColorBrushSelected2.get());
	} else {
		graphics.DrawString(title.c_str(), -1, g_listItemFont1.get(), titleRect, nullptr,
							g_listItemTextColorBrush1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFont2.get(), subRect, nullptr,
							g_listItemTextColorBrush2.get());
	}


	// 图标绘制
	if (action->IsUwpItem) {
		if (action->hIcon) {
			HDC hMemDC = CreateCompatibleDC(hdc);
			HGDIOBJ oldBmp = SelectObject(hMemDC, action->hIcon);

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
			BitBlt(hdc, iconX, iconY, LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE, hMemDC, 0, 0, SRCCOPY);
			SelectObject(hMemDC, oldBmp);
			DeleteDC(hMemDC);
		} else {
			if (isSelected) {
				ImageList_Draw(hImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconSelectedX,
							   rc.top + g_itemIconSelectedY,
							   ILD_TRANSPARENT);
			} else {
				ImageList_Draw(hImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconX, rc.top + g_itemIconY,
							   ILD_TRANSPARENT);
			}
		}
	} else {
		if (hImageList && action->iImageIndex >= 0) {
			if (isSelected) {
				ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + g_itemIconSelectedX,
							   rc.top + g_itemIconSelectedY,
							   ILD_TRANSPARENT);
			} else {
				ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + g_itemIconX, rc.top + g_itemIconY,
							   ILD_TRANSPARENT);
			}
			// ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
			// ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
		} else {
			if (isSelected) {
				ImageList_Draw(hImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconSelectedX,
							   rc.top + g_itemIconSelectedY,
							   ILD_TRANSPARENT);
			} else {
				ImageList_Draw(hImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconX, rc.top + g_itemIconY,
							   ILD_TRANSPARENT);
			}
		}
	}
}

// ListViewManager.cpp

LRESULT ListViewManager::OnCustomDraw(LPNMLVCUSTOMDRAW lplvcd) {
	// We are only interested in the stage before the whole control is painted.
	if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
		HDC hdc = lplvcd->nmcd.hdc;
		HWND hWnd = lplvcd->nmcd.hdr.hwndFrom;
		RECT rcClient;
		GetClientRect(hWnd, &rcClient);
		// 这个判断很重要，用来判断这次是要重绘listview背景，还是重绘item，用来解决item和listview绘制冲突
		// 在VS2022编译里管用，clion 的 mingw里管用，clion的Vs构建不需要这个判断
#if defined(USE_VS_TOOLCHAIN)
		if (lplvcd->dwItemType != 2)
		{
			return CDRF_NOTIFYITEMDRAW;
		}

#elif defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
		if (lplvcd->dwItemType != 2) {
			return CDRF_NOTIFYITEMDRAW;
		}
#endif

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
