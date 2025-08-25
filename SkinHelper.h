#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "DataKeeper.hpp"
#include <gdiplus.h>
#include <atomic>

static void getSkinPictureFile(Gdiplus::Image*& image, const std::string& skinKey)
{
	const std::string picturePath = g_skinJson.value(skinKey, "");
	if (image)
	{
		// 删除旧的图片对象
		delete image;
		image = nullptr;
	}
	if (!picturePath.empty())
	{
		image = new Gdiplus::Image(utf8_to_wide(picturePath).c_str());
		if (image->GetLastStatus() != Gdiplus::Ok)
		{
			delete image;
			image = nullptr;
			ShowErrorMsgBox(L"加载背景图片失败");
		}
	}
}


static void refreshSkin(HWND listViewHwnd)
{
	std::wstring skinPath = LR"(C:\Users\Administrator\source\repos\WindowsProject1\skin_test.json)";
	std::ifstream in((skinPath.data()));
	if (!in)
	{
		std::wcerr << L"文件不存在：" << skinPath << std::endl;
		return;
	}

	// 读取整个文件内容（UTF-8 编码）
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string utf8json = buffer.str();

	// 解析 JSON
	try
	{
		g_skinJson = nlohmann::json::parse(utf8json);
		in.close();
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::wcerr << L"JSON 解析错误：" << utf8_to_wide(e.what()) << std::endl;
		return;
	}
	// --- 1. 更新主窗口 ---
	MAIN_WINDOW_WIDTH = g_skinJson.value("window_width", DEFAULT_MAIN_WINDOW_WIDTH);
	MAIN_WINDOW_HEIGHT = g_skinJson.value("window_height", DEFAULT_MAIN_WINDOW_HEIGHT);
	SetWindowPos(g_mainHwnd, nullptr, 0, 0, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);

	int windowOpacity = g_skinJson.value("window_opacity", 255);
	if (windowOpacity < 0) windowOpacity = 0;
	if (windowOpacity > 255) windowOpacity = 255;
	if (g_lastWindowOpacity != windowOpacity)
	{
		g_lastWindowOpacity = windowOpacity;
		SetLayeredWindowAttributes(g_mainHwnd, 0, static_cast<BYTE>(windowOpacity), LWA_ALPHA);
	}

	// 处理背景图片
	getSkinPictureFile(g_BgImage, "window_bg_picture");
	getSkinPictureFile(g_editBgImage, "editbox_bg_picture");
	getSkinPictureFile(g_listViewBgImage, "listview_bg_picture");
	getSkinPictureFile(g_listItemBgImage, "item_font_bg_picture");
	getSkinPictureFile(g_listItemBgImageSelected, "item_font_bg_picture_selected");

	// --- 2. 更新编辑框 (hEdit) ---
	int editX = g_skinJson.value("editbox_x", 10);
	int editY = g_skinJson.value("editbox_y", 10);
	int editWidth = g_skinJson.value("editbox_width", 580);
	int editHeight = g_skinJson.value("editbox_height", 35);
	SetWindowPos(g_editHwnd, nullptr, editX, editY, editWidth, editHeight, SWP_NOZORDER);

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
	if (hEditFont)
	{
		SendMessage(g_editHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(hEditFont), TRUE);
	}
	SendMessage(g_editHwnd, WM_NOTIFY_HEDIT_REFRESH_SKIN, 0, TRUE);

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
	g_itemListWidth = g_skinJson.value("listview_width", 580);
	g_itemListHeight = g_skinJson.value("listview_height", 380);
	SetWindowPos(listViewHwnd, nullptr, listX, listY, g_itemListWidth, g_itemListHeight, SWP_NOZORDER);
	int thumbWidth = GetWindowVScrollBarThumbWidth(listViewHwnd, true);
	ListView_SetColumnWidth(listViewHwnd, 0, g_itemListWidth-thumbWidth-6);
	//	listViewManager.InitializeGraphicsResources(); // 确保字体和画刷已初始化

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

	// 暴力强制渲染
	RECT rc;
	GetWindowRect(g_mainHwnd, &rc);
	// 临时缩小窗口
	SetWindowPos(g_mainHwnd, nullptr, rc.left, rc.top, rc.left + 1, rc.top + 1,
				 SWP_NOZORDER);
	// 恢复原大小
	SetWindowPos(g_mainHwnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
				 SWP_NOZORDER);
	ShowWindow(g_mainHwnd,SW_FORCEMINIMIZE);
	SetTimer(g_mainHwnd, TIMER_SHOW_WINDOW, 50, nullptr);
}
