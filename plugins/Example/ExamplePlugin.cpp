#include <windows.h>
#include <memory>

#include "ExampleAction.hpp"
#include "ExamplePluginData.hpp"
#include "../../util/BitmapUtil.hpp"

// 演示 2.2 的全部机制：触发关键字、临时结果 (TransientSet)、异步查询 (OnQuery + PostResults)、
// 共享图标 (iconKey)、稳定标识 (stableKey)、设置项变更通知 (OnSettingChanged)。
// 2.3 本地化：所有界面文本通过 Tr(key, 默认文本) 取得，翻译在清单 ExamplePlugin.plugin.json 的 "strings" 里
// （元数据、默认设置和各语言字符串都在清单里，构建时复制到 DLL 旁边），代码里的默认文本只在没有任何翻译时使用。
class ExamplePlugin : public candy::Plugin {
private:
	// 0 号集合：固定 actions；1 号集合：根据输入动态生成的 actions，两者都参与全局搜索
	candy::ActionSet<ExampleAction> staticActions{0};
	candy::ActionSet<ExampleAction> dynamicActions{1};
	// 2 号：输入 "ex " 时直接显示的临时结果；3 号：异步查询结果。集合 id 不能和 ActionSet 重复
	candy::TransientSet<ExampleAction> interceptResults{2};
	candy::TransientSet<ExampleAction> queryResults{3};

	static std::shared_ptr<ExampleAction> MakeAction(const wchar_t* id, std::wstring title, std::wstring subtitle) {
		auto action = std::make_shared<ExampleAction>();
		action->id = id;
		action->title = std::move(title);
		action->subtitle = std::move(subtitle);
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

	static std::wstring UtcNow() {
		SYSTEMTIME st;
		GetSystemTime(&st);
		wchar_t buffer[64];
		wsprintf(buffer, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		return buffer;
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

	// 语言切换后主程序会再调用 RefreshActions，集合里的标题自动换语言；这里只演示回调本身
	void OnLanguageChanged(const std::wstring& language) override {
		GetHost().Log(L"language changed: " + language + L", ui language is " + GetHost().GetUiLanguage());
	}

	void RefreshActions() override {
		// matchText 留空，由主程序根据 title 生成（小写 + 拼音）
		auto action1 = MakeAction(L"ExamplePlugin_hello", Tr(L"action.hello.title", L"Hello World"),
								Tr(L"action.hello.subtitle", L"Show a Hello World message"));
		action1->iconBitmap = LoadShell32IconAsBitmap(15);

		// 两个条目共用一张位图：只有第一个带 iconBitmap，第二个只给 iconKey
		auto action2 = MakeAction(L"ExamplePlugin_time", Tr(L"action.time.title", L"Show Time"),
								Tr(L"action.time.subtitle", L"Show the current system time"));
		action2->iconBitmap = LoadShell32IconAsBitmap(265);
		action2->iconKey = L"clock";

		auto action3 = MakeAction(L"ExamplePlugin_date", Tr(L"action.date.title", L"Show Date"),
								Tr(L"action.date.subtitle", L"Show today's date"));
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
			auto action = MakeAction(L"ExamplePlugin_dynamic_test", Tr(L"action.test.title", L"Test feature"),
									Tr(L"action.test.subtitle", L"Run the test feature (input contains \"test\")"));
			action->matchText = L"test";
			action->matchTextVerbatim = true;
			action->iconPath = LR"(c:\nothing.ico)";
			dynamicActions.Publish(GetHost(), {action});
		} else if (input.find(L"calc") != std::wstring::npos) {
			auto action = MakeAction(L"ExamplePlugin_calculator", Tr(L"action.calc.title", L"Calculator"),
									Tr(L"action.calc.subtitle", L"Open the system calculator (input contains \"calc\")"));
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
		auto echo = MakeAction(L"ExamplePlugin_echo", L"Echo: " + arg,
								Tr(L"action.echo.subtitle", L"Transient result, Enter shows a message"));
		sink.AddTransient(interceptResults, {echo});
	}

	// 异步查询：这里为了示例直接在调用线程里投递；真实插件会在工作线程完成后再 PostResults
	void OnQuery(const uint64_t queryId, const std::wstring& input, const std::wstring& arg) override {
		if (input.find(L"async") == std::wstring::npos) return;
		auto result = MakeAction(L"ExamplePlugin_async", Tr(L"action.async.title", L"Async result"),
								L"query " + std::to_wstring(queryId));
		queryResults.PostResults(GetHost(), queryId, {result}, true, true);
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto exampleAction = FindAction(userData);
		if (!exampleAction) return false;

		const std::wstring pluginTitle = Tr(L"msg.title", L"Example Plugin");
		if (exampleAction->id == L"ExamplePlugin_hello") {
			GetHost().ShowMessage(pluginTitle, Tr(L"msg.hello", L"Hello World from Plugin!"));
		} else if (exampleAction->id == L"ExamplePlugin_time") {
			// 带占位符的文本：翻译可以调整 {0} 的位置
			GetHost().ShowMessage(Tr(L"msg.time.title", L"Time"),
								candy::Format(Tr(L"msg.time.text", L"Current time: {0} UTC"), {UtcNow()}));
		} else if (exampleAction->id == L"ExamplePlugin_date") {
			GetHost().ShowMessage(Tr(L"msg.date.title", L"Date"),
								candy::Format(Tr(L"msg.date.text", L"Today is {0} (UTC)"), {UtcNow().substr(0, 10)}));
		} else if (exampleAction->id == L"ExamplePlugin_dynamic_test") {
			GetHost().ShowMessage(Tr(L"msg.test.title", L"Test"),
								Tr(L"msg.test.text", L"The dynamically generated test action ran!"));
		} else if (exampleAction->id == L"ExamplePlugin_calculator") {
			ShellExecuteA(nullptr, "open", "calc.exe", nullptr, nullptr, SW_SHOWNORMAL);
		} else if (exampleAction->id == L"ExamplePlugin_echo" || exampleAction->id == L"ExamplePlugin_async") {
			GetHost().ShowMessage(pluginTitle, exampleAction->title);
		}
		return true;
	}
};

// 名称 / 描述是没有清单时的默认文本；清单 strings 里的 plugin.name / plugin.description 按界面语言覆盖，
// 默认设置项也在清单里（default_setting_json），所以这里传 nullptr
CANDY_PLUGIN_EXPORT(ExamplePlugin, L"ExamplePlugin", L"Simple example plugin", L"1.0.0", L"com.example.plugin",
					nullptr)
