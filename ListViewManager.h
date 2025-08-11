#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <memory>
#include "ListedRunnerPlugin.h"
#include "ThreadPool.h"


class ListViewManager
{
public:
	ListViewManager();
	void Initialize(HWND parent, HINSTANCE hInstance, int x, int y, int width, int height);
	void LoadActions(const std::vector<std::shared_ptr<RunCommandAction>>& actions);
	void Exactmatch(const std::wstring& keyword);
	void Exactmatch_Optimized(const std::wstring& keyword);
	void Fuzzymatch_MultiThreaded(const std::wstring& keyword);
	void Fuzzymatch_MultiThreaded2(const std::wstring& keyword);
	void Fuzzymatch(const std::wstring& keyword);
	void Filter(const std::wstring& keyword);
	[[nodiscard]] HWND GetHandle() const { return hListView; }
	[[nodiscard]] HIMAGELIST GetImageList() const { return hImageList; }
	void OnGetDispInfo(NMLVDISPINFOW* pdi);
	static void InitializeGraphicsResources();
	void Cleanup();
	static void setUnknownFileIcon(HDC hdc, RECT rc);
	void DrawItem(const DRAWITEMSTRUCT* lpDrawItem) const;
	static LRESULT OnCustomDraw(LPNMLVCUSTOMDRAW lplvcd);
	static void MeasureItem(MEASUREITEMSTRUCT* lpMeasureItem);
	HWND hListView = nullptr;
	std::vector<std::shared_ptr<RunCommandAction>> filteredActions;

private:
	HIMAGELIST hImageList = nullptr;
	std::vector<std::shared_ptr<RunCommandAction>> allActions;
	static ThreadPool pool; 

};
