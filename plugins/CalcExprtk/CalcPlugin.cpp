#include "../Plugin.hpp"
#include <windows.h>
#include <memory>

#include "CalcAction.hpp"
#include "CalcPluginData.hpp"
#include "ExprtkWrapper.hpp"
#include "latex.hpp"
#include "util/BaseTools.hpp"
#include "exprtk/exprtk.hpp"
#include <algorithm>
#include <numeric>
#include <regex>
#include <string>
#include <iostream>
#include <cmath>

#include "util/BitmapUtil.hpp"


inline wchar_t editTextBuf[64];
inline std::vector<std::shared_ptr<BaseAction>> allPluginActions;

inline std::string utf8_to_wide_and_substring(const std::wstring& keyword, const unsigned long long num) {
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

inline std::wstring startStr = L"=";
inline bool isDetectReplace = true;

class CalcPlugin : public IPlugin {
private:

public:
	CalcPlugin() = default;
	~CalcPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"计算器";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.calc";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"科学计算器";
	}


	bool Initialize(IPluginHost* host) override {
		m_host = host;
		// TestLatex();
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.calc.start_str",
			"title": "直接激活命令",
			"type": "string",
			"subPage": "plugin",
			"defValue": "="
		},
		{
			"key": "com.candytek.calc.autoreplace",
			"title": "如果查询以 “=” 结尾，则替换输入",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.trigonometric.units",
			"title": "三角学单位",
			"type": "list",
			"subPage": "plugin",
			"entries": [
				"弧度",
				"度",
				"百分度"
			],
			"entryValues": [
			    "radian",
			    "degree",
			    "gradian"
			],
			"defValue": "radian"
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.calc.start_str").stringValue);
		isDetectReplace = (m_host->GetSettingsMap().at("com.candytek.calc.autoreplace").boolValue);
	}


	void RefreshAllActions() override {
		if (!m_host) return;

		allPluginActions.clear();
		CalcAction action1;
		action1.id = 1;
		action1.title = L"计算结果";
		action1.subTitle = L"";
		action1.iconFilePathIndex = GetSysImageIndex(action1.getIconFilePath());
		allPluginActions.push_back(std::make_shared<CalcAction>(action1));

		CalcAction action2;
		action2.id = 2;
		action2.title = L"化简";
		action2.subTitle = L"";
		action2.iconFilePathIndex = GetSysImageIndex(action2.getIconFilePath());
		allPluginActions.push_back(std::make_shared<CalcAction>(action1));
	}

	void Shutdown() override {
		if (m_host) {
		}
		allPluginActions.clear();
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!m_host) return {};
		return allPluginActions;
	}

	static double EvaluateExpression(const std::wstring& keyword) {
		EXPRTK_INIT();
		const std::string& exprStr = utf8_to_wide_and_substring(keyword, startStr.size());
		if (!parser.compile(exprStr, expression)) {
			// 解析失败，返回 NaN 或者 0
			return std::numeric_limits<double>::quiet_NaN();
		}
		return expression.value();
	}

	static ImprovedLaTeXCalculator calculator;

	// todo: 添加变量赋值的功能
	static std::wstring& EvaluateLatex(const std::wstring& keyword) {
		static std::wstring resultStr;
		using namespace exprtk;

		// Convert wstring to string
		const std::string exprStr = utf8_to_wide_and_substring(keyword, 5);
		const std::pair<bool, std::string> result = calculator.evaluate(exprStr);

		if (result.first) {
			try {
				double numericResult = std::stod(result.second); // Convert string to double
				std::wcout << L"Result: " << numericResult << std::endl;
				swprintf(&resultStr[0], resultStr.capacity(), L"结果: %.15g", numericResult);
				return resultStr;
			} catch (const std::invalid_argument& e) {
				resultStr = L"Invalid result format: " + utf8_to_wide(e.what());
			} catch (const std::out_of_range& e) {
				resultStr = L"Result out of range: " + utf8_to_wide(e.what());
			}
		} else {
			resultStr = L"Error: " + utf8_to_wide(result.second);
		}
		return resultStr;
	}

	static void TestLatex() {
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

	// todo: 实现全局和 前缀
	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			if (isDetectReplace && input.size() > (startStr.size() + 2) && MyEndsWith2(input, L"=")) {
				swprintf(editTextBuf, 64, L"%.15g", EvaluateExpression(input.substr(0, input.size() - 1)));
				m_host->ChangeEditTextText(startStr + editTextBuf);
			} else {
				swprintf(editTextBuf, 64, L"结果: %.15g", EvaluateExpression(input));
				std::dynamic_pointer_cast<CalcAction>(allPluginActions[0])->subTitle = editTextBuf;
			}

			return allPluginActions;
		} else if (StartsWith(input, L"latex ")) {
			std::dynamic_pointer_cast<CalcAction>(allPluginActions[0])->subTitle = EvaluateLatex(input);
			return allPluginActions;
		}
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto exampleAction = std::dynamic_pointer_cast<CalcAction>(action);
		if (!exampleAction) return false;

		if (exampleAction->id == 1) {
			m_host->ShowMessage(L"示例插件", L"Hello World from Plugin!");
			return true;
		}
		return false;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new CalcPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}

// 定义静态成员变量
ImprovedLaTeXCalculator CalcPlugin::calculator;
