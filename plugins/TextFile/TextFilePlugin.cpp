#include <memory>
#include <fstream>

#include "TextFileAction.hpp"
#include "TextFilePluginData.hpp"
#include "TextFileUtil.hpp"
#include "../../util/StringUtil.hpp"

class TextFilePlugin : public candy::Plugin {
private:
	// 0 号集合：索引到的文件，不参与全局搜索，只在激活命令下由插件自行按内容匹配
	candy::ActionSet<TextFileAction> files{0, 0u};
	// 1 号：本次输入的匹配结果（副标题为关键词上下文）及提示条目，集合 id 不能和 ActionSet 重复
	candy::TransientSet<TextFileAction> results{1};
	std::wstring startStr = L"text ";
	int64_t inputCount = 1;
	int64_t maxResults = 1000;
	// 影响索引的设置项变化后需要重新索引（主程序保存设置后不会自动刷新插件）
	// 实际生效的激活命令：留空时固定用 "text "，本插件从不参与全局搜索
	std::wstring TriggerStr() const {
		return startStr.empty() ? L"text " : startStr;
	}

	static std::shared_ptr<TextFileAction> MakeHint(const std::wstring& title, const std::wstring& subtitle) {
		auto hint = std::make_shared<TextFileAction>();
		hint->title = title;
		hint->subtitle = subtitle;
		return hint;
	}

	void ShowHint(candy::ResultSink& sink, const std::wstring& title, const std::wstring& subtitle) {
		sink.AddTransient(results, {MakeHint(title, subtitle)});
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		Logi(L"TextFile", L"RefreshActions start");

		// GetAllTextFile方法：
		// 解析 key com.candytek.textfileplugin.folder_list，获取 stringArr
		// 遍历每一条取出后面的 <扩展名>和前面的路径，路径使用 BaseTools 的 ExpandEnvironmentVariables 进行展开，然后判断文件夹是否存在
		// 文件夹存在则调用主程序的 TraverseFiles 遍历文件
		// 超过2mb的文本文件就跳过吧，尝试获取文本内容并构建 TextFileAction，内容填入 content，标题使用使用文件名称，副标题在 OnInterceptInput 里赋值为 找到的文本位置部分上下文内容
		auto items = GetAllTextFile();
		Logi(L"TextFile", L"Loaded ", items.size(), L" items");
		files.Publish(GetHost(), std::move(items));
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.textfileplugin.start_str", L"text ");
		inputCount = host.GetInt(L"com.candytek.textfileplugin.show_after_more_input", 1);
		maxResults = host.GetInt(L"com.candytek.textfileplugin.max_results", 1000);
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		host.SetTriggerKeywords({TriggerStr()});
		// 设置保存后主程序会在工作线程上重新调用 RefreshActions，这里不用自己重建索引
	}

	void Shutdown() override {
		files.ClearLocal();
		results.Begin();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		const std::wstring trigger = TriggerStr();
		if (!StartsWith(input, trigger)) {
			// 由插件系统管理
			return;
		}
		if (files.Size() == 0) {
			ShowHint(sink, L"未索引任何文件", L"请检查设置 > 本插件 > 文件夹路径是否正确");
			return;
		}

		// 单独显示本插件actions
		// 提取搜索关键词
		std::wstring searchText = MyTrim(input.substr(trigger.size()));
		if (searchText.size() <= static_cast<size_t>(inputCount)) {
			ShowHint(sink, L"键入更多字符以查询结果", L"");
			return;
		}

		if (searchText.empty()) {
			sink.AddAll(files.Id());
			return;
		}

		// 主程序低于 2.2 时没有 sink_add_descs，退化为由主程序按标题匹配
		if (!GetHost().Has(&CL_HostApi::sink_add_descs)) {
			sink.AddMatches(files.Id(), searchText);
			return;
		}

		// 自己实现文本内容匹配
		const std::wstring lowerSearch = MyToLower(searchText);
		std::vector<std::shared_ptr<TextFileAction>> matched;

		for (const auto& file : files.Items()) {
			// 在文本内容中查找
			if (file->content.find(lowerSearch) == std::wstring::npos) continue;

			// 结果条目只为本次输入生成，副标题显示关键词所在位置的上下文
			auto item = std::make_shared<TextFileAction>();
			item->title = file->title;
			item->subtitle = TextFileAction::ExtractContextLazy(file->content, lowerSearch);
			item->iconPath = file->iconPath;
			item->stableKey = file->stableKey;
			item->filePath = file->filePath;
			matched.push_back(std::move(item));
			if (static_cast<int64_t>(matched.size()) >= maxResults) {
				break;
			}
		}

		if (matched.empty()) {
			ShowHint(sink, L"未找到相关内容", L"");
			return;
		}
		sink.AddTransient(results, matched);
	}

	std::shared_ptr<TextFileAction> FindAction(const uint64_t userData) const {
		if (auto action = results.Find(userData)) return action;
		return files.Find(userData);
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = FindAction(userData);
		if (!action1 || action1->filePath.empty()) return false;
		text = action1->filePath;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = FindAction(userData);
		// 提示条目没有文件路径
		if (!action1 || action1->filePath.empty()) return false;

		// 调用系统打开该文件 ShellExecuteW
		try {
			const std::wstring& filePath = action1->filePath;

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
				Logi(L"TextFile", L"Successfully opened file: ", filePath);
				return true;
			} else {
				Loge(L"TextFile", L"Failed to open file: ", filePath);
				return false;
			}
		} catch (const std::exception& e) {
			Loge(L"TextFile", L"Exception in OnActionExecute: ", e.what());
			return false;
		} catch (...) {
			Loge(L"TextFile", L"Unknown exception in OnActionExecute");
			return false;
		}
	}
};

// 刷新只用到 traverse_files、设置读取等 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(TextFilePlugin, L"Text Files", L"Index text files in configured folders", L"1.0.0",
						L"com.candytek.textfileplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
