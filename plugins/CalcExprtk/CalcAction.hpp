#pragma once

#include "CalcPluginData.hpp"
#include <limits>
#include <string>
#include <ShlObj.h>

// 图标，只要是文件就可以
inline const wchar_t* const kCalcIconPath = LR"(c:\Windows\System32\calc.exe)";

class CalcAction final : public candy::ActionBase {
public:
	enum class Kind : int8_t {
		// 无操作，例如错误提示
		None,
		// 复制 copyText 到剪贴板
		Copy,
		// 保存 source 中的变量赋值 / 函数定义
		Commit,
		// 将 fillText 填入输入框
		Fill,
		// 清除所有自定义变量和函数
		ClearSymbols,
		// 清除所有计算历史
		ClearHistory,
		// 展开 / 收起 section 指定的帮助分组
		ToggleHelp,
		// 历史记录，回车填入 fillText，可拖拽表达式
		History,
		// 切换三角学单位：弧度 → 度 → 百分度，并保存到设置
		SwitchAngleUnit,
	};

	CalcAction() = default;

	CalcAction(std::wstring title, std::wstring subTitle, const Kind kind, const int iconIndex) : kind(kind) {
		this->title = std::move(title);
		this->subtitle = std::move(subTitle);
		this->iconIndex = iconIndex;
		// 系统图标索引尚未解析时由主程序按路径延迟解析
		this->iconPath = kCalcIconPath;
	}

	Kind kind = Kind::None;
	std::wstring copyText;
	std::wstring fillText;
	// 产生此结果的表达式（不含前缀），Commit 时会重新计算
	std::wstring source;
	double value = std::numeric_limits<double>::quiet_NaN();
	// ToggleHelp 对应的分组
	int8_t section = 0;
};
