#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "DataKeeper.hpp"
#include <gdiplus.h>
#include <atomic>


static void getSkinPictureFile(Gdiplus::Image*& image, const std::string& skinKey,int width,int height)
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
		if(MyEndsWith(utf8_to_wide(picturePath),L".9.png")){
			image =RenderNinePatchToSize(utf8_to_wide(picturePath).c_str(),width,height);
			//image =CreateNinePatchBitmap(utf8_to_wide(picturePath).c_str(),MAIN_WINDOW_WIDTH,MAIN_WINDOW_HEIGHT);
		}else{
			image = new Gdiplus::Image(utf8_to_wide(picturePath).c_str());
		}
		if (image->GetLastStatus() != Gdiplus::Ok)
		{
			delete image;
			image = nullptr;
			ShowErrorMsgBox(L"加载背景图片失败");
		}
	}
}

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

static void refreshSkin(std::wstring skinPath)
{
	if (skinPath == L"default") {
		skinPath = DEFAULT_SKIN_PATH;
	}else if (skinPath == L"night_mode") {
		skinPath = NIGHT_SKIN_PATH;
	}

	if(false){
		ShowWindow(g_editHwnd,SW_HIDE);
		ShowWindow(g_listViewHwnd,SW_HIDE);
	}
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

	// --- 2. 更新编辑框 (hEdit) ---
	int editX = g_skinJson.value("editbox_x", 10);
	int editY = g_skinJson.value("editbox_y", 10);
	int editWidth = g_skinJson.value("editbox_width", 580);
	int editHeight = g_skinJson.value("editbox_height", 35);
	g_listItemWidth = g_skinJson.value("item_width", 580);
	g_listItemHeight = g_skinJson.value("item_height", 35);
	if(g_listItemWidth <= 0){
		g_listItemHeight=580;
	}
	if(g_listItemHeight <= 0){
		g_listItemHeight=60;
	}

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

	// 处理背景图片
	getSkinPictureFile(g_BgImage, "window_bg_picture",MAIN_WINDOW_WIDTH,MAIN_WINDOW_HEIGHT);
	//RenderNinePatchToSize
	getSkinPictureFile(g_editBgImage, "editbox_bg_picture",editWidth,editHeight);
	getSkinPictureFile(g_listViewBgImage, "listview_bg_picture",g_itemListWidth,g_itemListHeight);
	getSkinPictureFile(g_listItemBgImage, "item_bg_picture",g_listItemWidth,g_listItemHeight);
	getSkinPictureFile(g_listItemBgImageSelected, "item_bg_picture_selected",g_listItemWidth,g_listItemHeight);

	if (hEditFont)
	{
		SendMessage(g_editHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(hEditFont), TRUE);
	}
	SendMessage(g_editHwnd, WM_NOTIFY_HEDIT_REFRESH_SKIN, 0, TRUE);
	SetWindowPos(g_listViewHwnd, nullptr, listX, listY, g_itemListWidth, g_itemListHeight, SWP_NOZORDER);
	SetWindowPos(g_editHwnd, nullptr, editX, editY, editWidth, editHeight, SWP_NOZORDER);
	int thumbWidth = GetWindowVScrollBarThumbWidth(g_listViewHwnd, true);
	ListView_SetColumnWidth(g_listViewHwnd, 0, g_itemListWidth-thumbWidth-6);
	SendMessage(g_listViewHwnd,WM_LISTVIEW_REFRESH_RESOURCE,0,0);

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

static void RefreshSkinFile() {
	auto prefSkin = std::find_if(g_settings2.rbegin(), g_settings2.rend(),
								 [](const SettingItem &item) { return item.key == "pref_skin"; });
	if (prefSkin == g_settings2.rend()) {
		return;
	}
	g_prefSkinIndex = std::distance(prefSkin, g_settings2.rend()) - 1;
	g_skinFilePaths = FindSkinFiles();

	prefSkin->entries.clear();
	prefSkin->entryValues.clear();

	prefSkin->entryValues.emplace_back("default");
	prefSkin->entries.emplace_back("默认");
	prefSkin->entryValues.emplace_back("night_mode");
	prefSkin->entries.emplace_back("黑夜模式");

	for (const auto &path: g_skinFilePaths) {
		std::filesystem::path p(path);
		std::wstring fileName = p.stem().wstring();
		prefSkin->entryValues.emplace_back(wide_to_utf8(path));
		prefSkin->entries.emplace_back(wide_to_utf8(fileName));
	}
}
