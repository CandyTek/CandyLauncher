#include <windows.h>
#include <memory>

#include "ExampleAction.hpp"
#include "ExamplePluginData.hpp"
#include "../../util/BitmapUtil.hpp"

static const wchar_t* EXAMPLE_DEFAULT_SETTING_JSON = LR"(
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

// 演示 2.2 的全部机制：触发关键字、临时结果 (TransientSet)、异步查询 (OnQuery + PostResults)、
// 共享图标 (iconKey)、稳定标识 (stableKey)、设置项变更通知 (OnSettingChanged)。
class ExamplePlugin : public candy::Plugin {
private:
	// 0 号集合：固定 actions；1 号集合：根据输入动态生成的 actions，两者都参与全局搜索
	candy::ActionSet<ExampleAction> staticActions{0};
	candy::ActionSet<ExampleAction> dynamicActions{1};
	// 2 号：输入 "ex " 时直接显示的临时结果；3 号：异步查询结果。集合 id 不能和 ActionSet 重复
	candy::TransientSet<ExampleAction> interceptResults{2};
	candy::TransientSet<ExampleAction> queryResults{3};

	static std::shared_ptr<ExampleAction> MakeAction(const wchar_t* id, const wchar_t* title, const wchar_t* subtitle) {
		auto action = std::make_shared<ExampleAction>();
		action->id = id;
		action->title = title;
		action->subtitle = subtitle;
		// 稳定标识：刷新后主程序仍能认出同一个条目
		action->stableKey = id;
		return action;
	}

	std::shared_ptr<ExampleAction> FindAction(const uint64_t userData) const {
		if (auto action = staticActions.Find(userData)) return action;
		if (auto action = dynamicActions.Find(userData)) return action;
		if (auto action = interceptResults.Find(userData)) return action;
		return queryResults.Find(userData);
	}

public:
	// 不要在初始化里加载Actions，由主程序调用 RefreshActions
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void OnUserSettingsLoaded() override {
		// "ex " 开头的输入路由到本插件；CL_TRIGGER_ALSO_GLOBAL 让 OnUserInput 对其它输入也继续生效
		GetHost().SetTriggerKeywords({L"ex "}, CL_TRIGGER_ALSO_GLOBAL);
	}

	void OnSettingChanged(const std::wstring& key) override {
		GetHost().Log(L"setting changed: " + key);
	}

	void RefreshActions() override {
		// matchText 留空，由主程序根据 title 生成（小写 + 拼音）
		auto action1 = MakeAction(L"ExamplePlugin_hello", L"Hello World", L"显示Hello World消息");
		action1->iconBitmap = LoadShell32IconAsBitmap(15);

		// 两个条目共用一张位图：只有第一个带 iconBitmap，第二个只给 iconKey
		auto action2 = MakeAction(L"ExamplePlugin_time", L"显示时间", L"显示当前系统时间");
		action2->iconBitmap = LoadShell32IconAsBitmap(265);
		action2->iconKey = L"clock";

		auto action3 = MakeAction(L"ExamplePlugin_time_en", L"Show Time", L"Show current system time");
		action3->iconKey = L"clock";

		staticActions.Publish(GetHost(), {action1, action2, action3});
	}

	void Shutdown() override {
		staticActions.ClearLocal();
		dynamicActions.ClearLocal();
		m_host = nullptr;
	}

	void OnUserInput(const std::wstring& input) override {
		// 只在输入包含特定关键词时才添加动态actions
		if (input.find(L"test") != std::wstring::npos) {
			auto action = MakeAction(L"ExamplePlugin_dynamic_test", L"测试功能", L"执行测试功能 (输入包含test)");
			action->matchText = L"test";
			action->matchTextVerbatim = true;
			action->iconPath = LR"(c:\nothing.ico)";
			dynamicActions.Publish(GetHost(), {action});
		} else if (input.find(L"calc") != std::wstring::npos) {
			auto action = MakeAction(L"ExamplePlugin_calculator", L"计算器", L"打开系统计算器 (输入包含calc)");
			action->matchText = L"calc";
			action->matchTextVerbatim = true;
			action->iconPath = LR"(c:\Windows\System32\calc.exe)";
			dynamicActions.Publish(GetHost(), {action});
		} else if (dynamicActions.Size() > 0) {
			dynamicActions.Clear(GetHost());
		}
	}

	// "ex xxx"：接管结果列表，显示只为本次输入生成的条目（不进入任何集合，不做拼音处理）
	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (input.rfind(L"ex ", 0) != 0) return;
		const std::wstring arg = input.substr(3);
		auto echo = MakeAction(L"ExamplePlugin_echo", (L"Echo: " + arg).c_str(), L"临时结果，回车显示消息");
		sink.AddTransient(interceptResults, {echo});
	}

	// 异步查询：这里为了示例直接在调用线程里投递；真实插件会在工作线程完成后再 PostResults
	void OnQuery(const uint64_t queryId, const std::wstring& input, const std::wstring& arg) override {
		if (input.find(L"async") == std::wstring::npos) return;
		auto result = MakeAction(L"ExamplePlugin_async", L"异步结果", (L"query " + std::to_wstring(queryId)).c_str());
		queryResults.PostResults(GetHost(), queryId, {result}, true, true);
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto exampleAction = FindAction(userData);
		if (!exampleAction) return false;

		if (exampleAction->id == L"ExamplePlugin_hello") {
			GetHost().ShowMessage(L"示例插件", L"Hello World from Plugin!");
		} else if (exampleAction->id == L"ExamplePlugin_time") {
			SYSTEMTIME st;
			GetSystemTime(&st);
			wchar_t timeStr[256];
			wsprintf(timeStr, L"当前时间: %04d-%02d-%02d %02d:%02d:%02d UTC",
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			GetHost().ShowMessage(L"时间", timeStr);
		} else if (exampleAction->id == L"ExamplePlugin_time_en") {
			SYSTEMTIME st;
			GetSystemTime(&st);
			wchar_t timeStr[256];
			wsprintf(timeStr, L"Current time: %04d-%02d-%02d %02d:%02d:%02d UTC",
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			GetHost().ShowMessage(L"Time", timeStr);
		} else if (exampleAction->id == L"ExamplePlugin_dynamic_test") {
			GetHost().ShowMessage(L"测试", L"这是动态生成的测试动作被执行了！");
		} else if (exampleAction->id == L"ExamplePlugin_calculator") {
			ShellExecuteA(nullptr, "open", "calc.exe", nullptr, nullptr, SW_SHOWNORMAL);
		} else if (exampleAction->id == L"ExamplePlugin_echo" || exampleAction->id == L"ExamplePlugin_async") {
			GetHost().ShowMessage(L"示例插件", exampleAction->title);
		}
		return true;
	}
};

CANDY_PLUGIN_EXPORT(ExamplePlugin, L"ExamplePlugin", L"简单示例插件", L"1.0.0", L"com.example.plugin",
					EXAMPLE_DEFAULT_SETTING_JSON)
