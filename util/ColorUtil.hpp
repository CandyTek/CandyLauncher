#pragma once

#include <algorithm>
#include <chrono>
#include <windows.h>
#include <string>
#include <sstream>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <ShlGuid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <array>


// 辅助函数：将 #RRGGBB 格式的字符串转为 COLORREF
static COLORREF HexToCOLORREF(const std::string& hex) {
	if (hex.length() < 7 || hex[0] != '#') {
		return RGB(0, 0, 0); // 格式错误返回黑色
	}
	long r = std::stol(hex.substr(1, 2), nullptr, 16);
	long g = std::stol(hex.substr(3, 2), nullptr, 16);
	long b = std::stol(hex.substr(5, 2), nullptr, 16);
	return RGB(r, g, b);
}

// 辅助函数：将 #AARRGGBB 或 #RRGGBB 格式的字符串转为 Gdiplus::Color
static Gdiplus::Color HexToGdiplusColor(const std::string& hex) {
	if (hex.empty() || hex[0] != '#') {
		return Gdiplus::Color(255, 0, 0, 0); // 默认不透明黑色
	}

	// 检查格式：
	// - #RRGGBB（6位） → 默认 Alpha=255（不透明）
	// - #AARRGGBB（8位） → 包含 Alpha 通道
	if (hex.length() == 7) // #RRGGBB
	{
		int r = std::stoi(hex.substr(1, 2), nullptr, 16);
		int g = std::stoi(hex.substr(3, 2), nullptr, 16);
		int b = std::stoi(hex.substr(5, 2), nullptr, 16);
		return Gdiplus::Color(255, r, g, b); // 默认不透明
	} else if (hex.length() == 9) // #AARRGGBB
	{
		int a = std::stoi(hex.substr(1, 2), nullptr, 16);
		int r = std::stoi(hex.substr(3, 2), nullptr, 16);
		int g = std::stoi(hex.substr(5, 2), nullptr, 16);
		int b = std::stoi(hex.substr(7, 2), nullptr, 16);
		return Gdiplus::Color(a, r, g, b); // 包含 Alpha
	} else {
		return Gdiplus::Color(255, 0, 0, 0); // 格式错误返回不透明黑色
	}
}

static bool isValidHexColor(const std::string& color) {
	// 检查长度：6位（#RRGGBB）或8位（#RRGGBBAA）
	if (color.length() != 7 && color.length() != 9) {
		return false;
	}

	// 检查是否以#开头
	if (color[0] != '#') {
		return false;
	}

	// 检查其余字符是否为十六进制数字
	return std::all_of(color.begin() + 1, color.end(), [](char c) {
		return std::isxdigit(static_cast<unsigned char>(c));
	});
}

static std::string validateHexColor(const std::string& color, const std::string& defaultColor) {
	if (color.empty() || !isValidHexColor(color)) {
		return defaultColor;
	}
	return color;
}

static std::string validateHexColor(const std::string& color) {
	return validateHexColor(color, "#FFFFFF");
}
