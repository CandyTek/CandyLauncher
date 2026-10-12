#include <memory>
#include <fstream>

#include "EverNoteAction.hpp"
#include "EverNotePluginData.hpp"
#include "EverNoteUtil.hpp"
#include "../../util/StringUtil.hpp"

class EverNotePlugin : public candy::Plugin {
private:
	candy::ActionSet<EverNoteAction> notes{0};
	std::wstring startStr = L"en ";
	// 影响索引的设置项变化后需要重新索引（主程序保存设置后不会自动刷新插件）
	// 设置了激活命令时，笔记只通过激活命令显示，不参与全局搜索
	uint32_t NoteSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	void LoadNotes(std::vector<std::shared_ptr<EverNoteAction>>& items) const {
		// 从配置中读取 Evernote 数据库文件路径
		const std::wstring dbPath = GetHost().GetString(L"com.candytek.evernoteplugin.db_path");
		Logi(L"EverNote", L"Database path: ", dbPath);

		// 检查文件是否存在
		std::ifstream file_check(dbPath);
		if (!file_check.good()) {
			Loge(L"EverNote", L"Database file does not exist: ", dbPath);
			return;
		}
		file_check.close();

		const std::string dbPathUtf8 = wide_to_utf8(dbPath);
		sqlite3* db = nullptr;
		int rc = sqlite3_open(dbPathUtf8.c_str(), &db);
		if (rc == SQLITE_OK) {
			Logi(L"EverNote", L"Database opened successfully");
			// 调用 db_parse，传入 items 引用
			db_parse(db, items);
			sqlite3_close(db);
			Logi(L"EverNote", L"Loaded ", items.size(), L" actions");
		} else {
			// 数据库打开失败
			Loge(L"EverNote", L"Failed to open database: ", dbPathUtf8, L", error: ", sqlite3_errmsg(db));
			if (db) sqlite3_close(db);
		}
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		Logi(L"EverNote", L"RefreshActions start");
		std::vector<std::shared_ptr<EverNoteAction>> items;
		LoadNotes(items);
		notes.SetFlags(GetHost(), NoteSetFlags());
		notes.Publish(GetHost(), std::move(items));
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.evernoteplugin.start_str", L"en ");
		notes.SetFlags(host, NoteSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
		// 设置保存后主程序会在工作线程上重新调用 RefreshActions，这里不用自己重建索引
	}

	void Shutdown() override {
		notes.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(notes.Id(), input.substr(startStr.size()));
		}
	}

	// Ctrl+C 复制笔记的应用内链接
	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = notes.Find(userData);
		if (!action1 || action1->noteId.empty()) return false;
		text = action1->GetNoteUrl();
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = notes.Find(userData);
		if (!action1) return false;

		const std::wstring url = action1->GetNoteUrl();

		Logi(L"EverNote", L"Opening note: ", action1->title);
		Logi(L"EverNote", L"URL: ", url);

		// 使用 ShellExecute 打开 Evernote URL
		HINSTANCE result = ShellExecute(
			nullptr,
			L"open",
			url.c_str(),
			nullptr,
			nullptr,
			SW_SHOW
		);

		if (reinterpret_cast<INT_PTR>(result) <= 32) {
			Loge(L"EverNote", L"Failed to open note, error code: ", reinterpret_cast<INT_PTR>(result));
			return false;
		}

		return true;
	}
};

// 读取数据库只用到 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(EverNotePlugin, L"Evernote Notes", L"Index the Evernote database", L"1.0.0",
						L"com.candytek.evernoteplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
