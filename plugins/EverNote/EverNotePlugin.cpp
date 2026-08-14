#include "../Plugin.hpp"
#include <memory>
#include <fstream>

#include "EverNoteAction.hpp"
#include "EverNotePluginData.hpp"
#include "EverNoteUtil.hpp"
#include "../../util/StringUtil.hpp"

class EverNotePlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"en ";

public:
	EverNotePlugin() = default;
	~EverNotePlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"EverNote 笔记";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.evernoteplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"EverNote 数据库索引";
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
			Loge(L"EverNote", L"RefreshAllActions: m_host is null");
			return;
		}

		Logi(L"EverNote", L"RefreshAllActions start");
		allPluginActions.clear();

		// 从配置中读取 Evernote 数据库文件路径
		std::string db_path = m_host->GetSettingsMap().at("com.candytek.evernoteplugin.db_path").stringValue;
		Logi(L"EverNote", L"Database path: ", db_path);

		// 检查文件是否存在
		std::ifstream file_check(db_path);
		if (!file_check.good()) {
			Loge(L"EverNote", L"Database file does not exist: ", db_path);
			return;
		}
		file_check.close();

		sqlite3* db = nullptr;
		int rc = sqlite3_open(db_path.c_str(), &db);
		if (rc == SQLITE_OK) {
			Logi(L"EverNote", L"Database opened successfully");
			// 调用 db_parse，传入 allPluginActions 引用
			db_parse(db, allPluginActions);
			sqlite3_close(db);
			Logi(L"EverNote", L"Loaded ", allPluginActions.size(), L" actions");
		} else {
			// 数据库打开失败
			Loge(L"EverNote", L"Failed to open database: ", db_path, L", error: ", sqlite3_errmsg(db));
			if (db) sqlite3_close(db);
		}
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.evernoteplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "en "
		},
		{
			"key": "com.candytek.evernoteplugin.db_path",
			"title": "Evernote 数据库文件路径",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.evernoteplugin.db_path",
			"title": "数据库文件路径位置需要自己查找，在用户的Evernote文件夹内，比如\n C:\\Users\\用户名\\AppData\\Roaming\\Evernote\\conduit-storage\\https%3A%2F%2Fwww.evernote.com\\UDB-User12345+RemoteGraph.sql",
			"type": "text",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.evernoteplugin.start_str").stringValue);
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
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()),allPluginActions);
		}
		// 由插件系统管理
		return {};
	}
	

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<EverNoteAction>(action);
		if (!action1) return false;

		// 构造 Evernote In-App Note Link:
		// evernote:///view/[userId]/[shardId]/[noteGuid]/[noteGuid]/
		// 注意：noteGuid 需要重复两次
		std::wstring url = L"evernote:///view/" +
		                   action1->userId + L"/" +
		                   action1->shardId + L"/" +
		                   action1->noteId + L"/" +
		                   action1->noteId + L"/";

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

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new EverNotePlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
