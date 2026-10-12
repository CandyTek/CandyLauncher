#include <memory>

#include "OneNotePluginData.hpp"
#include "FindOneNoteUtil.hpp"
#include "OneNoteAction.hpp"
#include "OneNoteSimple.hpp"  // New #import-based implementation
#include "OneNoteUtil.hpp"    // For ParseOneNoteHierarchyXML function
#include "../../util/StringUtil.hpp"

class OneNotePlugin : public candy::Plugin {
private:
	candy::ActionSet<OneNoteAction> allPluginActions{0};
	std::wstring startStr = L"one ";
	std::wstring iconPath;

	// 有激活命令时不参与全局搜索，只在 OnInterceptInput 里显示
	uint32_t OneNoteSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	// 通过 COM 读取全部页面及其内容，失败时返回空列表
	std::vector<std::shared_ptr<OneNoteAction>> LoadPages() {
		std::vector<std::shared_ptr<OneNoteAction>> pages;
		try {
			// 使用新的 #import 实现
			SimpleOneNoteHelper oneNote;

			if (!oneNote.IsInitialized()) {
				Logw(L"OneNote", L"OneNote is not available");
				return pages;
			}

			// 获取所有页面的 XML
			std::wstring xml = oneNote.GetHierarchy();

			if (xml.empty()) {
				Logw(L"OneNote", L"Failed to get OneNote hierarchy");
				return pages;
			}

			// 解析 XML
			int iconIndex = GetSysImageIndex(iconPath);
			pages = ParseOneNoteHierarchyXML(xml, iconPath, iconIndex, maxResults);

			// 获取页面内容并处理匹配文本
			Logi(L"OneNote", L"Fetching page contents...");
			int contentCount = 0;
			for (auto& oneNoteAction : pages) {
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

				// 刷新后主程序仍能认出同一个页面
				oneNoteAction->stableKey = oneNoteAction->pageId;

				// 设置匹配文本：标题 + 内容（插件自己处理，主程序原样使用）
				oneNoteAction->matchTextVerbatim = true;
				try {
					if (oneNoteAction->pageContent.length() < 500) {
						oneNoteAction->matchText = GetHost().ProcessMatchText(oneNoteAction->title + oneNoteAction->pageContent);
					} else {
						oneNoteAction->matchText = GetHost().ProcessMatchText(oneNoteAction->title) + oneNoteAction->pageContent;
					}
				} catch (...) {
					oneNoteAction->matchText = oneNoteAction->title + oneNoteAction->pageContent;
				}
			}
			Logi(L"OneNote", L"Successfully fetched content for ", contentCount, L" / ", pages.size(), L" pages");

			Logi(L"OneNote", L"Loaded ", pages.size(), L" OneNote pages");

		} catch (const std::exception& e) {
			Loge(L"OneNote", L"Exception in RefreshActions: ", e.what());
		} catch (...) {
			Loge(L"OneNote", L"Unknown exception in RefreshActions");
		}
		return pages;
	}

public:
	OneNotePlugin() = default;
	~OneNotePlugin() override = default;

	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		Logi(L"OneNote", L"RefreshActions start");

		if (const std::wstring path = GetHost().GetString(L"com.candytek.onenoteplugin.icon_path"); path.empty()) {
			iconPath = FindOneNotePathEnhanced();
			if (!iconPath.empty()) {
				Logi(L"OneNote", L"Found OneNote: ", iconPath);
			} else {
				Logw(L"OneNote", L"OneNote not found (desktop Office 2013/2016/2019 checks attempted).");
				iconPath = LR"(C:\Program Files\Microsoft Office\root\Office16\ONENOTE.EXE)";
			}
		} else {
			iconPath = path;
		}

		allPluginActions.SetFlags(GetHost(), OneNoteSetFlags());
		allPluginActions.Publish(GetHost(), LoadPages());
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.onenoteplugin.start_str", L"one ");
		iconPath = host.GetString(L"com.candytek.onenoteplugin.icon_path");
		maxResults = static_cast<int>(host.GetInt(L"com.candytek.onenoteplugin.max_results", 0));

		allPluginActions.SetFlags(host, OneNoteSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	void Shutdown() override {
		allPluginActions.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(allPluginActions.Id(), input.substr(startStr.size()));
		}
		// 否则由插件系统管理
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = allPluginActions.Find(userData);
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

CANDY_PLUGIN_EXPORT(OneNotePlugin, L"OneNote Notes", L"Index local OneNote notebooks", L"1.0.0", L"com.candytek.onenoteplugin",
					nullptr)
