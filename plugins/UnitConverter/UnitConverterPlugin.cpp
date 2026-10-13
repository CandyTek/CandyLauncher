#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "UnitConverterAction.hpp"
#include "UnitConverterPluginData.hpp"
#include "UnitConverterUtil.hpp"
#include "../../util/ClipboardUtil.hpp"
#include "../../util/StringUtil.hpp"

inline constexpr wchar_t kUnitConverterStartStrKey[] = L"com.candytek.unitconverterplugin.start_str";

class UnitConverterPlugin : public candy::Plugin {
private:
	// 0 号：带激活命令时每次按键生成的转换结果，不进入任何集合
	candy::TransientSet<UnitConverterAction> results{0};
	// 1 号：不带激活命令时通过异步查询追加到正常搜索结果后面的转换结果
	candy::TransientSet<UnitConverterAction> queryResults{1};
	std::wstring startStr = L"%%";

	// 转换结果项；noResult 时显示占位
	static std::shared_ptr<UnitConverterAction> MakeAction() {
		auto action = std::make_shared<UnitConverterAction>();
		action->iconPath = L"shell32.dll";
		action->iconIndex = 70; // 使用一个合适的图标
		action->stableKey = L"UnitConverterPlugin_result";
		return action;
	}

	static std::shared_ptr<UnitConverterAction> MakeEmptyAction() {
		auto action = MakeAction();
		action->title = L"空";
		action->subtitle = L"类型: ";
		return action;
	}

	// 解析并转换输入，无法转换时返回 nullptr
	static std::shared_ptr<UnitConverterAction> Convert(const std::wstring& queryInput) {
		// 尝试解析输入
		const auto model = InputInterpreter::Parse(queryInput);
		if (!model) return nullptr;

		// 执行转换
		const auto results = UnitHandler::Convert(*model);
		if (results.empty()) return nullptr;

		const auto& result = results[0];
		auto action = MakeAction();
		// 标题显示转换结果
		action->copyText = FormatNumber(result.ConvertedValue);
		action->title = action->copyText + L" " + StringToWString(result.UnitName);
		// 副标题显示单位类型
		action->subtitle = L"类型: " + StringToWString(result.QuantityName);
		return action;
	}

	void ApplyTriggerKeywords() const {
		if (startStr.empty()) GetHost().SetTriggerKeywords({});
		else GetHost().SetTriggerKeywords({startStr});
	}

public:
	UnitConverterPlugin() = default;
	~UnitConverterPlugin() override = default;

	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void OnUserSettingsLoaded() override {
		startStr = GetHost().GetString(kUnitConverterStartStrKey, L"%%");
		ApplyTriggerKeywords();
	}

	void OnSettingChanged(const std::wstring& key) override {
		if (key == kUnitConverterStartStrKey) {
			startStr = GetHost().GetString(kUnitConverterStartStrKey, L"%%");
			ApplyTriggerKeywords();
		}
	}

	void Shutdown() override {
		results.Begin();
		queryResults.Begin();
		m_host = nullptr;
	}

	// 设置了激活命令时接管结果列表；留空时由 OnQuery 把结果追加到正常搜索结果后面
	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (startStr.empty() || !StartsWith(input, startStr)) return;
		auto action = Convert(input.substr(startStr.size()));
		if (!action) action = MakeEmptyAction();
		sink.AddTransient(results, {action});
	}

	// 不带激活命令时，能转换的输入把结果追加到当前列表，不影响正常搜索
	void OnQuery(const uint64_t queryId, const std::wstring& input, const std::wstring& arg) override {
		if (!startStr.empty()) return;
		const auto action = Convert(input);
		if (!action) return;
		queryResults.PostResults(GetHost(), queryId, {action}, true, true);
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		auto action = results.Find(userData);
		if (!action) action = queryResults.Find(userData);
		if (!action || action->copyText.empty()) return false;
		text = action->copyText;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		auto converterAction = results.Find(userData);
		if (!converterAction) converterAction = queryResults.Find(userData);
		if (!converterAction) return false;

		// 执行时只复制数字部分（不包含单位）到剪贴板
		const std::wstring& numberPart = converterAction->copyText;
		// 主程序低于 2.1 时没有 set_clipboard_text，退回自行复制
		if (!GetHost().SetClipboardText(numberPart) && !CopyTextToClipboard(nullptr, numberPart)) {
			return false;
		}
		GetHost().ShowMessage(Tr(L"plugin.name", L"Unit Converter"), L"结果已复制到剪贴板");
		return true;
	}
};

CANDY_PLUGIN_EXPORT(UnitConverterPlugin, L"Unit Converter", L"Convert length, mass, temperature, area, volume, speed and other units",
					L"1.0.0", L"com.candytek.unitconverterplugin", nullptr)
