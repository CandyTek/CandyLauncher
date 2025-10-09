#include "../Plugin.hpp"
#include <memory>
#include <fstream>

#include "TextFileAction.hpp"
#include "TextFilePluginData.hpp"
#include "TextFileUtil.hpp"
#include "../../util/StringUtil.hpp"

class TextFilePlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::vector<std::shared_ptr<BaseAction>> emptyResultActions;
	std::wstring startStr = L"text ";
	int64_t inputCount = 1;
	int64_t maxResults = 1000;

public:
	TextFilePlugin() = default;
	~TextFilePlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"文件夹文本";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.textfileplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"索引文件夹内的指定文本";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		auto empthAction = std::make_shared<TextFileAction>();
		emptyResultActions.push_back(empthAction);
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) {
			Loge(L"TextFilePlugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"TextFilePlugin", L"RefreshAllActions start");
		allPluginActions.clear();

		// 获取所有浏览器历史记录
		allPluginActions = GetAllTextFile();
		// GetAllTextFile方法：
		// 解析 key com.candytek.textfileplugin.folder_list，获取 stringArr
		// 遍历每一条取出后面的 <扩展名>和前面的路径，路径使用 BaseTools 的 ExpandEnvironmentVariables 进行展开，然后判断文件夹是否存在
		// 文件夹存在则调用 m_host->TraverseFilesForEverythingSDK 遍历文件
		// 超过2mb的文本文件就跳过吧，尝试获取文本内容并构建 TextFileAction，内容填入 matchText，标题使用使用文件名称，副标题动态更改，在InterceptInputShowResultsDirectly里赋值为 找到的文本位置部分上下文内容

		ConsolePrintln(L"TextFilePlugin", L"Loaded " + std::to_wstring(allPluginActions.size()) + L" items");
	}
	// TODO: 文件夹里响应 .gitignore 的设置
	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.textfileplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "text "
		},
		{
			"key": "com.candytek.textfileplugin.show_after_more_input",
			"title": "在输入更多字符前才进行搜索",
			"type": "long",
			"subPage": "plugin",
			"defValue": 1
		},
		{
			"key": "com.candytek.textfileplugin.folder_list",
			"type": "stringArr",
			"title": "文件夹路径<.txt|.md>",
			"subPage": "plugin",
			"defValue": "%USERPROFILE%\\Desktop<.txt|.md>"
		},
		{
			"key": "com.candytek.textfileplugin.max_results",
			"type": "long",
			"title": "最大结果数",
			"subPage": "plugin",
			"defValue": 1000
		},
		{
			"key": "com.candytek.textfileplugin.help",
			"type": "text",
			"title": "本插件索引指定文件夹下的文本文件，可用于下面这些用途：有道云笔记 (查看有道云笔记设置>笔记 本地文件)\\nCherryTree (笔记文件夹位置)\\n代码项目文件夹",
			"subPage": "plugin",
			"defValue": 1000
		}


	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.textfileplugin.start_str").stringValue);
		inputCount = (m_host->GetSettingsMap().at("com.candytek.textfileplugin.show_after_more_input").intValue);
		maxResults = static_cast<int>(m_host->GetSettingsMap().at("com.candytek.textfileplugin.max_results").intValue);
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		// 全局搜索不提供数据
		return {};
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if ((!startStr.empty() && MyStartsWith2(input, startStr)) || (startStr.empty() && MyStartsWith2(input, L"text "))) {
			if (allPluginActions.empty()) {
				std::dynamic_pointer_cast<TextFileAction>(emptyResultActions[0])->title = L"未索引任何文件";
				std::dynamic_pointer_cast<TextFileAction>(emptyResultActions[0])->subTitle = L"请检查设置 > 本插件 > 文件夹路径是否正确";
				return emptyResultActions;
			}

			// 单独显示本插件actions
			// 提取搜索关键词
			std::wstring searchText = startStr.empty() ? input.substr(5) : input.substr(startStr.size());
			searchText = MyTrim(searchText);
			if (searchText.size() <= (size_t)inputCount) {
				std::dynamic_pointer_cast<TextFileAction>(emptyResultActions[0])->title = L"键入更多字符以查询结果";
				std::dynamic_pointer_cast<TextFileAction>(emptyResultActions[0])->subTitle = L"";
				return emptyResultActions;
			}

			if (searchText.empty()) {
				return allPluginActions;
			}

			// 自己实现文本内容匹配
			std::vector<std::shared_ptr<BaseAction>> results;
			const std::wstring lowerSearch = MyToLower(searchText);
			int matchCount = 0;

			for (auto& action : allPluginActions) {
				auto textAction = std::dynamic_pointer_cast<TextFileAction>(action);
				if (!textAction) continue;

				// 在文本内容中查找
				if (textAction->matchText.find(lowerSearch) != std::wstring::npos) {
					// 标记需要提取上下文，并设置搜索关键词（用小写匹配）
					textAction->searchKeyword = lowerSearch;
					textAction->needExtractContext = true;
					// 不在这里提取上下文，而是在 getSubTitle() 被调用时才动态提取
					results.push_back(textAction);
					matchCount++;
				}
				if (matchCount >= maxResults) {
					break;
				}
			}

			if (results.empty()) {
				std::dynamic_pointer_cast<TextFileAction>(emptyResultActions[0])->title = L"未找到相关内容";
				std::dynamic_pointer_cast<TextFileAction>(emptyResultActions[0])->subTitle = L"";
				return emptyResultActions;
			}

			return results;
		}
		// 由插件系统管理
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<TextFileAction>(action);
		if (!action1) return false;

		// 调用系统打开该文件 ShellExecuteW
		try {
			// 从副标题中获取完整文件路径
			std::wstring filePath = action1->iconFilePath;

			// 使用 ShellExecuteW 打开文件
			HINSTANCE result = ShellExecuteW(
				nullptr,
				L"open",
				filePath.c_str(),
				nullptr,
				nullptr,
				SW_SHOWNORMAL
			);

			// ShellExecuteW 返回值大于 32 表示成功
			if (reinterpret_cast<INT_PTR>(result) > 32) {
				ConsolePrintln(L"TextFilePlugin", L"Successfully opened file: " + filePath);
				return true;
			} else {
				Loge(L"TextFilePlugin", L"Failed to open file: " + filePath, "");
				return false;
			}
		} catch (const std::exception& e) {
			Loge(L"TextFilePlugin", L"Exception in OnActionExecute", e.what());
			return false;
		} catch (...) {
			Loge(L"TextFilePlugin", L"Unknown exception in OnActionExecute", "");
			return false;
		}
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new TextFilePlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
