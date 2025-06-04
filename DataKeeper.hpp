#pragma once
#include <Shobjidl.h>
#include <string>
#include "MainTools.hpp"

// const int COLOR_UI_BG_VALUE = 110;
//const int COLOR_UI_BG_VALUE = 255;
const int COLOR_UI_BG_VALUE = 170;
const int COLOR_UI_BG_DEEP_VALUE = 130;

const COLORREF COLOR_UI_BG = RGB(COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE);
const COLORREF COLOR_UI_BG_DEEP = RGB(COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE);
const int fontColorGray = 50;
const int COLOR_UI_BG_GLASS = (0xCC << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
// const int COLOR_UI_BG_GLASS = (0x22 << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;

const std::wstring EXE_FOLDER_PATH=GetExecutableFolder();

// 定义回调函数类型，它接受一个整数参数，返回一个整数
typedef int (*CallbackFunction)(int); 

// std::wstring EXE_FOLDER_PATH ={};
// const std::wstring EXE_FOLDER_PATH =MainTools::GetExecutableFolder();

