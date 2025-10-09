#include "../Plugin.hpp"
#include <memory>

#include "OneNoteAction.hpp"
#include "OneNotePluginData.hpp"
#include "OneNoteSimple.hpp"  // New #import-based implementation
#include "OneNoteUtil.hpp"    // For ParseOneNoteHierarchyXML function
#include "../../util/StringUtil.hpp"

class OneNotePlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"one ";
	std::wstring iconPath = LR"(C:\Program Files\Microsoft Office\root\Office16\ONENOTE.EXE)";

public:
	OneNotePlugin() = default;
	~OneNotePlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"OneNote笔记";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.onenoteplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"索引本地OneNote笔记";
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
			Loge(L"OneNote Plugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"OneNote Plugin", L"RefreshAllActions start");
		allPluginActions.clear();

		try {
			// 使用新的 #import 实现
			SimpleOneNoteHelper oneNote;

			if (!oneNote.IsInitialized()) {
				ConsolePrintln(L"OneNote Plugin", L"OneNote is not available");
				return;
			}

			// 获取所有页面的 XML
			std::wstring xml = oneNote.GetHierarchy();

			if (xml.empty()) {
				ConsolePrintln(L"OneNote Plugin", L"Failed to get OneNote hierarchy");
				return;
			}

			// 解析 XML
			int iconIndex = GetSysImageIndex(iconPath);
			allPluginActions = ParseOneNoteHierarchyXML(xml, iconPath, iconIndex, maxResults);

			// 获取页面内容并处理匹配文本
			ConsolePrintln(L"OneNote Plugin", L"Fetching page contents...");
			int contentCount = 0;
			for (auto& action : allPluginActions) {
				auto oneNoteAction = std::dynamic_pointer_cast<OneNoteAction>(action);
				if (oneNoteAction) {
					// 获取页面内容
					std::wstring pageXml = oneNote.GetPageContent(oneNoteAction->pageId);
					if (!pageXml.empty()) {
						oneNoteAction->pageContent = ExtractTextFromPageXML(pageXml);
						// 限制内容长度，避免过长
						// if (oneNoteAction->pageContent.length() > 1000) {
						// 	oneNoteAction->pageContent = oneNoteAction->pageContent.substr(0, 1000);
						// }
						if (!oneNoteAction->pageContent.empty()) {
							contentCount++;
						}
					}

					// 设置匹配文本：标题 + 内容
					try {
						if (oneNoteAction->pageContent.length()<500) {
							action->matchText = m_host->GetTheProcessedMatchingText(action->getTitle() + oneNoteAction->pageContent);
						}else {
							action->matchText = m_host->GetTheProcessedMatchingText(action->getTitle()) + oneNoteAction->pageContent;
						}
					} catch (...) {
						action->matchText = action->getTitle() + oneNoteAction->pageContent;
					}
				}
			}
			ConsolePrintln(L"OneNote Plugin", L"Successfully fetched content for " +
				std::to_wstring(contentCount) + L" / " + std::to_wstring(allPluginActions.size()) + L" pages");

			ConsolePrintln(L"OneNote Plugin", L"Loaded " + std::to_wstring(allPluginActions.size()) + L" OneNote pages");

		} catch (const std::exception& e) {
			Loge(L"OneNote Plugin", L"Exception in RefreshAllActions: ", e.what());
		} catch (...) {
			Loge(L"OneNote Plugin", L"Unknown exception in RefreshAllActions");
		}
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.onenoteplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "one "
		},
		{
			"key": "com.candytek.onenoteplugin.icon_path",
			"type": "string",
			"title": "OneNote图标路径",
			"subPage": "plugin",
			"defValue": "C:\\Program Files\\Microsoft Office\\root\\Office16\\ONENOTE.EXE"
		},
		{
			"key": "com.candytek.onenoteplugin.max_results",
			"type": "long",
			"title": "最大笔记数量（0为不限制）",
			"subPage": "plugin",
			"defValue": 0
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.onenoteplugin.start_str").stringValue);
		iconPath = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.onenoteplugin.icon_path").stringValue);
		maxResults = static_cast<int>(m_host->GetSettingsMap().at("com.candytek.onenoteplugin.max_results").intValue);
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
			// 单独显示本插件actions
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()), allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<OneNoteAction>(action);
		if (!action1) return false;

		try {
			// 使用新的实现打开 OneNote 页面
			SimpleOneNoteHelper oneNote;

			if (!oneNote.IsInitialized()) {
				return false;
			}

			bool success = oneNote.NavigateToPage(action1->pageId);

			if (success) {
				// 显示 OneNote 窗口
				HWND hwnd = FindWindowW(L"Framework::CFrame", nullptr);
				if (hwnd) {
					if (IsIconic(hwnd)) {
						ShowWindow(hwnd, SW_RESTORE);
					}
					SetForegroundWindow(hwnd);
				}
			}

			return success;
		} catch (...) {
			return false;
		}
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new OneNotePlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
