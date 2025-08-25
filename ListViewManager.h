#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <memory>
#include "ListedRunnerPlugin.h"
#include "ThreadPool.h"

LRESULT CALLBACK ListViewSubclassProc(HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam,
											 UINT_PTR, const DWORD_PTR dwRefData);

class ListViewManager
{
public:
	static void Initialize(HWND parent, HINSTANCE hInstance, int x, int y, int width, int height);
	static void LoadActions(const std::vector<std::shared_ptr<RunCommandAction>>& actions);
	static void Exactmatch(const std::wstring& keyword);
	static void Exactmatch_Optimized(const std::wstring& keyword);
	static void Fuzzymatch_MultiThreaded(const std::wstring& keyword);
	static void Fuzzymatch_MultiThreaded2(const std::wstring& keyword);
	static void Fuzzymatch(const std::wstring& keyword);
	static void Filter(const std::wstring& keyword);
	static HIMAGELIST GetImageList() { return hImageList; }
	static void OnGetDispInfo(NMLVDISPINFOW* pdi);
	static void InitializeGraphicsResources();
	static void Cleanup();
	static void setUnknownFileIcon(HDC hdc, RECT rc);
	static void DrawItem(const DRAWITEMSTRUCT* lpDrawItem);
	static LRESULT OnCustomDraw(LPNMLVCUSTOMDRAW lplvcd);
	static void MeasureItem(MEASUREITEMSTRUCT* lpMeasureItem);
	
	static std::vector<std::shared_ptr<RunCommandAction>> filteredActions;

private:
	static HIMAGELIST hImageList;
	static std::vector<std::shared_ptr<RunCommandAction>> allActions;
	static ThreadPool pool; 

};
