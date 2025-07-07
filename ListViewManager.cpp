#include "ListViewManager.h"
#include <shlobj.h>
#include <shlguid.h>
#include <sstream>
#include <Shobjidl.h>    // For IImageList
#include <commoncontrols.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")
#include <gdiplus.h>
// TODO: 模糊算法库 rapidfuzz 在 vs2022里很难加载，在clion可以
#ifdef _MSC_VER
#else
#include <rapidfuzz/fuzz.hpp>
#endif

#include "DataKeeper.hpp"

ListViewManager::ListViewManager() = default;
// static ULONG_PTR gdiplusToken;

typedef HRESULT (WINAPI*SHGetImageListPtr)(int iImageList, REFIID riid, void** ppv);

HIMAGELIST GetSystemImageListEx(const int size)
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
	// HRESULT hr = pSHGetImageList(size, IID_IImageList, (void**)&pImageList);
	HRESULT hr = pSHGetImageList(size, __uuidof(IImageList), reinterpret_cast<void**>(&pImageList));

	if (FAILED(hr))
	{
		FreeLibrary(hShell32);
		return nullptr;
	}

	// Release handle after use — or detach to HIMAGELIST
	HIMAGELIST hImageList = nullptr;
	// pImageList->QueryInterface(IID_IImageList, (void**)&hImageList);
	pImageList->QueryInterface(__uuidof(IImageList), reinterpret_cast<void**>(&hImageList));


	pImageList->Release();
	FreeLibrary(hShell32);
	return hImageList;
}


// 系统图像列表的索引
static int GetSysImageIndex(const std::wstring& filePath)
{
	SHFILEINFOW sfi = {};
	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
					SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES);
	return sfi.iIcon;
}


static HIMAGELIST GetSystemImageList(bool largeIcon)
{
	SHFILEINFOW sfi = {};
	//UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | (largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON);
	const UINT flags = SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_OPENICON;

	// SHGetFileInfo 返回值是图像列表句柄，sfi.hIcon 是图标句柄，不要搞错
	return reinterpret_cast<HIMAGELIST>(SHGetFileInfoW(L".txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags));
}

void ListViewManager::Initialize(HWND parent, HINSTANCE hInstance, const int x, const int y, const int width,
								const int height)
{
	// 初始化 Common Controls
	INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES};
	InitCommonControlsEx(&icex);

	// ListView 控件（使用 OWNERDRAWFIXED）
	const DWORD style = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_OWNERDRAWFIXED |
		LVS_NOCOLUMNHEADER;
	//DWORD style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER;

	hListView = CreateWindowW(WC_LISTVIEW, L"", style,
							x, y, width, height, parent, reinterpret_cast<HMENU>(2), hInstance, nullptr);

	// 设置列 0（主标题），即使我们不显示它，也必须添加
	LVCOLUMN col = {LVCF_TEXT | LVCF_WIDTH};
	col.pszText = const_cast<LPWSTR>(L"Title");
	col.cx = 500;
	ListView_InsertColumn(hListView, 0, &col);

	//hImageList = GetSystemImageList(true);
	hImageList = GetSystemImageListEx(SHIL_EXTRALARGE); // 使用 48x48 图标

	ListView_SetImageList(hListView, hImageList, LVSIL_NORMAL);
}

// 加载快捷方式项到列表里
void ListViewManager::LoadActions(const std::vector<std::shared_ptr<RunCommandAction>>& actions)
{
	// 保存引用，便于后续点击等操作
	allActions = actions;
	ListView_DeleteAllItems(hListView);

	for (size_t i = 0; i < actions.size(); ++i)
	{
		const std::shared_ptr<RunCommandAction>& action = actions[i];
		// 获取图标
		if (action->iImageIndex == -1)
			action->iImageIndex = GetSysImageIndex(action->GetTargetPath());
		// 插入 ListView 项
		LVITEM item = {};
		item.mask = LVIF_TEXT | LVIF_IMAGE;
		item.iItem = static_cast<int>(i);
		item.iImage = action->iImageIndex;
		//wprintf(L"当前索引: %d\n", action->iImageIndex);
		item.pszText = const_cast<LPWSTR>(action->GetTitle().c_str());
		// ListView_InsertItem(hListView, &item);
		//ListView_SetItemText(hListView, item.iItem, 1, const_cast<LPWSTR>(action->GetTargetPath().c_str()));
	}

	filteredActions = actions;
}

void ListViewManager::Exactmatch(const std::wstring& keyword)
{
	int matchedCount = 0;

	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) words.push_back(word);

	for (const auto& action : allActions)
	{
		const std::wstring& runCommand = action->RunCommand;

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
			const std::wstring& title = action->GetTitle();
			const std::wstring& subtitle = action->GetSubtitle();

			LVITEM item = {};
			item.mask = LVIF_TEXT | LVIF_IMAGE;
			item.iItem = ListView_GetItemCount(hListView);
			item.pszText = const_cast<LPWSTR>(title.c_str());
			item.iImage = (action->iImageIndex == -1) ? 0 : action->iImageIndex;

			ListView_InsertItem(hListView, &item);
			ListView_SetItemText(hListView, item.iItem, 1, const_cast<LPWSTR>(subtitle.c_str()));

			filteredActions.push_back(action); // <-- 保存筛选后的 action 顺序
			matchedCount++;
			if (!pref_max_search_results <= 0 && matchedCount >= pref_max_search_results)
				break;
		}
	}
}

// --- 修改后的模糊匹配函数 ---
// 定义一个结构体来存储 action 及其匹配分数
struct ScoredAction
{
	// const RunCommandAction* action;
	std::shared_ptr<RunCommandAction> action;
	double score;

	// 用于排序的比较运算符
	bool operator>(const ScoredAction& other) const
	{
		return score > other.score;
	}
};

void ListViewManager::Fuzzymatch(const std::wstring& keyword)
{
	// 如果关键字为空，则不显示任何内容或显示默认列表（根据你的需求）
	if (keyword.empty())
	{
		ListView_DeleteAllItems(hListView);
		filteredActions.clear();
		return;
	}

	// 1. 准备数据：将关键字转为小写
	const std::wstring lowerKeyword = MyToLower(keyword);


	std::vector<ScoredAction> scoredActions;
	const double SCORE_THRESHOLD = 70.0; // 匹配分数阈值，低于此分数的项将被忽略

	// 2. 计算分数：为所有 action 计算匹配度分数
	for (const std::shared_ptr<RunCommandAction>& action : allActions)
	{
		// 关键改动：我们将标题和副标题拼接起来作为搜索目标，这通常比 runCommand 更符合用户预期
		// const std::wstring searchableText = MyToLower(action->GetTitle() + L" " + action->GetSubtitle());
		const std::wstring searchableText = action->RunCommand;

		// 使用 rapidfuzz::fuzz::WRatio 计算加权比率分数。
		// WRatio 对于不同长度的字符串和乱序的单词有很好的效果，是通用的优选。
#ifdef _MSC_VER
		double score = 0;
#else
		double score = rapidfuzz::fuzz::WRatio(lowerKeyword, searchableText);
#endif

		if (score >= SCORE_THRESHOLD)
		{
			scoredActions.push_back({action, score});
		}
	}

	// 3. 排序：按分数从高到低排序
	std::sort(scoredActions.begin(), scoredActions.end(), std::greater<ScoredAction>());
	// std::sort(scoredActions.begin(), scoredActions.end(),
	// 		  [](const ScoredAction& a, const ScoredAction& b) {
	// 			  return a > b;  // 按照分数从大到小排序
	// 		  });

	// 4. 显示结果：清空并填充 ListView
	ListView_DeleteAllItems(hListView);
	filteredActions.clear();

	int matchedCount = 0;
	for (const auto& scored : scoredActions)
	{
		const std::shared_ptr<RunCommandAction> action = scored.action;
		const std::wstring& title = action->GetTitle();
		const std::wstring& subtitle = action->GetSubtitle();

		LVITEMW item = {}; // 使用 LVITEMW 明确表示宽字符
		item.mask = LVIF_TEXT | LVIF_IMAGE;
		item.iItem = ListView_GetItemCount(hListView); // 总是插入到末尾
		item.pszText = const_cast<LPWSTR>(title.c_str());
		item.iImage = (action->iImageIndex == -1) ? 0 : action->iImageIndex;

		int newItemIndex = ListView_InsertItem(hListView, &item);
		if (newItemIndex != -1)
		{
			ListView_SetItemText(hListView, newItemIndex, 1, const_cast<LPWSTR>(subtitle.c_str()));

			// 保存筛选和排序后的 action 顺序
			filteredActions.push_back(action);

			matchedCount++;
			if (!pref_max_search_results <= 0 && matchedCount >= pref_max_search_results)
				break;
		}
	}
}

// 编辑框筛选列表内容
void ListViewManager::Filter(const std::wstring& keyword)
{
	ListView_DeleteAllItems(hListView);
	filteredActions.clear();

	if (keyword.empty())
		return;

	if (pref_fuzzy_match)
	{
		Fuzzymatch(keyword);
	}
	else
	{
		Exactmatch(keyword);
	}

	if (!filteredActions.empty())
	{
		// 清除所有项的选中状态
		//ListView_SetItemState(hListView, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		// 设置第一项为选中状态
		ListView_SetItemState(hListView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		// 确保第一项可见
		ListView_EnsureVisible(hListView, 0, FALSE);
	}
}


void ListViewManager::MeasureItem(MEASUREITEMSTRUCT* lpMeasureItem)
{
	lpMeasureItem->itemHeight = 60;
}

HBRUSH hNormalBrush = CreateSolidBrush(COLOR_UI_BG);
HBRUSH hSelectedBrush = CreateSolidBrush(COLOR_UI_BG_DEEP);

std::unique_ptr<Gdiplus::Font> titleFont;
std::unique_ptr<Gdiplus::Font> subFont;
std::unique_ptr<Gdiplus::SolidBrush> whiteBrush;
std::unique_ptr<Gdiplus::SolidBrush> grayBrush;
std::unique_ptr<Gdiplus::SolidBrush> blackBrush;
bool fontsInitialized = false;

void InitializeGraphicsResources()
{
	if (fontsInitialized) return;
	Gdiplus::FontFamily fontFamily(L"Segoe UI");
	titleFont = std::make_unique<Gdiplus::Font>(&fontFamily, 14, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	subFont = std::make_unique<Gdiplus::Font>(&fontFamily, 12, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	whiteBrush = std::make_unique<Gdiplus::SolidBrush>(Gdiplus::Color(255, 255, 255, 255));
	blackBrush = std::make_unique<Gdiplus::SolidBrush>(Gdiplus::Color(255, 0, 0, 0));
	grayBrush = std::make_unique<Gdiplus::SolidBrush>(Gdiplus::Color(255, fontColorGray, fontColorGray, fontColorGray));
	fontsInitialized = true;
}

void CleanupGraphicsResources()
{
	// 如果不需要，可以直接让 unique_ptr 自动销毁
	titleFont.reset();
	subFont.reset();
	whiteBrush.reset();
	grayBrush.reset();
	blackBrush.reset();
	fontsInitialized = false;
}


// 通过一个 thread_local 存储 Gdiplus::Graphics 缓存，避免每次都重新创建
Gdiplus::Graphics* GetCachedGraphics(HDC hdc)
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
void ListViewManager::Cleanup()
{
	// 释放 ListView 的图像列表
	if (hImageList)
	{
		ImageList_Destroy(hImageList);
		hImageList = nullptr;
	}

	// 清理 GDI+ 资源
	CleanupGraphicsResources();
}


void ListViewManager::DrawItem(const DRAWITEMSTRUCT* lpDrawItem) const
{
	const UINT index = lpDrawItem->itemID;
	if (index < 0 || index >= static_cast<int>(filteredActions.size()))
		return;

	std::shared_ptr<RunCommandAction> action = filteredActions[index];
	HDC hdc = lpDrawItem->hDC;
	const RECT rc = lpDrawItem->rcItem;

	// GDI 背景填充
	if (lpDrawItem->itemState & ODS_SELECTED)
	{
		FillRect(hdc, &rc, hSelectedBrush);
	}
	else
	{
		FillRect(hdc, &rc, hNormalBrush);
	}

	// GDI+ 绘制文字
	//Graphics graphics(hdc);
	//graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
	Gdiplus::Graphics* graphics = GetCachedGraphics(hdc);
	//graphics->DrawString(...);

	InitializeGraphicsResources();

	const int paddingLeft = 60;
	const Gdiplus::RectF titleRect(static_cast<Gdiplus::REAL>(rc.left + paddingLeft),
									static_cast<Gdiplus::REAL>(rc.top + 2),
									static_cast<Gdiplus::REAL>(rc.right - rc.left - paddingLeft), 18);
	const Gdiplus::RectF subRect(static_cast<Gdiplus::REAL>(rc.left + paddingLeft),
								static_cast<Gdiplus::REAL>(rc.top + 20),
								static_cast<Gdiplus::REAL>(rc.right - rc.left - paddingLeft), 18);

	const std::wstring& title = action->GetTitle();
	const std::wstring& subtitle = action->GetSubtitle();

	graphics->DrawString(title.c_str(), -1, titleFont.get(), titleRect, nullptr, blackBrush.get());
	graphics->DrawString(subtitle.c_str(), -1, subFont.get(), subRect, nullptr, grayBrush.get());

	// 图标绘制（使用 ImageList）
	if (hImageList && action->iImageIndex >= 0)
		ImageList_Draw(hImageList, action->iImageIndex, hdc, rc.left + 4, rc.top + 4, ILD_TRANSPARENT);
}
