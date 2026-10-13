#include <chrono>
#include <filesystem>
#include <memory>
#include <utility>

#include "CherryTreeAction.hpp"
#include "CherryTreePluginData.hpp"
#include "CherryTreeUtil.hpp"
#include "../../util/BackgroundWorkTracker.hpp"
#include "../../util/ClipboardUtil.hpp"
#include "../../util/StringUtil.hpp"

class CherryTreePlugin : public candy::Plugin {
private:
	candy::ActionSet<CherryTreeAction> notes{0};
	std::wstring startStr = L"c ";
	// 粘贴线程要睡两次再发按键；Shutdown 时取消并 join，否则 DLL 卸载后线程醒来会跑进已释放的代码
	BackgroundWorkTracker pasteWorkers;
	// 影响索引的设置项变化后需要重新索引（主程序保存设置后不会自动刷新插件）
	// 设置了激活命令时，笔记只通过激活命令显示，不参与全局搜索
	uint32_t NoteSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	// 支持 CherryTree SQLite 数据库和多文件目录两种存储格式
	void LoadNotes(std::vector<std::shared_ptr<CherryTreeAction>>& items) const {
		const std::wstring dbPath = GetHost().GetString(L"com.candytek.cherrytreeplugin.db_path");
		const std::filesystem::path storagePath(dbPath);
		Logi(L"CherryTree", L"Storage path: ", storagePath.wstring());

		std::error_code pathError;
		if (std::filesystem::is_directory(storagePath, pathError)) {
			Logi(L"CherryTree", L"Multi-file directory opened successfully");
			folder_parse(storagePath, items);
			Logi(L"CherryTree", L"Loaded ", items.size(), L" actions");
			return;
		}

		if (!std::filesystem::is_regular_file(storagePath, pathError)) {
			Loge(L"CherryTree", L"Storage path does not exist: ", storagePath.wstring());
			return;
		}

		const std::string dbPathUtf8 = wide_to_utf8(dbPath);
		sqlite3* db = nullptr;
		int rc = sqlite3_open(dbPathUtf8.c_str(), &db);
		if (rc == SQLITE_OK) {
			Logi(L"CherryTree", L"Database opened successfully");
			// 调用 db_parse，传入 items 引用
			db_parse(db, items);
			sqlite3_close(db);
			Logi(L"CherryTree", L"Loaded ", items.size(), L" actions");
		} else {
			// 数据库打开失败
			Loge(L"CherryTree", L"Failed to open database: ", dbPathUtf8, L", error: ", sqlite3_errmsg(db));
			if (db) sqlite3_close(db);
		}
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		Logi(L"CherryTree", L"RefreshActions start");
		std::vector<std::shared_ptr<CherryTreeAction>> items;
		LoadNotes(items);
		notes.SetFlags(GetHost(), NoteSetFlags());
		notes.Publish(GetHost(), std::move(items));
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.cherrytreeplugin.start_str", L"c ");
		isMatchTextContent = host.GetBool(L"com.candytek.cherrytreeplugin.matchtext_content", false);
		notes.SetFlags(host, NoteSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
		// 设置保存后主程序会在工作线程上重新调用 RefreshActions，这里不用自己重建索引
	}

	void Shutdown() override {
		pasteWorkers.Shutdown();
		notes.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(notes.Id(), input.substr(startStr.size()));
		}
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = notes.Find(userData);
		if (!action1 || action1->text.empty()) return false;
		text = action1->text;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = notes.Find(userData);
		if (!action1) return false;

		std::wstring text = action1->text;

		// 插件正在卸载时 Run 返回 false，不再粘贴
		return pasteWorkers.Run([this, text = std::move(text)]() {
			if (!CopyTextToClipboard(nullptr, text)) {
				Loge(L"CherryTree Plugin", L"Failed to copy action text to clipboard");
				return;
			}

			// Give the clipboard time to lock/settle; cancelled by Shutdown
			if (!pasteWorkers.Wait(200)) return;

			// Release any physical Shift/Ctrl keys the user might still be holding down
			// to prevent modifier clashing
			INPUT releaseModifiers[2]{};
			releaseModifiers[0].type = INPUT_KEYBOARD;
			releaseModifiers[0].ki.wVk = VK_SHIFT;
			releaseModifiers[0].ki.dwFlags = KEYEVENTF_KEYUP;
			releaseModifiers[1].type = INPUT_KEYBOARD;
			releaseModifiers[1].ki.wVk = VK_CONTROL;
			releaseModifiers[1].ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(2, releaseModifiers, sizeof(INPUT));

			if (!pasteWorkers.Wait(50)) return;

			// Send Shift + Insert cleanly
			INPUT inputs[4]{};

			// 1. Shift Down
			inputs[0].type = INPUT_KEYBOARD;
			inputs[0].ki.wVk = VK_SHIFT;

			// 2. Insert Down (extended key flag added)
			inputs[1].type = INPUT_KEYBOARD;
			inputs[1].ki.wVk = VK_INSERT;
			inputs[1].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;

			// 3. Insert Up
			inputs[2].type = INPUT_KEYBOARD;
			inputs[2].ki.wVk = VK_INSERT;
			inputs[2].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;

			// 4. Shift Up
			inputs[3].type = INPUT_KEYBOARD;
			inputs[3].ki.wVk = VK_SHIFT;
			inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

			UINT sent = SendInput(4, inputs, sizeof(INPUT));
			if (sent < 4) {
				Loge(L"CherryTree", L"SendInput failed. Sent: ", sent);
			}
			keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
			keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
			keybd_event(VK_RSHIFT, 0, KEYEVENTF_KEYUP, 0);
		});
	}
};

// 读取数据库 / 目录只用到 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(CherryTreePlugin, L"CherryTree Notes", L"Index CherryTree databases and multi-file folders", L"1.2.0",
						L"com.candytek.cherrytreeplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
