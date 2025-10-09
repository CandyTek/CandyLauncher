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


template <typename T>
T MyMax(const T& a, const T& b) {
	return (a > b) ? a : b;
}

template <typename T>
T MyMin(const T& a, const T& b) {
	return (a < b) ? a : b;
}


// 辅助函数：计算最大公约数
static int calculateGCD(int a, int b) {
	a = std::abs(a);
	b = std::abs(b);
	while (b != 0) {
		int temp = b;
		b = a % b;
		a = temp;
	}
	return a;
}
