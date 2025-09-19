#pragma once
#include "../Plugin.h"
#include <windows.h>
#include <memory>

#include "CalcAction.hpp"
#include "ExprtkPlus.hpp"
#include "latex.hpp"
#include "util/BaseTools.hpp"
#include "exprtk/exprtk.hpp"
#include <algorithm>
#include <numeric>
#include <regex>
#include <string>
#include <iostream>
#include <cmath>

inline wchar_t editTextBuf[64];
inline std::vector<std::shared_ptr<ActionBase>> allExampleActions;

inline std::string utf8_to_wide_and_substring(const std::wstring& keyword, const int num)
{
#if defined(DEBUG) || _DEBUG
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, keyword.c_str() + num, (int)(keyword.size() - num), NULL, 0,
										NULL, NULL);
	std::string exprStr(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, keyword.c_str() + num, (int)(keyword.size() - num), &exprStr[0], sizeNeeded,
						NULL, NULL);
#else
	const std::string exprStr(keyword.begin()+num+1, keyword.end());
#endif
	return exprStr;
}

class CalcPlugin : public IPlugin
{
private:
	IPluginHost* m_host = nullptr;

public:
	CalcPlugin() = default;
	~CalcPlugin() override = default;

	std::wstring GetPluginName() const override
	{
		return L"计算器";
	}
	
	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.calc";
	}

	std::wstring GetPluginVersion() const override
	{
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override
	{
		return L"科学计算器";
	}


	bool Initialize(IPluginHost* host) override
	{
		m_host = host;
		RefreshAllActions();
		// TestLatex();
		return m_host != nullptr;
	}

	void OnUserSettingsLoadDone() override
	{
		std::unordered_map<std::string, SettingItem> abc = m_host->GetSettingsMap();
		abc["pref_indexed_myapp_extra_launch"].boolValue;
	}

	
	void RefreshAllActions() override
	{
		if (!m_host) return;

		allExampleActions.clear();
		CalcAction action1;
		action1.id = 1;
		action1.title = L"计算结果";
		action1.subTitle = L"";
		action1.iconFilePathIndex = GetSysImageIndex(action1.getIconFilePath());
		allExampleActions.push_back(std::make_shared<CalcAction>(action1));

		CalcAction action2;
		action2.id = 2;
		action2.title = L"化简";
		action2.subTitle = L"";
		action2.iconFilePathIndex = GetSysImageIndex(action2.getIconFilePath());
		allExampleActions.push_back(std::make_shared<CalcAction>(action1));

	}

	void Shutdown() override
	{
		if (m_host)
		{
		}
		allExampleActions.clear();
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<ActionBase>> GetAllActions() override
	{
		if (!m_host) return {};
		return allExampleActions;
	}

	static double EvaluateExpression(const std::wstring& keyword)
	{
		EXPRTK_INIT();
		const std::string& exprStr = utf8_to_wide_and_substring(keyword, 4);
		if (!parser.compile(exprStr, expression))
		{
			// 解析失败，返回 NaN 或者 0
			return std::numeric_limits<double>::quiet_NaN();
		}
		return expression.value();
	}

	static ImprovedLaTeXCalculator calculator;
	
	// todo: 添加变量赋值的功能
	static std::wstring& EvaluateLatex(const std::wstring& keyword)
	{
		static std::wstring resultStr;
		using namespace exprtk;

		// Convert wstring to string
		const std::string exprStr = utf8_to_wide_and_substring(keyword, 5);
		const std::pair<bool, std::string> result = calculator.evaluate(exprStr);

		if (result.first)
		{
			try
			{
				double numericResult = std::stod(result.second); // Convert string to double
				std::cout << "Result: " << numericResult << std::endl;
				swprintf(&resultStr[0], resultStr.capacity(), L"结果: %.15g", numericResult);
				return resultStr;
			}
			catch (const std::invalid_argument& e)
			{
				resultStr = L"Invalid result format: " + utf8_to_wide(e.what());
			} catch (const std::out_of_range& e)
			{
				resultStr = L"Result out of range: " + utf8_to_wide(e.what());
			}
		}
		else
		{
			resultStr = L"Error: " + utf8_to_wide(result.second);
		}
		return resultStr;
	}
	
	static void TestLatex()
	{
		std::wstring expr1 = L"3 + 5"; // Simple addition
		std::wstring result1 = EvaluateLatex(expr1);
		std::wcout << L"Result of '3 + 5': " << result1 << std::endl; // Expected: 8
		std::wstring expr2 = L"\\frac{1}{2} + 2"; // Fraction plus integer
		std::wstring result2 = EvaluateLatex(expr2);
		std::wcout << L"Result of '\\frac{1}{2} + 2': " << result2 << std::endl; // Expected: 2.5
		std::wstring expr3 = L"\\sin(\\frac{\\pi}{2})"; // Sine of pi/2
		std::wstring result3 = EvaluateLatex(expr3);
		std::wcout << L"Result of '\\sin(\\frac{\\pi}{2})': " << result3 << std::endl; // Expected: 1
		std::wstring expr4 = L"x^2 + 4"; // x squared + 4, where x=3
		ImprovedLaTeXCalculator calc;
		calc.setVariable("x", 3); // Set x = 3
		std::wstring result4 = EvaluateLatex(expr4);
		std::wcout << L"Result of 'x^2 + 4' (x=3): " << result4 << std::endl; // Expected: 13
		std::wstring expr5 = L"\\log(100)"; // Log base 10 of 100
		std::wstring result5 = EvaluateLatex(expr5);
		std::wcout << L"Result of '\\log(100)': " << result5 << std::endl; // Expected: 2
		std::wstring expr6 = L"\\frac{\\frac{1}{2}}{3}"; // Nested fraction
		std::wstring result6 = EvaluateLatex(expr6);
		std::wcout << L"Result of '\\frac{\\frac{1}{2}}{3}': " << result6 << std::endl; // Expected: 0.1667
		std::wstring expr7 = L"\\sin(\\frac{\\pi}{4}) + \\cos(\\frac{\\pi}{4})"; // sin(pi/4) + cos(pi/4)
		std::wstring result7 = EvaluateLatex(expr7);
		std::wcout << L"Result of '\\sin(\\frac{\\pi}{4}) + \\cos(\\frac{\\pi}{4})': " << result7 << std::endl;
		// Expected: 1.4142
		std::wstring expr8 = L"(x + 2)^2"; // (x + 2)^2 where x=3
		calc.setVariable("x", 3); // Set x = 3
		std::wstring result8 = EvaluateLatex(expr8);
		std::wcout << L"Result of '(x + 2)^2' (x=3): " << result8 << std::endl; // Expected: 25
		std::wstring expr9 = L"\\sqrt{16}"; // Square root of 16
		std::wstring result9 = EvaluateLatex(expr9);
		std::wcout << L"Result of '\\sqrt{16}': " << result9 << std::endl; // Expected: 4
		std::wstring expr10 = L"\\pi * 2"; // pi * 2
		std::wstring result10 = EvaluateLatex(expr10);
		std::wcout << L"Result of '\\pi * 2': " << result10 << std::endl; // Expected: 6.2832 (approximately 2π)
	}

	bool InterceptInputShowResultsDirectly(const std::wstring& input) override
	{
		if (MyStartsWith2(input, L"calc "))
		{
			swprintf(editTextBuf, 64, L"结果: %.15g", EvaluateExpression(input));
			std::dynamic_pointer_cast<CalcAction>(allExampleActions[0])->subTitle = editTextBuf;
			return true;
		}
		else if (MyStartsWith2(input, L"latex "))
		{
			std::dynamic_pointer_cast<CalcAction>(allExampleActions[0])->subTitle = EvaluateLatex(input);
			return true;
		}
		return false;
	}

	void OnUserInput(const std::wstring& input) override
	{
		if (!m_host) return;
	}

	void OnActionExecute(std::shared_ptr<ActionBase>& action) override
	{
		if (!m_host) return;
		auto exampleAction = std::dynamic_pointer_cast<CalcAction>(action);
		if (!exampleAction) return;

		if (exampleAction->id == 1)
		{
			m_host->ShowMessage(L"示例插件", L"Hello World from Plugin!");
		}
	}
	

};

PLUGIN_EXPORT IPlugin* CreatePlugin()
{
	return new CalcPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin)
{
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion()
{
	return 1;
}
