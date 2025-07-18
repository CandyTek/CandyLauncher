#pragma once
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <memory>
#include "ListedRunnerPlugin.h"


class ListViewManager
{
public:
	ListViewManager();
	void Initialize(HWND parent, HINSTANCE hInstance, int x, int y, int width, int height);
	void LoadActions(const std::vector<std::shared_ptr<RunCommandAction>>& actions);
	void Exactmatch(const std::wstring& keyword);
	void Fuzzymatch(const std::wstring& keyword);
	void Filter(const std::wstring& keyword);
	[[nodiscard]] HWND GetHandle() const { return hListView; }
	[[nodiscard]] HIMAGELIST GetImageList() const { return hImageList; }
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
};
