#include "ListViewManager.h"
#include <shlobj.h>
#include <shlguid.h>
#include <sstream>
#include <Shobjidl.h>    // For IImageList
#include <CommonControls.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")
#include <gdiplus.h>
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

// 编辑框筛选列表内容
void ListViewManager::Filter(const std::wstring& keyword)
{
	ListView_DeleteAllItems(hListView);
	filteredActions.clear();

	if (keyword.empty())
		return;

	std::wstringstream ss(MyToLower(keyword));
	std::wstring word;
	std::vector<std::wstring> words;
	while (ss >> word) words.push_back(word);

	int matchedCount = 0;

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
			if (matchedCount >= 7)
				break;
		}
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
