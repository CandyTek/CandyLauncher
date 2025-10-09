#include "../Plugin.hpp"
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "PluginAction.hpp"
#include "PluginData.hpp"
#include "PluginUtil.hpp"
#include "../../util/StringUtil.hpp"

inline std::vector<std::shared_ptr<BaseAction>> allPluginActions;

class ValueGeneratorPlugin : public IPlugin {
private:
	std::wstring startStr = L"value ";

public:
	ValueGeneratorPlugin() = default;
	~ValueGeneratorPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"值生成器";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.valuegeneratorplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"计算哈希并生成值";
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

		// UUID 生成器
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"UUID v1";
			action->subTitle = L"生成基于时间的 UUID";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 70;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"UUID v3";
			action->subTitle = L"生成基于 MD5 的命名空间 UUID (需要命名空间和名称)";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 70;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"UUID v4";
			action->subTitle = L"生成随机 UUID";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 70;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"UUID v5";
			action->subTitle = L"生成基于 SHA1 的命名空间 UUID (需要命名空间和名称)";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 70;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"UUID v7";
			action->subTitle = L"生成基于时间排序的 UUID";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 70;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}

		// 哈希计算
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"MD5";
			action->subTitle = L"计算字符串的 MD5 哈希值";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 155;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"SHA1";
			action->subTitle = L"计算字符串的 SHA1 哈希值";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 155;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"SHA256";
			action->subTitle = L"计算字符串的 SHA256 哈希值";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 155;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"SHA384";
			action->subTitle = L"计算字符串的 SHA384 哈希值";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 155;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"SHA512";
			action->subTitle = L"计算字符串的 SHA512 哈希值";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 155;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}

		// Base64 编码/解码
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"Base64 Encode";
			action->subTitle = L"将字符串编码为 Base64";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 265;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"Base64 Decode";
			action->subTitle = L"将 Base64 字符串解码";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 265;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}

		// URL 编码/解码
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"URL Encode";
			action->subTitle = L"将字符串编码为 URL 格式";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 14;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"URL Decode";
			action->subTitle = L"将 URL 编码的字符串解码";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 14;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}

		// Data Escape/Unescape
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"Data Escape";
			action->subTitle = L"转义数据字符串 (Uri.EscapeDataString)";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 14;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"Data Unescape";
			action->subTitle = L"解除数据字符串转义 (Uri.UnescapeDataString)";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 14;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}

		// Hex Escape/Unescape
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"Hex Escape";
			action->subTitle = L"将单个字符转换为十六进制转义 (Uri.HexEscape)";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 14;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
		{
			const auto action = std::make_shared<PluginAction>();
			action->title = L"Hex Unescape";
			action->subTitle = L"解除十六进制转义为字符 (Uri.HexUnescape)";
			action->iconFilePath = L"shell32.dll";
			action->iconFilePathIndex = 14;
			action->matchText = m_host->GetTheProcessedMatchingText(action->title);
			allPluginActions.push_back(action);
		}
	}


	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.valuegeneratorplugin.start_str",
			"title": "直接激活命令",
			"type": "string",
			"subPage": "plugin",
			"defValue": "value "
		}
	]
}

   )";
	}


	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.valuegeneratorplugin.start_str").stringValue);
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!m_host || !startStr.empty()) return {};
		return allPluginActions;
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (!startStr.empty() && MyStartsWith2(input, startStr)) {
			const std::wstring restInput = input.substr(startStr.size());

			// 检查是否是直接计算命令（格式：md5 abcd1234）
			std::vector<std::pair<std::wstring, std::wstring>> directCommands = {
				{L"md5 ", L"MD5"},
				{L"sha1 ", L"SHA1"},
				{L"sha256 ", L"SHA256"},
				{L"sha384 ", L"SHA384"},
				{L"sha512 ", L"SHA512"},
				{L"base64 ", L"Base64 Encode"},
				{L"base64d ", L"Base64 Decode"},
				{L"url ", L"URL Encode"},
				{L"urld ", L"URL Decode"},
				{L"esc:hex ", L"Hex Escape"},
				{L"esc:data ", L"Data Escape"},
				{L"uesc:hex ", L"Hex Unescape"},
				{L"uesc:data ", L"Data Unescape"}
			};

			for (const auto& [command, actionTitle] : directCommands) {
				if (MyStartsWith2(restInput, command)) {
					const std::wstring dataToProcess = restInput.substr(command.size());

					// 如果有数据，创建一个计算结果的action
					if (!dataToProcess.empty()) {
						// 查找对应的action
						for (const auto& action : allPluginActions) {
							const auto pluginAction = std::dynamic_pointer_cast<PluginAction>(action);
							if (pluginAction && pluginAction->title == actionTitle) {
								// 创建一个新的action显示计算结果
								auto resultAction = std::make_shared<PluginAction>();
								resultAction->title = pluginAction->title;
								resultAction->iconFilePath = pluginAction->iconFilePath;
								resultAction->iconFilePathIndex = pluginAction->iconFilePathIndex;

								// 执行计算
								std::shared_ptr<IComputeRequest> request;

								if (actionTitle == L"MD5") {
									request = std::make_shared<HashRequest>(HashAlgorithm::MD5, dataToProcess);
								} else if (actionTitle == L"SHA1") {
									request = std::make_shared<HashRequest>(HashAlgorithm::SHA1, dataToProcess);
								} else if (actionTitle == L"SHA256") {
									request = std::make_shared<HashRequest>(HashAlgorithm::SHA256, dataToProcess);
								} else if (actionTitle == L"SHA384") {
									request = std::make_shared<HashRequest>(HashAlgorithm::SHA384, dataToProcess);
								} else if (actionTitle == L"SHA512") {
									request = std::make_shared<HashRequest>(HashAlgorithm::SHA512, dataToProcess);
								} else if (actionTitle == L"Base64 Encode") {
									request = std::make_shared<Base64Request>(dataToProcess);
								} else if (actionTitle == L"Base64 Decode") {
									request = std::make_shared<Base64DecodeRequest>(dataToProcess);
								} else if (actionTitle == L"URL Encode") {
									request = std::make_shared<UrlEncodeRequest>(dataToProcess);
								} else if (actionTitle == L"URL Decode") {
									request = std::make_shared<UrlDecodeRequest>(dataToProcess);
								} else if (actionTitle == L"Data Escape") {
									request = std::make_shared<DataEscapeRequest>(dataToProcess);
								} else if (actionTitle == L"Data Unescape") {
									request = std::make_shared<DataUnescapeRequest>(dataToProcess);
								} else if (actionTitle == L"Hex Escape") {
									request = std::make_shared<HexEscapeRequest>(dataToProcess);
								} else if (actionTitle == L"Hex Unescape") {
									request = std::make_shared<HexUnescapeRequest>(dataToProcess);
								}

								if (request && request->Compute()) {
									resultAction->subTitle = request->ResultToString();
									// 存储数据到action的arg中，以便后续执行时使用
									resultAction->arg = dataToProcess;
								} else if (request) {
									resultAction->subTitle = L"错误: " + request->GetErrorMessage();
								}

								return {resultAction};
							}
						}
					}
				}
			}

			// 否则显示本插件actions
			return m_host->GetSuccessfullyMatchingTextActions(restInput, allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		const std::shared_ptr<PluginAction> workspaceAction = std::dynamic_pointer_cast<PluginAction>(action);
		if (!workspaceAction) return false;

		std::shared_ptr<IComputeRequest> request;
		const std::wstring title = workspaceAction->title;

		// 如果PluginAction中已经有arg，使用它；否则使用参数传入的arg
		std::wstring actualArg = workspaceAction->arg.empty() ? arg : workspaceAction->arg;

		// 根据 title 创建相应的请求
		if (title == L"UUID v1") {
			request = std::make_shared<GUIDRequest>(1);
		} else if (title == L"UUID v3") {
			// 需要解析命名空间和名称
			if (actualArg.empty()) {
				m_host->ShowMessage(L"提示", L"UUID v3 需要命名空间和名称参数\n格式: ns:<dns|url|oid|x500> 名称\n示例: ns:dns example.com");
				return true;
			}
			// 解析参数
			size_t spacePos = actualArg.find(L' ');
			if (spacePos == std::wstring::npos) {
				m_host->ShowMessage(L"错误", L"参数格式错误\n格式: ns:<dns|url|oid|x500> 名称");
				return true;
			}
			std::wstring namespacePart = actualArg.substr(0, spacePos);
			std::wstring namePart = actualArg.substr(spacePos + 1);
			request = std::make_shared<GUIDRequest>(3, namespacePart, namePart);
		} else if (title == L"UUID v4") {
			request = std::make_shared<GUIDRequest>(4);
		} else if (title == L"UUID v5") {
			// 需要解析命名空间和名称
			if (actualArg.empty()) {
				m_host->ShowMessage(L"提示", L"UUID v5 需要命名空间和名称参数\n格式: ns:<dns|url|oid|x500> 名称\n示例: ns:dns example.com");
				return true;
			}
			// 解析参数
			size_t spacePos = actualArg.find(L' ');
			if (spacePos == std::wstring::npos) {
				m_host->ShowMessage(L"错误", L"参数格式错误\n格式: ns:<dns|url|oid|x500> 名称");
				return true;
			}
			std::wstring namespacePart = actualArg.substr(0, spacePos);
			std::wstring namePart = actualArg.substr(spacePos + 1);
			request = std::make_shared<GUIDRequest>(5, namespacePart, namePart);
		} else if (title == L"UUID v7") {
			request = std::make_shared<GUIDRequest>(7);
		} else if (title == L"MD5") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HashRequest>(HashAlgorithm::MD5, actualArg);
		} else if (title == L"SHA1") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HashRequest>(HashAlgorithm::SHA1, actualArg);
		} else if (title == L"SHA256") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HashRequest>(HashAlgorithm::SHA256, actualArg);
		} else if (title == L"SHA384") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HashRequest>(HashAlgorithm::SHA384, actualArg);
		} else if (title == L"SHA512") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HashRequest>(HashAlgorithm::SHA512, actualArg);
		} else if (title == L"Base64 Encode") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<Base64Request>(actualArg);
		} else if (title == L"Base64 Decode") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<Base64DecodeRequest>(actualArg);
		} else if (title == L"URL Encode") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<UrlEncodeRequest>(actualArg);
		} else if (title == L"URL Decode") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<UrlDecodeRequest>(actualArg);
		} else if (title == L"Data Escape") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<DataEscapeRequest>(actualArg);
		} else if (title == L"Data Unescape") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<DataUnescapeRequest>(actualArg);
		} else if (title == L"Hex Escape") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HexEscapeRequest>(actualArg);
		} else if (title == L"Hex Unescape") {
			if (actualArg.empty()) {
				return false;
			}
			request = std::make_shared<HexUnescapeRequest>(actualArg);
		} else {
			return false;
		}

		// 执行计算
		if (request && request->Compute()) {
			const std::wstring result = request->ResultToString();

			// 复制结果到剪贴板
			if (OpenClipboard(nullptr)) {
				EmptyClipboard();
				const HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (result.size() + 1) * sizeof(wchar_t));
				if (hMem) {
					memcpy(GlobalLock(hMem), result.c_str(), (result.size() + 1) * sizeof(wchar_t));
					GlobalUnlock(hMem);
					SetClipboardData(CF_UNICODETEXT, hMem);
				}
				CloseClipboard();
			}

			m_host->ShowMessage(L"成功", L"结果已复制到剪贴板:\n" + result);
		} else if (request) {
			m_host->ShowMessage(L"错误", request->GetErrorMessage());
		}

		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new ValueGeneratorPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
