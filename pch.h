// pch.h: 这是预编译头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

// 添加要在此处预编译的标头
#include "framework.h" // 通常包含了 <windows.h> 和其他 WinAPI 头文件
#include "targetver.h"
#include "Resource.h"

// 添加常用的标准库
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>

// 添加第三方库
#include "json.hpp"

// 添加您自己项目中不常变动的核心头文件
#include "Constants.hpp"
#include "BaseTools.hpp"

#endif //PCH_H
