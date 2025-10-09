#include "../Plugin.hpp"
#include <memory>
#include <regex>
#include <searchapi.h>

#include "WindowsSearchAction.hpp"
#include "WindowsSearchPluginData.hpp"
#include "WindowsSearchUtil.hpp"
#include "../../util/StringUtil.hpp"

// 外部声明 CLSID 和 IID
extern const CLSID CLSID_CSearchManager;
extern const IID IID_ISearchManager;

class WindowsSearchPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"?";
	std::wstring reservedStringPattern = LR"(^[\/\\\$\%]+$|^.*[<>].*$)";

public:
	WindowsSearchPlugin() = default;
	~WindowsSearchPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"Windows 索引搜索";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.windowssearchplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"使用 Windows 索引服务搜索文件和文件夹";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) {
			Loge(L"WindowsSearch Plugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"WindowsSearch Plugin", L"RefreshAllActions start");
		allPluginActions.clear();

		// Windows Search 插件不需要预加载数据，搜索是实时的
		// 但我们可以测试一下 COM 是否能正常工作
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		bool comInitialized = SUCCEEDED(hr);
		if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
			comInitialized = false;
		}

		ISearchManager* testManager = nullptr;
		hr = CoCreateInstance(CLSID_CSearchManager, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_ISearchManager,
							reinterpret_cast<void**>(&testManager));

		if (SUCCEEDED(hr) && testManager) {
			ConsolePrintln(L"WindowsSearch Plugin", L"Windows Search Manager test: SUCCESS");
			testManager->Release();
		} else {
			wchar_t msg[256];
			swprintf_s(msg, 256, L"Windows Search Manager test: FAILED (HRESULT: 0x%08X)", hr);
			Loge(L"WindowsSearch Plugin", msg);
		}

		if (comInitialized) CoUninitialize();

		ConsolePrintln(L"WindowsSearch Plugin", L"Ready for dynamic searching");
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.windowssearchplugin.start_str",
			"title": "直接激活命令（默认 '?'）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "?"
		},
		{
			"key": "com.candytek.windowssearchplugin.max_results",
			"type": "long",
			"title": "最大搜索结果数",
			"subPage": "plugin",
			"defValue": 30
		},
		{
			"key": "com.candytek.windowssearchplugin.display_hidden_files",
			"type": "bool",
			"title": "显示隐藏文件",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.windowssearchplugin.excluded_patterns",
			"type": "stringArr",
			"title": "排除模式（每行一个，支持通配符）",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.windowssearchplugin.start_str").stringValue);
		maxSearchCount = static_cast<int>(m_host->GetSettingsMap().at("com.candytek.windowssearchplugin.max_results").intValue);
		displayHiddenFiles = m_host->GetSettingsMap().at("com.candytek.windowssearchplugin.display_hidden_files").boolValue;

		// 读取排除模式
		excludedPatterns.clear();
		auto excludedPatternsArr = m_host->GetSettingsMap().at("com.candytek.windowssearchplugin.excluded_patterns").stringArr;
		for (const auto& pattern : excludedPatternsArr) {
			std::wstring wpattern = utf8_to_wide(pattern);
			if (!wpattern.empty()) {
				excludedPatterns.push_back(wpattern);
			}
		}
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		// Windows Search 插件不提供预加载的 actions
		return {};
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (startStr.empty() || !MyStartsWith2(input, startStr)) {
			return {};
		}

		// 获取搜索关键词（去除前缀）
		std::wstring searchQuery = input.substr(startStr.size());

		// 去除前后空格
		searchQuery = MyTrim(searchQuery);

		// 如果搜索关键词为空，返回提示
		if (searchQuery.empty()) {
			return {};
		}

		// 检查是否包含保留字符
		try {
			std::wregex regexPattern(reservedStringPattern);
			if (std::regex_match(searchQuery, regexPattern)) {
				ConsolePrintln(L"WindowsSearch", L"Query contains reserved characters, skipping search");
				return {};
			}
		} catch (...) {
			// 正则表达式错误，继续搜索
		}

		// 执行搜索
		MethodTimerStart();
		auto results = WindowsSearchAPI::Search(searchQuery, maxSearchCount, displayHiddenFiles, L"*", excludedPatterns);
		MethodTimerEnd(L"WindowsSearch Query");

		return results;
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<WindowsSearchAction>(action);
		if (!action1) return false;

		try {
			// 检查是否是打开设置的 action
			if (action1->path == L"OpenSettingsAction") {
				// 打开 Windows 设置 -> 搜索 -> Windows 搜索
				// 使用 ms-settings URI scheme
				ShellExecuteW(NULL, L"open", L"ms-settings:cortana-windowssearch", NULL, NULL, SW_SHOWNORMAL);
				return true;
			}

			// 打开文件或文件夹
			ShellExecuteW(NULL, L"open", action1->path.c_str(), NULL, NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}

		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new WindowsSearchPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
