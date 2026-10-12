#include <windows.h>
#include <memory>
#include <vector>

#include "RegistryAction.hpp"
#include "RegistryPluginData.hpp"
#include "RegistryUtil.hpp"
#include "../../util/StringUtil.hpp"

class RegistryPlugin : public candy::Plugin {
private:
	// 注册表条目随输入动态生成，只为本次输入存在，不进入任何集合
	candy::TransientSet<RegistryAction> results{0};
	std::wstring startStr = L": ";
	std::wstring iconPath;

public:
	bool Initialize() override {
		m_host = &GetHost();
		iconPath = LR"(C:\Windows\regedit.exe)";
		return true;
	}

	void RefreshActions() override {
		// 注册表插件不需要在启动时预加载所有内容
		// 只在用户输入时动态搜索
		Logi(L"Registry", L"Registry plugin initialized");
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.registryplugin.start_str", L": ");
		maxResults = static_cast<int>(host.GetInt(L"com.candytek.registryplugin.max_results", 2000));
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	void Shutdown() override {
		results.Begin();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (startStr.empty() || !StartsWith(input, startStr)) return;
		MethodTimerStart(L"Registry OnInterceptInput");

		// 移除前缀
		const std::wstring query = input.substr(startStr.size());
		sink.AddTransient(results, BuildResults(query));

		MethodTimerEnd(L"Registry OnInterceptInput");
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = results.Find(userData);
		if (!action1) return false;

		try {
			// 打开注册表编辑器并跳转到该键
			if (!OpenRegistryEditor(action1->keyPath)) {
				Logw(L"Registry", L"Failed to open registry editor");
				return false;
			}
		} catch (...) {
			return false;
		}

		return true;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = results.Find(userData);
		if (!action1 || action1->keyPath.empty()) return false;
		text = action1->valueName.empty() ? action1->keyPath : action1->keyPath + L"\\" + action1->valueName;
		return true;
	}

private:
	// 根据输入生成本次要显示的条目
	std::vector<std::shared_ptr<RegistryAction>> BuildResults(const std::wstring& query) {
		// 如果查询为空，显示所有根键
		if (query.empty()) {
			return ConvertRegistryEntriesToActions(GetAllBaseKeys());
		}

		// 解析查询
		std::wstring keyPart, valuePart;
		const bool searchForValue = ParseQuery(query, keyPart, valuePart);

		// 获取根键
		auto [baseKeys, subKey] = GetRegistryBaseKey(keyPart);

		if (baseKeys.empty()) {
			// 没有找到匹配的根键，显示所有根键
			return ConvertRegistryEntriesToActions(GetAllBaseKeys());
		}

		if (baseKeys.size() == 1) {
			// 只找到一个根键，搜索子键
			auto entries = SearchForSubKey(baseKeys[0], subKey);

			// 如果搜索值名称且只找到一个键
			if (searchForValue && entries.size() == 1 && entries[0].key) {
				auto valueEntries = GetValuesFromKey(entries[0].key, entries[0].keyPath, valuePart);
				return ConvertRegistryEntriesToActions(valueEntries, true);
			}

			return ConvertRegistryEntriesToActions(entries);
		}

		// 找到多个根键，显示它们
		std::vector<RegistryEntry> entries;
		for (HKEY key : baseKeys) {
			entries.push_back(RegistryEntry(key, GetBaseKeyName(key), 0, 0));
		}
		return ConvertRegistryEntriesToActions(entries);
	}

	// 将 RegistryEntry 转换为 RegistryAction
	std::vector<std::shared_ptr<RegistryAction>> ConvertRegistryEntriesToActions(
		const std::vector<RegistryEntry>& entries, bool isValue = false) {
		std::vector<std::shared_ptr<RegistryAction>> result;
		Logi(L"Registry", L"列表数量：", entries.size());

		for (const auto& entry : entries) {
			auto action = std::make_shared<RegistryAction>();

			action->keyPath = entry.keyPath;
			action->valueName = entry.valueName;
			action->valueData = entry.valueData;
			action->valueType = entry.valueType;
			action->hasException = entry.hasException;
			action->exceptionMsg = entry.exceptionMsg;
			action->subKeyCount = entry.subKeyCount;
			action->valueCount = entry.valueCount;

			// 所有条目共用 regedit 图标，由主程序延迟解析
			action->iconPath = iconPath;
			action->stableKey = entry.GetValueNameWithKey();

			if (isValue) {
				// 这是一个注册表值
				std::wstring displayName = entry.valueName.empty() ? L"(Default)" : entry.valueName;
				action->title = TruncateText(displayName, 100, true);

				// 子标题显示类型和值
				std::wstring valuePreview = GetValueString(entry.key, entry.valueName,
															entry.valueType == L"REG_SZ"
																? REG_SZ
																: entry.valueType == L"REG_DWORD"
																? REG_DWORD
																: entry.valueType == L"REG_QWORD"
																? REG_QWORD
																: entry.valueType == L"REG_BINARY"
																? REG_BINARY
																: entry.valueType == L"REG_EXPAND_SZ"
																? REG_EXPAND_SZ
																: entry.valueType == L"REG_MULTI_SZ"
																? REG_MULTI_SZ
																: REG_NONE, 50);

				action->subtitle = L"Type: " + entry.valueType + L" - Value: " + valuePreview;
			} else if (entry.hasException) {
				// 有异常的键
				action->title = TruncateText(entry.keyPath, 100, true);
				action->subtitle = TruncateText(entry.exceptionMsg, 150, false);
			} else {
				// 普通注册表键
				action->title = TruncateText(entry.keyPath, 100, true);
				action->subtitle = L"子键: " + std::to_wstring(entry.subKeyCount) +
					L" - 值: " + std::to_wstring(entry.valueCount);
			}

			// 临时条目不参与匹配，不需要匹配文本
			result.push_back(action);
		}
		return result;
	}
};

CANDY_PLUGIN_EXPORT(RegistryPlugin, L"Registry Navigator", L"Navigate the Windows registry", L"1.1.0", L"com.candytek.registryplugin",
					nullptr)
