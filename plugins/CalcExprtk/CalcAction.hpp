#pragma once

#include "CalcPluginData.hpp"
#include <limits>
#include <string>
#include <ShlObj.h>

class CalcAction final : public BaseAction {
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

	CalcAction() {
		pluginId = m_pluginId;
	}

	CalcAction(std::wstring title, std::wstring subTitle, const Kind kind, const int iconIndex)
		: title(std::move(title)), subTitle(std::move(subTitle)), kind(kind), iconFilePathIndex(iconIndex) {
		pluginId = m_pluginId;
	}

	std::wstring title;
	std::wstring subTitle;
	Kind kind = Kind::None;
	std::wstring copyText;
	std::wstring fillText;
	// 产生此结果的表达式（不含前缀），Commit 时会重新计算
	std::wstring source;
	double value = std::numeric_limits<double>::quiet_NaN();
	// ToggleHelp 对应的分组
	int8_t section = 0;
	int iconFilePathIndex = -1;

	std::wstring& getTitle() override {
		return title;
	}

	std::wstring& getSubTitle() override {
		return subTitle;
	}

	// 图标，只要是文件就可以
	std::wstring& getIconFilePath() override {
		static std::wstring path = LR"(c:\Windows\System32\calc.exe)";
		return path;
	}

	int getIconFilePathIndex() override {
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
	}

	~CalcAction() override {
	}
};
