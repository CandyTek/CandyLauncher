#pragma once
#include "../Plugin.h"
#include <windows.h>
#include <memory>

#include "ExampleAction.hpp"

class ExamplePlugin : public IPlugin {
private:
	IPluginHost* m_host = nullptr;
	std::vector<std::shared_ptr<ActionBase>> allExampleActions;
	std::vector<std::shared_ptr<ActionBase>> dynamicExampleActions;

public:
	ExamplePlugin() = default;
	~ExamplePlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"ExamplePlugin";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.example.plugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"简单示例插件";
	}

	// 不要在初始化里加载Actions，由主程序调用 RefreshAllActions
	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void RefreshAllActions() override {
		if (!m_host) return;

		allExampleActions.clear();
		ExampleAction action1;
		action1.id = L"ExamplePlugin_hello";
		action1.title = L"Hello World";
		action1.subTitle = L"显示Hello World消息";
		action1.matchText = m_host->GetTheProcessedMatchingText(action1.title);
		allExampleActions.push_back(std::make_shared<ExampleAction>(action1));

		ExampleAction action2;
		action2.id = L"ExamplePlugin_time";
		action2.title = L"显示时间";
		action2.subTitle = L"显示当前系统时间";
		action2.matchText = m_host->GetTheProcessedMatchingText(action2.title);
		allExampleActions.push_back(std::make_shared<ExampleAction>(action2));

		ExampleAction action3;
		action3.id = L"ExamplePlugin_time_en";
		action3.title = L"Show Time";
		action3.subTitle = L"Show current system time";
		action3.matchText = m_host->GetTheProcessedMatchingText(action3.title);
		allExampleActions.push_back(std::make_shared<ExampleAction>(action3));
	}

	bool InterceptInputShowResultsDirectly(const std::wstring& input) override {
		return false;
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "example_key_1",
			"type": "expand",
			"title": "我的设置项",
			"subPage": "plugin",
			"children": [
				{
					"key": "example_key_2",
					"type": "expand",
					"title": "我的设置项2",
					"subPage": "plugin",
					"children": [
						{
							"key": "example_key_3",
							"title": "开关",
							"type": "bool",
							"subPage": "plugin",
							"defValue": true
						},
						{
							"key": "example_acknowledgements2",
							"title": "开源许可 & 致谢: \nCLion\n(For non-commercial use only)(https://www.jetbrains.com/clion/)\nHakeQuick\n(https://github.com/lzl1918/HakeQuick)\nrapidfuzz-cpp\n(https://github.com/rapidfuzz/rapidfuzz-cpp)\nnlohmann-json\n(https://github.com/nlohmann/json)\n",
							"type": "text",
							"subPage": "plugin",
							"defValue": ""
						}
					]
				}
			]
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		// auto it = settings_map.find("pref_pinyin_mode");
		// if (it != settings_map.end()) {
		// 	pinyinType = it->second.stringValue;
		// }
	}

	void Shutdown() override {
		if (m_host) {
			allExampleActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<ActionBase>> GetAllActions() override {
		if (!m_host) return {};
		std::vector<std::shared_ptr<ActionBase>> combinedActions = allExampleActions;
		combinedActions.insert(combinedActions.end(), dynamicExampleActions.begin(), dynamicExampleActions.end());

		return combinedActions;
	}

	void OnMainWindowShow(bool isShow) override {
	}

	void OnUserInput(const std::wstring& input) override {
		if (!m_host) return;
		dynamicExampleActions.clear();
		// 只在输入包含特定关键词时才添加动态actions，并且使用固定ID避免重复
		if (input.find(L"test") != std::wstring::npos) {
			ExampleAction action;
			action.id = L"ExamplePlugin_dynamic_test";
			action.title = L"测试功能";
			action.subTitle = L"执行测试功能 (输入包含test)";
			action.matchText = L"test";
			dynamicExampleActions.push_back(std::make_shared<ExampleAction>(action));
		} else if (input.find(L"calc") != std::wstring::npos) {
			ExampleAction action;
			action.id = L"ExamplePlugin_calculator";
			action.title = L"计算器";
			action.subTitle = L"打开系统计算器 (输入包含calc)";
			action.matchText = L"calc";
			dynamicExampleActions.push_back(std::make_shared<ExampleAction>(action));
		}
	}

	void OnActionExecute(std::shared_ptr<ActionBase>& action) override {
		if (!m_host) return;
		auto exampleAction = std::dynamic_pointer_cast<ExampleAction>(action);
		if (!exampleAction) return;

		if (exampleAction->id == L"ExamplePlugin_hello") {
			m_host->ShowMessage(L"示例插件", L"Hello World from Plugin!");
		} else if (exampleAction->id == L"ExamplePlugin_time") {
			SYSTEMTIME st;
			GetSystemTime(&st);
			wchar_t timeStr[256];
			wsprintf(timeStr, L"当前时间: %04d-%02d-%02d %02d:%02d:%02d UTC",
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			m_host->ShowMessage(L"时间", timeStr);
		} else if (exampleAction->id == L"ExamplePlugin_time_en") {
			SYSTEMTIME st;
			GetSystemTime(&st);
			wchar_t timeStr[256];
			wsprintf(timeStr, L"Current time: %04d-%02d-%02d %02d:%02d:%02d UTC",
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			m_host->ShowMessage(L"Time", timeStr);
		} else if (exampleAction->id == L"ExamplePlugin_dynamic_test") {
			m_host->ShowMessage(L"测试", L"这是动态生成的测试动作被执行了！");
		} else if (exampleAction->id == L"ExamplePlugin_calculator") {
			ShellExecuteA(nullptr, "open", "calc.exe", nullptr, nullptr, SW_SHOWNORMAL);
		}
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new ExamplePlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}

