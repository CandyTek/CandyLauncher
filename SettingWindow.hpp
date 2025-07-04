#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include "resource.h"
#include "json.hpp"
#include <set>
#include <iostream>
#include "BaseTools.hpp"
#include "MainTools.hpp"
#include "ScrollViewHelper.hpp"
#include "SettingsHelper.hpp"

// static std::vector<std::vector<HWND>> hCtrlsByTab; // tab下所有控件句柄
// static std::vector<HWND> tabContainers;
// static std::vector<SettingItem> settings2;
static std::vector<std::wstring> subPages; // 所有tab
static std::vector<HWND> hTabButtons; // tab按钮句柄
static int currentSubPageIndex = 0;
static HWND hSettingWnd = nullptr;

static HFONT hFontDefault = CreateFontW(
	20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
	DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
	CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei UI");


static void LoadSettingList()
{
	const std::string settingsText = GetSettingsJsonText();
	// ShowErrorMsgBox(settingsText);
	settings2 = ParseConfig(settingsText);
	nlohmann::json userConfig = loadUserConfig();
	// 将用户设置合并到主要设置结构中。此此更新在UI构建之前更新每个项目的“ defvalue”。
	if (!userConfig.is_null() && !userConfig.empty())
	{
		for (auto& setting : settings2)
		{
			if (userConfig.contains(setting.key))
			{
				// 该键存在一个保存的值。用用户的值将设置的默认值置换。Nlohmann:: JSON库自动处理类型转换。
				setting.defValue = userConfig[setting.key];
			}
		}
	}
	LoadSettingsMap();
}


static void ShowSettingsWindow(HINSTANCE hInstance, HWND hParent)
{
	if (hSettingWnd != nullptr)
	{
		ShowWindow(hSettingWnd, SW_SHOW);
		return;
	}
	// 获取屏幕宽度和高度
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 计算居中位置
	int x = (screenWidth - SETTINGS_WINDOW_WIDTH) / 2;
	int y = (screenHeight - SETTINGS_WINDOW_HEIGHT) / 2;

	hSettingWnd = CreateWindowExW(
		WS_EX_ACCEPTFILES, // 扩展样式
		L"SettingsWndClass", // 窗口类名
		L"设置", // 窗口标题
		WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // 窗口样式
		x, y, // 初始位置
		SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT, // 宽度和高度
		hParent, // 父窗口
		nullptr, // 菜单
		hInstance, // 实例句柄
		nullptr // 附加参数
	);


	ShowWindow(hSettingWnd, SW_SHOW);
	UpdateWindow(hSettingWnd);
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		{
			MyScrollViewRegisterClass();
			// --------- 1. 收集所有subPage名 ---------
			std::set<std::wstring> tabSet;
			for (const auto& sub : settings2)
			{
				std::wstring name = utf8_to_wide(sub.subPage);
				if (tabSet.insert(name).second)
				{
					// insert() 返回 pair<iterator, bool>，第二个为true表示是新插入
					subPages.push_back(name);
				}
			}

			// --------- 2. 创建Tab按钮（左侧） ---------
			int tabY = 10;
			for (size_t i = 0; i < subPages.size(); ++i)
			{
				std::wstring label = tabNameMap.count(subPages[i]) ? tabNameMap[subPages[i]] : subPages[i];

				HWND hTabBtn = CreateWindowW(L"BUTTON", label.c_str(),
											WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
											10, tabY, 80, 30, hwnd, (HMENU)(2000 + i), nullptr, nullptr);
				hTabButtons.push_back(hTabBtn);
				tabY += 40;
			}

			// 3. 创建每个Tab的容器
			tabContainers.resize(subPages.size());
			for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx)
			{
				HWND hContainer = CreateWindowExW(
					WS_EX_ACCEPTFILES | WS_EX_COMPOSITED,
					L"SCROLLVIEW", nullptr,
					WS_CHILD | WS_VISIBLE | WS_VSCROLL,
					100, 0, 600, 380, hwnd, nullptr,GetModuleHandle(nullptr), nullptr);
				tabContainers[tabIdx] = hContainer;
			}

			// 4. 在每个容器中生成控件
			hCtrlsByTab.clear();
			hCtrlsByTab.resize(subPages.size());
			for (size_t tabIdx = 0; tabIdx < subPages.size(); ++tabIdx)
			{
				int y = 10;
				int contorlX = 200;
				std::vector<HWND> ctrls;
				HWND hParent = tabContainers[tabIdx];

				for (size_t i = 0; i < settings2.size(); ++i)
				{
					const auto& item = settings2[i];
					if (utf8_to_wide(item.subPage) != subPages[tabIdx])
						continue;

					std::wstring text = utf8_to_wide(item.title);

					// label
					HWND hLabel;
					int labelHeight;
					if (item.type == "text")
					{
						int labelWidth = 300;
						labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
						hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
												WS_CHILD | WS_VISIBLE,
												10, y, labelWidth, labelHeight, hParent, nullptr, nullptr, nullptr);


						SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFontDefault, TRUE);
						y += labelHeight;
						continue;
					}
					else
					{
						int labelWidth = 180;
						labelHeight = GetLabelHeight(hwnd, text, labelWidth, hFontDefault);
						hLabel = CreateWindowW(L"STATIC", utf8_to_wide(item.title).c_str(),
												WS_CHILD | WS_VISIBLE,
												10, y, labelWidth, labelHeight, hParent, nullptr, nullptr, nullptr);
						SendMessageW(hLabel, WM_SETFONT, (WPARAM)hFontDefault, TRUE);
					}

					ctrls.push_back(hLabel);

					HWND hCtrl = nullptr;
					if (item.type == "bool")
					{
						hCtrl = CreateWindowW(L"BUTTON", L"",
											WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
											// WS_CHILD | WS_VISIBLE | BS_CHECKBOX,
											contorlX, y, 20, 25, hParent, (HMENU)(3000 + i), nullptr,
											nullptr);
						if (item.defValue.is_number_integer() && item.defValue.get<int>() == 1)
							SendMessage(hCtrl, BM_SETCHECK, BST_CHECKED, 0);
					}
					else if (item.type == "string")
					{
						hCtrl = CreateWindowW(L"EDIT",
											utf8_to_wide(item.defValue.get<std::string>()).c_str(),
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
											contorlX, y, 200, 25, hParent, (HMENU)(3000 + i), nullptr,
											nullptr);
					}
					else if (item.type == "stringArr")
					{
						std::wstring defVal;
						for (auto& s : item.defValue)
							defVal += utf8_to_wide(s.get<std::string>()) + L"\r\n";
						hCtrl = CreateWindowW(L"EDIT",
											defVal.c_str(),
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE | WS_VSCROLL,
											contorlX, y, 200, 50, hParent, (HMENU)(3000 + i), nullptr,
											nullptr);
						y += 30;
					}
					else if (item.type == "list")
					{
						hCtrl = CreateWindowW(L"COMBOBOX", L"",
											WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST| WS_VSCROLL,
											contorlX, y, 200, 300, hParent, (HMENU)(3000 + i),
											nullptr, nullptr);
						// 1. 獲取 ComboBox 的詳細資訊，主要是為了得到下拉列表的句柄 (hwndList)
						COMBOBOXINFO cbi = {sizeof(COMBOBOXINFO)};
						GetComboBoxInfo(hCtrl, &cbi);
						if (cbi.hwndCombo) // 確保列表句柄有效
						{
							SetWindowSubclass(cbi.hwndCombo, ComboBoxListSubclassProc, 1, (DWORD_PTR)hParent);
						}
						// SetWindowSubclass(hCtrl, ComboBoxListSubclassProc, 2, (DWORD_PTR)hParent);

						const auto& entries = item.entries;
						const auto& entryValues = item.entryValues;
						int selIndex = 0;

						for (size_t j = 0; j < entries.size(); ++j)
						{
							std::wstring entryW = utf8_to_wide(entries[j]);
							SendMessageW(hCtrl, CB_ADDSTRING, 0, (LPARAM)entryW.c_str());

							if (entryValues[j] == item.defValue)
								selIndex = static_cast<int>(j);
						}
						SendMessageW(hCtrl, CB_SETCURSEL, selIndex, 0);
					}
					else if (item.type == "long")
					{
						hCtrl = CreateWindowW(L"EDIT",
											std::to_wstring(item.defValue.get<long>()).c_str(),
											WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_NUMBER,
											contorlX, y, 100, 25, hParent, (HMENU)(3000 + i),
											nullptr, nullptr);
					}
					else if (item.type == "text")
					{
					}

					ctrls.push_back(hCtrl);
					y += MyMax(35, labelHeight);
				}

				int contentHeight = y + 20; // y 是最后一个控件的底部位置
				int visibleHeight = 380;

				int maxScroll = MyMax(0, contentHeight - visibleHeight);
				SetScrollRange(hParent, SB_VERT, 0, maxScroll, TRUE);
				SetScrollPos(hParent, SB_VERT, 0, TRUE);

				hCtrlsByTab[tabIdx] = ctrls;
			}

			// --------- 5. 只显示当前tab内容 ---------
			for (size_t tabIdx = 0; tabIdx < tabContainers.size(); ++tabIdx)
				ShowWindow(tabContainers[tabIdx], tabIdx == 0 ? SW_SHOW : SW_HIDE);

			// --------- 6. 保存按钮 ---------
			CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
						350, 400, 70, 30, hwnd, (HMENU)9999, nullptr, nullptr);
			break;
		}
	case WM_MOUSEWHEEL:
		SendMessage(tabContainers[currentSubPageIndex], WM_MOUSEWHEEL, wParam, lParam);
		break;
	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			SetBkMode(hdcStatic, TRANSPARENT); // 设置背景透明
			return (INT_PTR)GetStockObject(NULL_BRUSH); // 返回一个透明画刷
		}

	case WM_COMMAND:
		// --- 切换Tab ---
		if (LOWORD(wParam) >= 2000 && LOWORD(wParam) < 2000 + hTabButtons.size())
		{
			int newIdx = LOWORD(wParam) - 2000;
			if (newIdx == currentSubPageIndex)
				break;

			ShowWindow(tabContainers[currentSubPageIndex], SW_HIDE);
			ShowWindow(tabContainers[newIdx], SW_SHOW);
			currentSubPageIndex = newIdx;

			break;
		}
		// --- 保存 ---
		else if (LOWORD(wParam) == 9999)
		{
			saveConfig(hwnd, subPages, settings2, hCtrlsByTab);
			break;
		}
	case WM_DESTROY:
	case WM_CLOSE:
		{
			ShowWindow(hwnd, SW_HIDE);
			return 0;
		}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
