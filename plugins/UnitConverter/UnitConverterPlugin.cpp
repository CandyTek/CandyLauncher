#include "../Plugin.hpp"
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "UnitConverterAction.hpp"
#include "UnitConverterPluginData.hpp"
#include "UnitConverterUtil.hpp"
#include "../../util/StringUtil.hpp"

inline std::vector<std::shared_ptr<BaseAction>> allPluginActions;

class UnitConverterPlugin : public IPlugin {
private:
	std::wstring startStr = L"%%";
	bool isNeedShow = false;

public:
	UnitConverterPlugin() = default;
	~UnitConverterPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"单位转换";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.unitconverterplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"单位转换插件，支持长度、质量、温度、面积、体积、速度等多种单位的转换";
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
			return;
		}

		allPluginActions.clear();

		// 创建一个默认的 action 用于显示转换结果
		const auto action = std::make_shared<UnitConverterAction>();
		action->title = L"单位转换结果";
		action->subTitle = L"输入格式: 数值 单位 in/to 单位 (例如: 10 ft in cm)";
		action->iconFilePath = L"shell32.dll";
		action->iconFilePathIndex = 70; // 使用一个合适的图标
		allPluginActions.push_back(action);
	}


	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.unitconverterplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "%%"
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.unitconverterplugin.start_str").stringValue);
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	// 格式化数字显示
	static std::wstring FormatNumber(double value) {
		std::wostringstream woss;

		// 检查是否是整数
		if (std::floor(value) == value && std::abs(value) < 1e15) {
			woss << std::fixed << std::setprecision(0) << value;
		} else {
			// 使用科学记数法或固定小数点
			woss << std::setprecision(14) << std::noshowpoint << value;
			std::wstring result = woss.str();

			// 移除尾随的零
			if (result.find(L'.') != std::wstring::npos) {
				result.erase(result.find_last_not_of(L'0') + 1, std::wstring::npos);
				if (result.back() == L'.') {
					result.pop_back();
				}
			}
			return result;
		}

		return woss.str();
	}


	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		// 如果设置了启动前缀，检查是否以前缀开头
		std::wstring queryInput;
		isNeedShow = false;
		if (!startStr.empty()) {
			if (!StartsWith(input, startStr)) {
				return {};
			}
			queryInput = input.substr(startStr.size());
		} else {
			queryInput = input;
		}
		if (allPluginActions.empty()) {
			return {};
		}
		const auto action = std::dynamic_pointer_cast<UnitConverterAction>(allPluginActions[0]);


		// 尝试解析输入
		const auto model = InputInterpreter::Parse(queryInput);
		if (!model) {
			if (!startStr.empty()) {
				action->title = L"空";
				action->subTitle = L"类型: ";
				return allPluginActions;
			}
			return {};
		}

		// 执行转换
		const auto results = UnitHandler::Convert(*model);

		if (results.empty()) {
			if (!startStr.empty()) {
				action->title = L"空";
				action->subTitle = L"类型: ";
				return allPluginActions;
			}
			return {};
		}

		const auto& result = results[0];

		// 标题显示转换结果
		action->title = FormatNumber(result.ConvertedValue) + L" " +
			StringToWString(result.UnitName);

		// 副标题显示单位类型
		action->subTitle = L"类型: " + StringToWString(result.QuantityName);

		if (!startStr.empty()) {
			return allPluginActions;
		} else {
			isNeedShow = true;
			return {};
		}
	}

	// todo:
	// std::vector<std::shared_ptr<BaseAction>> GetAlwaysDisplayedActions() override{
	// 	if (m_host && isNeedShow) {
	// 		return allPluginActions;
	// 	}else {
	// 		return {};
	// 	}
	// }
	
	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto converterAction = std::dynamic_pointer_cast<UnitConverterAction>(action);
		if (!converterAction) return false;

		// 执行时将结果复制到剪贴板
		if (OpenClipboard(nullptr)) {
			EmptyClipboard();

			// 只复制数字部分（不包含单位）
			std::wstring title = converterAction->title;
			size_t spacePos = title.find(L' ');
			std::wstring numberPart = (spacePos != std::wstring::npos) ? title.substr(0, spacePos) : title;

			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (numberPart.size() + 1) * sizeof(wchar_t));
			if (hMem) {
				memcpy(GlobalLock(hMem), numberPart.c_str(), (numberPart.size() + 1) * sizeof(wchar_t));
				GlobalUnlock(hMem);
				SetClipboardData(CF_UNICODETEXT, hMem);
			}
			CloseClipboard();

			m_host->ShowMessage(L"单位转换", L"结果已复制到剪贴板");
			return true;
		}
		return false;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new UnitConverterPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
