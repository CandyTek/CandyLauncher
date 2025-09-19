#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <memory>
// #include "ListedRunnerPlugin.h"
#include "../util/ThreadPool.h"
#include "../util/MainTools.hpp"
#include <shlobj.h>
#include <shlguid.h>
#include <sstream>
#include <Shobjidl.h>    // For IImageList
#include <commoncontrols.h>
#include <gdiplus.h>
#include <rapidfuzz/fuzz.hpp>

#include "../common/DataKeeper.hpp"
#include <optional>
#include <execution>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <functional> // For std::greater
#include <thread>     // For std::thread::hardware_concurrency
#include <future>     // For std::async and std::future

#include "plugins/ActionNormal.hpp"
#include "plugins/PluginManager.hpp"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

// Static variable definitions
inline HIMAGELIST g_listFileImageList = nullptr;
// 用于显示的列表
inline std::vector<std::shared_ptr<ActionBase>> allActions;
inline std::vector<std::shared_ptr<ActionBase>> baseAppLaunchActions;
inline std::vector<std::shared_ptr<ActionBase>> filteredActions;

// 模糊搜索的异步线程
inline ThreadPool poolFuzzyMatch(std::thread::hardware_concurrency());
// 编辑框文本缓存
inline std::wstring editTextBuffer;
// inline HBRUSH hNormalBrush = CreateSolidBrush(COLOR_UI_BG);
// inline HBRUSH hSelectedBrush = CreateSolidBrush(COLOR_UI_BG_DEEP);

// std::unique_ptr<Gdiplus::SolidBrush> whiteBrush;
// std::unique_ptr<Gdiplus::SolidBrush> grayBrush;
// std::unique_ptr<Gdiplus::SolidBrush> blackBrush;
inline bool listViewFontsInitialized = false;

typedef HRESULT (WINAPI *SHGetImageListPtr)(int iImageList, REFIID riid, void** ppv);

static void listViewInitializeGraphicsResources()
{
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


	if (g_skinJson != nullptr)
	{
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
	}
	else
	{
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

static void refreshAppLaunchAction()
{
	baseAppLaunchActions.clear();
	baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"刷新列表", L"刷新索引列表", IDI_REFRESH,
																appLaunchActionCallBacks["refreshList"]));
	baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"刷新插件", L"重新加载所有插件动作", IDI_REFRESH,
																appLaunchActionCallBacks["refreshPlugins"]));

	if (g_settings_map["pref_indexed_myapp_extra_launch"].boolValue)
	{
		baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"退出软件", L"退出本软件", IDI_CLOSE,
																	appLaunchActionCallBacks["quit"]));
		baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"重启软件", L"重启本软件", IDI_REFRESH,
																	appLaunchActionCallBacks["restart"]));
		baseAppLaunchActions.push_back(
			std::make_shared<ActionNormal>(L"查看用户软件配置 JSON 文件", L"打开user_settings.json文件", IDI_CLOSE,
											appLaunchActionCallBacks["editUserConfig"]));
		baseAppLaunchActions.push_back(
			std::make_shared<ActionNormal>(L"查看所有软件配置 JSON 文件", L"打开settings.json文件", IDI_CLOSE,
											appLaunchActionCallBacks["viewAllSettings"]));
		// baseAppLaunchActions.push_back(
		// 	std::make_shared<ActionNormal>(L"查看用户索引配置 JSON 文件", L"打开runner.json文件", IDI_CLOSE,
		// 									appLaunchActionCallBacks["viewIndexConfig"]));
		baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"软件帮助文档", L"前往浏览器查看帮助文档", IDI_CLOSE,
																	appLaunchActionCallBacks["openHelp"]));
		baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"检查更新", L"前往浏览器检查更新", IDI_CLOSE,
																	appLaunchActionCallBacks["checkUpdate"]));
		baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"反馈建议", L"前往浏览器反馈建议", IDI_CLOSE,
																	appLaunchActionCallBacks["feedback"]));
		baseAppLaunchActions.push_back(std::make_shared<ActionNormal>(L"打开Github项目主页", L"前往浏览器浏览项目主页", IDI_CLOSE,
																	appLaunchActionCallBacks["openGithub"]));
	}
}


static void listViewCleanupGraphicsResources()
{
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
											UINT_PTR, const DWORD_PTR dwRefData)
{
	switch (message)
	{
	case WM_KEYDOWN:
		break;
	case WM_MBUTTONUP:
		TimerIDSetFocusEdit = SetTimer(GetParent(hWnd), TIMER_SETFOCUS_EDIT, 10, nullptr); // 10 毫秒延迟
		break;
	case WM_LISTVIEW_REFRESH_RESOURCE:
		listViewCleanupGraphicsResources();
		listViewInitializeGraphicsResources(); // 确保字体和画刷已初始化
		break;
	case WM_MOUSEWHEEL:
		{
			// 在Shift+滚动上键时，会触发列表的NM_DBLCLK,不知怎么回事，干脆直接屏蔽这个组合
			const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			if (shiftDown)
			{
				return 0;
			}
		}
		break;
	default:
		break;
	}
	return DefSubclassProc(hWnd, message, wParam, lParam);
}

static HIMAGELIST GetSystemImageListEx(const int size)
{
	SHFILEINFO sfi = {};
	SHGetFileInfo(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
				SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);

	HMODULE hShell32 = LoadLibraryW(L"Shell32.dll");
	if (!hShell32) return nullptr;

	SHGetImageListPtr pSHGetImageList = reinterpret_cast<SHGetImageListPtr>(GetProcAddress(hShell32, "SHGetImageList"));
	if (!pSHGetImageList)
	{
		FreeLibrary(hShell32);
		return nullptr;
	}

	IImageList* pImageList = nullptr;
	if (const HRESULT hr = pSHGetImageList(size, __uuidof(IImageList), reinterpret_cast<void**>(&pImageList));
		FAILED(hr))
	{
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
static HIMAGELIST GetSystemImageList(bool largeIcon)
{
	SHFILEINFOW sfi = {};
	//UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | (largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON);
	constexpr UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_OPENICON;

	// SHGetFileInfo 返回值是图像列表句柄，sfi.hIcon 是图标句柄，不要搞错
	return reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags));
}

static void listViewInitialize(HWND parent, HINSTANCE hInstance, const int x, const int y, const int width,
								const int height)
{
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

static void Exactmatch(const std::wstring& keyword)
{
	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) words.push_back(word);
	int matchedCount = 0;

	auto addAction = [&](std::vector<std::shared_ptr<ActionBase>>& actions)
	{
		for (const auto& action : actions)
		{
			const std::wstring& runCommand = action->matchText;

			bool allMatch = true;
			for (const auto& w : words)
			{
				if (runCommand.find(w) == std::wstring::npos)
				{
					allMatch = false;
					break;
				}
			}

			if (allMatch)
			{
				filteredActions.push_back(action); // <-- 保存筛选后的 action 顺序
				if (pref_max_search_results > 0)
				{
					matchedCount++;
					if (matchedCount >= pref_max_search_results)
						break;
				}
			}
		}
	};
	addAction(allActions);
}

/**
 * 定义一个结构体来存储 action 及其匹配分数
 */
struct ScoredAction
{
	std::shared_ptr<ActionBase> action;
	double score;

	/**
	 * 用于排序的比较运算符
	 */
	bool operator>(const ScoredAction& other) const
	{
		return score > other.score;
	}
};

/**
 * 正常匹配，通过空格分隔每个关键字，通过 contain 来筛选项
 * 如果项达到上百万条，还可以进一步使用多线程方案
 * @param keyword 搜索词 
 */
static void Exactmatch_Optimized(const std::wstring& keyword)
{
	// 分隔关键字
	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word)
	{
		if (!word.empty())
			words.push_back(word);
	}

	if (words.empty())
	{
		return;
	}

	auto addAction = [&](std::vector<std::shared_ptr<ActionBase>>& actions)
	{
		for (const auto& action : actions)
		{
			const std::wstring& runCommand = action->matchText;

			bool isMatch = true;
			for (const auto& w : words)
			{
				if (runCommand.find(w) == std::wstring::npos)
				{
					isMatch = false;
					break;
				}
			}

			if (isMatch)
			{
				// 只将匹配项添加到 vector 中
				filteredActions.push_back(action);
				if (pref_max_search_results > 0 && filteredActions.size() >= static_cast<size_t>(
					pref_max_search_results))
				{
					break; // 达到最大结果数，停止筛选
				}
			}
		}
	};

	addAction(allActions);
}

/**
 * 线程分配逻辑完全可控，可以做更精细的分块优化
 * @param keyword 搜索词
 */
static void Fuzzymatch_MultiThreaded(const std::wstring& keyword)
{
	const std::wstring lowerKeyword = MyToLower(keyword);

	// 并行计算分数
	// 确定要使用的线程数，通常基于硬件核心数
	// hardware_concurrency() 可能返回0，所以至少保证1个线程
	unsigned int numThreads = std::thread::hardware_concurrency();
	if (numThreads == 0)
	{
		numThreads = 1;
	}

	std::vector<std::future<std::vector<ScoredAction>>> futures;
	const size_t totalSize = allActions.size();
	const size_t chunkSize = (totalSize + numThreads - 1) / numThreads;

	// 创建并分发任务给多个线程
	for (unsigned int i = 0; i < numThreads; ++i)
	{
		// 1. 先计算索引，而不是直接操作迭代器
		const size_t start_index = i * chunkSize;

		// 2. 检查起始索引是否已超出范围，如果超出则后续已无数据可分，直接退出循环
		if (start_index >= totalSize)
		{
			break;
		}
		// 3. 计算结束索引，并确保它不会超过容器的实际大小
		const size_t end_index = std::min(start_index + chunkSize, totalSize);

		// 4. 使用安全的索引来创建迭代器
		const auto start_iterator = allActions.begin() + start_index;
		const auto end_iterator = allActions.begin() + end_index;

		// std::async 启动一个异步任务
		// 注意 lambda 的捕获列表，使用 start_iterator 和 end_iterator
		futures.push_back(poolFuzzyMatch.enqueue([&, start_iterator, end_iterator]()
			// futures.push_back(std::async(std::launch::async, [start_iterator, end_iterator, &lowerKeyword, this]() 
			{
				std::vector<ScoredAction> localScoredActions;

				// 每个线程在自己的数据块上进行计算
				for (auto it = start_iterator; it != end_iterator; ++it)
				{
					const auto& action = *it;
					if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword,
																	action->matchText);
						score >=
						pref_fuzzy_match_score_threshold)
					{
						localScoredActions.push_back({action, score});
					}
				}
				return localScoredActions; // 返回该线程的处理结果
			}));
	}

	// 合并所有线程的结果 (在主线程中完成)
	std::vector<ScoredAction> scoredActions;
	try
	{
		for (auto& f : futures)
		{
			// f.get() 会等待线程完成，并返回其结果
			std::vector<ScoredAction> localResult = f.get();
			// 将子线程的结果移动到主列表中，提高效率
			scoredActions.insert(scoredActions.end(),
								std::make_move_iterator(localResult.begin()),
								std::make_move_iterator(localResult.end()));
		}
	}
	catch (const std::exception& e)
	{
		// 记录错误或适当处理
		std::cerr << "An exception occurred during parallel fuzzy matching: " << e.what() << std::endl;
		// Clear any partial results and return
		return;
	}

	// 排序：按分数从高到低排序 (在主线程中完成)
	if (pref_max_search_results > 0 && scoredActions.size() > static_cast<size_t>(pref_max_search_results))
	{
		// 只对前 pref_max_search_results 个元素进行部分排序
		std::partial_sort(scoredActions.begin(),
						scoredActions.begin() + static_cast<std::ptrdiff_t>(pref_max_search_results),
						scoredActions.end(),
						std::greater<>()); // 依然是降序

		// 调整大小，丢弃后面不需要的元素
		scoredActions.resize(static_cast<size_t>(pref_max_search_results));
	}
	else
	{
		// 如果匹配项不多，或者不需要限制数量，则进行全排序
		std::sort(scoredActions.begin(), scoredActions.end(), std::greater<>());
	}
	std::transform(scoredActions.begin(), scoredActions.end(),
					std::back_inserter(filteredActions),
					[](const ScoredAction& s) { return s.action; });
}

/**
 * 模糊匹配方法2，和方法1耗时上没区别
 * @param keyword 搜索词
 */
static void Fuzzymatch_MultiThreaded2(const std::wstring& keyword)
{
	const std::wstring lowerKeyword = MyToLower(keyword);

	// 拷贝快照，保证并行只读
	std::vector<std::shared_ptr<ActionBase>> snapshot = allActions;

	// 1) 并行打分到等长缓冲区
	std::vector<std::optional<ScoredAction>> tmp(snapshot.size());
	std::vector<size_t> idx(snapshot.size());
	std::iota(idx.begin(), idx.end(), 0);

	std::for_each(std::execution::par, idx.begin(), idx.end(), [&](size_t i)
	{
		const auto& action = snapshot[i];
		const std::wstring& searchableText = action->matchText;

		if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword, searchableText); score >=
			pref_fuzzy_match_score_threshold)
		{
			tmp[i] = ScoredAction{action, score};
		}
	});

	// 2) 压缩有效项
	std::vector<ScoredAction> scored;
	for (auto& o : tmp)
		if (o) scored.push_back(std::move(*o));

	// Top-K 用 nth_element 避免了不必要的全量排序
	// 3) Top-K + 排序（如果需要）
	size_t limit = (pref_max_search_results > 0)
						? static_cast<size_t>(pref_max_search_results)
						: scored.size();

	if (scored.size() > limit)
	{
		auto nth = scored.begin() + limit;
		std::nth_element(scored.begin(), nth, scored.end(),
						[](const ScoredAction& a, const ScoredAction& b)
						{
							return a.score > b.score;
						});
		scored.erase(nth, scored.end());
	}
	std::sort(scored.begin(), scored.end(), std::greater<>());

	std::transform(scored.begin(), scored.end(),
					std::back_inserter(filteredActions),
					[](const ScoredAction& s) { return s.action; });
}

static void Fuzzymatch(const std::wstring& keyword)
{
	// 1. 准备数据：将关键字转为小写
	const std::wstring lowerKeyword = MyToLower(keyword);

	std::vector<ScoredAction> scoredActions;

	// 2. 计算分数：为所有 action 计算匹配度分数
	for (const std::shared_ptr<ActionBase>& action : allActions)
	{
		// 使用 rapidfuzz::fuzz::WRatio 计算加权比率分数。
		// WRatio 对于不同长度的字符串和乱序的单词有很好的效果，是通用的优选。
		if (const double score = rapidfuzz::fuzz::WRatio(lowerKeyword, action->matchText); score >=
			pref_fuzzy_match_score_threshold)
		{
			scoredActions.push_back({action, score});
		}
	}

	// 3. 排序：按分数从高到低排序
	std::sort(scoredActions.begin(), scoredActions.end(), std::greater<>());

	int matchedCount = 0;
	for (const auto& [action, score] : scoredActions)
	{
		filteredActions.push_back(action);
		if (pref_max_search_results > 0)
		{
			matchedCount++;
			if (matchedCount >= pref_max_search_results)
				break;
		}
	}
}

static void addActionDone()
{
	ListView_SetItemCountEx(g_listViewHwnd, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	if (!filteredActions.empty())
	{
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
inline void actionFilter(const std::wstring& keyword)
{
	filteredActions.clear();
	allActions.clear();
	allActions = baseAppLaunchActions;
	PluginManager::GetAllPluginActions(allActions);

	ConsolePrintln(L"项目数量"+std::to_wstring(allActions.size()));
	if (pref_fuzzy_match)
	{
		MethodTimerStart();
#if defined(DEBUG) || _DEBUG
		Fuzzymatch(keyword);
#else
		// Fuzzymatch_MultiThreaded(keyword);
		Fuzzymatch_MultiThreaded2(keyword);
#endif
		MethodTimerEnd(L"fuzzymatch");
	}
	else
	{
		MethodTimerStart();
		// Exactmatch(keyword);
		Exactmatch_Optimized(keyword);
		MethodTimerEnd(L"Exactmatch");
	}
	// 关键方法，用于提交列表数据更新
	addActionDone();
}

static void listViewMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem)
{
	lpMeasureItem->itemHeight = g_listItemHeight;
}

// listview自定义数据时，必须提供的数据分配方法
static void listViewCustomDataOnGetDispInfo(NMLVDISPINFOW* pdi)
{
	const int index = pdi->item.iItem;

	// 安全检查，确保索引在 filteredActions 的范围内
	if (index < 0 || static_cast<size_t>(index) >= filteredActions.size())
	{
		return; // 无效索引，不处理
	}

	// 获取对应的数据项
	const auto& action = filteredActions[index];

	// 根据 ListView 请求的信息 (mask) 来提供数据
	// 注意：因为我们是 OwnerDraw，ListView 不会请求 LVIF_TEXT，但为了完整性，最好写上
	if (pdi->item.mask & LVIF_TEXT)
	{
		// 如果你需要显示文本（比如在非OwnerDraw的列中），就这样做：
		// wcsncpy_s(pdi->item.pszText, pdi->item.cchTextMax, action->GetTitle().c_str(), _TRUNCATE);
	}

	// ListView 会请求图像索引
	if (pdi->item.mask & LVIF_IMAGE)
	{
		pdi->item.iImage = (action->getIconFilePathIndex() == -1) ? unknown_file_icon_index : action->getIconFilePathIndex();
	}

	// 如果有其他状态（如选中），也可以在这里设置
	// if (pdi->item.mask & LVIF_STATE) { ... }
}

// 通过一个 thread_local 存储 Gdiplus::Graphics 缓存，避免每次都重新创建
static Gdiplus::Graphics* GetCachedGraphics(HDC hdc)
{
	thread_local std::unique_ptr<Gdiplus::Graphics> cachedGraphics;
	static HDC lastDC = nullptr;

	if (!cachedGraphics || lastDC != hdc)
	{
		cachedGraphics = std::make_unique<Gdiplus::Graphics>(hdc);
		lastDC = hdc;
		cachedGraphics->SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
	}

	return cachedGraphics.get();
}

// 在合适的地方释放资源，如在窗口销毁时调用
static void listViewCleanup()
{
	// 释放 ListView 的图像列表
	if (g_listFileImageList)
	{
		ImageList_Destroy(g_listFileImageList);
		g_listFileImageList = nullptr;
	}

	// 清理 GDI+ 资源
	listViewCleanupGraphicsResources();
}

static void setUnknownFileIcon(HDC hdc, const RECT rc)
{
	static HICON hIcon = nullptr;

	// 获取系统的未知文件图标
	if (hIcon == nullptr)
	{
		SHFILEINFOW shfi = {0};
		const UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON;
		if (SHGetFileInfoW(L"non_existent.ico", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), flags))
		{
			hIcon = shfi.hIcon;
		}
	}

	if (hIcon)
	{
		DrawIconEx(hdc, rc.left + 4, rc.top + 4, hIcon, LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE, 0, nullptr, DI_NORMAL);
	}
	else
	{
		// 获取失败，绘制一个灰色占位图标
		RECT iconRect = {rc.left + 4, rc.top + 4, rc.left + 20, rc.top + 20};
		FillRect(hdc, &iconRect, (HBRUSH)(COLOR_GRAYTEXT + 1));
	}
}

static void listViewDrawItem(const DRAWITEMSTRUCT* lpDrawItem)
{
	const UINT index = lpDrawItem->itemID;
	if (index < 0 || index >= static_cast<int>(filteredActions.size()))
		return;

	std::shared_ptr<ActionBase> action = filteredActions[index];
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
	if (isSelected)
	{
		if (g_listItemBgImageSelected)
		{
			graphics.DrawImage(g_listItemBgImageSelected, rect);
		}
		else
		{
			graphics.FillRectangle(g_listItemBgColorBrushSelected.get(), rect);
		}
	}
	else
	{
		if (g_listItemBgImage)
		{
			graphics.DrawImage(g_listItemBgImage, rect);
		}
		else
		{
			graphics.FillRectangle(g_listItemBgColorBrush.get(), rect);
		}
	}

	// 3. 绘制文字 (你的代码已是 GDI+，保持即可)
	Gdiplus::RectF titleRect;
	Gdiplus::RectF subRect;
	if (isSelected)
	{
		const float titleHeight = g_listItemFontSelected1->GetHeight(&graphics);
		const float subHeight = g_listItemFontSelected2->GetHeight(&graphics) * 2.2f; // 限制为两行
		titleRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosSelectedX1),
									static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosSelectedY1),
									static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosSelectedX1),
									titleHeight);
		subRect = Gdiplus::RectF(static_cast<Gdiplus::REAL>(rc.left + g_itemTextPosSelectedX2),
								static_cast<Gdiplus::REAL>(rc.top + g_itemTextPosSelectedY2),
								static_cast<Gdiplus::REAL>(itemWidth - g_itemTextPosSelectedX2), subHeight);
	}
	else
	{
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
	if (isSelected)
	{
		graphics.DrawString(title.c_str(), -1, g_listItemFontSelected1.get(), titleRect, nullptr,
							g_listItemTextColorBrushSelected1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFontSelected2.get(), subRect, &stringFormat,
							g_listItemTextColorBrushSelected2.get());
	}
	else
	{
		graphics.DrawString(title.c_str(), -1, g_listItemFont1.get(), titleRect, nullptr,
							g_listItemTextColorBrush1.get());
		graphics.DrawString(subtitle.c_str(), -1, g_listItemFont2.get(), subRect, &stringFormat,
							g_listItemTextColorBrush2.get());
	}


	// 图标绘制
	
		if (action->getIconBitmap())
		{
			// 假设 action->hIcon 是一个 32-bpp 的 HBITMAP
			// 目标绘制位置
			int iconX;
			int iconY;
			if (isSelected)
			{
				iconX = rc.left + g_itemIconSelectedX;
				iconY = rc.top + g_itemIconSelectedY;
			}
			else
			{
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
	else
	{
		if (g_listFileImageList && action->getIconFilePathIndex() >= 0)
		{
			if (isSelected)
			{
				ImageList_Draw(g_listFileImageList, action->getIconFilePathIndex(), hdc, rc.left + g_itemIconSelectedX,
								rc.top + g_itemIconSelectedY,
								ILD_TRANSPARENT);
			}
			else
			{
				ImageList_Draw(g_listFileImageList, action->getIconFilePathIndex(), hdc, rc.left + g_itemIconX,
								rc.top + g_itemIconY,
								ILD_TRANSPARENT);
			}
			// ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
			// ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
		}
		else
		{
			if (isSelected)
			{
				ImageList_Draw(g_listFileImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconSelectedX,
								rc.top + g_itemIconSelectedY,
								ILD_TRANSPARENT);
			}
			else
			{
				ImageList_Draw(g_listFileImageList, unknown_file_icon_index, hdc, rc.left + g_itemIconX,
								rc.top + g_itemIconY,
								ILD_TRANSPARENT);
			}
		}
	}
}

static LRESULT listViewOnCustomDraw(LPNMLVCUSTOMDRAW lplvcd)
{
	// We are only interested in the stage before the whole control is painted.
	if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		// 这个判断很重要，用来判断这次是要重绘listview背景，还是重绘item，用来解决item和listview绘制冲突
		if (lplvcd->dwItemType != 2)
		{
			return CDRF_NOTIFYITEMDRAW;
		}
		HDC hdc = lplvcd->nmcd.hdc;
		HWND hWnd = lplvcd->nmcd.hdr.hwndFrom;
		RECT rcClient;
		GetClientRect(hWnd, &rcClient);

		Gdiplus::Graphics graphics(hdc);
		graphics.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);
		// 不知道为什么 列表的左边和上边会多空出一个像素
		if (g_skinJson != nullptr)
		{
			const Gdiplus::Rect rect(rcClient.left - 1, rcClient.top - 1, rcClient.right - rcClient.left + 1,
									rcClient.bottom - rcClient.top + 1);
			if (g_listViewBgImage)
			{
				graphics.DrawImage(g_listViewBgImage, rect);
			}
			else
			{
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

static void refreshPluginRunner()
{
	refreshAppLaunchAction();
	// loadRunnerJsonConfig();

	// 添加插件动作
	if (g_pluginManager)
	{
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

inline void textMatching()
{
	// 通知插件用户输入变化
	if (g_pluginManager)
	{
		if (const int16_t pluginId=PluginManager::IsInterceptInputShowResultsDirectly(editTextBuffer); pluginId!=-1){
			filteredActions.clear();
			PluginManager::GetPluginActions(pluginId,filteredActions);
			addActionDone();
			return;
		}
		g_pluginManager->DispatchUserInput(editTextBuffer);
	}
	actionFilter(editTextBuffer);
}

// void ExecutePluginAction(const std::wstring& actionId)
// {
// 	if (g_pluginManager)
// 	{
// 		std::wstring actionIdStr = (actionId);
// 		g_pluginManager->DispatchActionExecute(actionIdStr);
// 	}
// }

inline void editTextInput()
{
	editTextBuffer.resize(256, L'\0');
	editTextBuffer.resize(GetWindowTextW(g_editHwnd, &editTextBuffer[0], 256));
	if (editTextBuffer.empty())
	{
		ListView_SetItemCountEx(g_listViewHwnd, 0, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		return;
	}
	SendMessage(g_listViewHwnd, WM_SETREDRAW, FALSE, 0);
	textMatching();
	SendMessage(g_listViewHwnd, WM_SETREDRAW, TRUE, 0);
}
