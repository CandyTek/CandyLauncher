#include "../Plugin.hpp"
#include <memory>
#include <fstream>

#include "CherryTreeAction.hpp"
#include "CherryTreePluginData.hpp"
#include "CherryTreeUtil.hpp"
#include "../../util/StringUtil.hpp"

class CherryTreePlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"c ";

public:
	CherryTreePlugin() = default;
	~CherryTreePlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"CherryTree 笔记";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.cherrytreeplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"CherryTree 数据库索引";
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
			Loge(L"CherryTree Plugin", L"RefreshAllActions: m_host is null" );
			return;
		}

		std::wcout << L"[CherryTree Plugin] RefreshAllActions start" << std::endl;
		allPluginActions.clear();

		// 从配置中读取 CherryTree 数据库文件路径
		std::string db_path = m_host->GetSettingsMap().at("com.candytek.cherrytreeplugin.db_path").stringValue;
		std::wcout << L"[CherryTree Plugin] Database path: " << utf8_to_wide(db_path) << std::endl;

		// 检查文件是否存在
		std::ifstream file_check(db_path);
		if (!file_check.good()) {
			std::wcerr << L"[CherryTree Plugin] Database file does not exist: " << utf8_to_wide(db_path) << std::endl;
			return;
		}
		file_check.close();

		sqlite3* db = nullptr;
		int rc = sqlite3_open(db_path.c_str(), &db);
		if (rc == SQLITE_OK) {
			std::wcout << L"[CherryTree Plugin] Database opened successfully" << std::endl;
			// 调用 db_parse，传入 allPluginActions 引用
			db_parse(db, allPluginActions);
			sqlite3_close(db);
			std::wcout << L"[CherryTree Plugin] Loaded " << allPluginActions.size() << L" actions" << std::endl;
		} else {
			// 数据库打开失败
			std::wcerr << L"[CherryTree Plugin] Failed to open database: " << utf8_to_wide(db_path)
			          << L", error: " << sqlite3_errmsg(db) << std::endl;
			if (db) sqlite3_close(db);
		}
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.cherrytreeplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "c "
		},
		{
			"key": "com.candytek.cherrytreeplugin.db_path",
			"title": "CherryTree 数据库文件路径",
			"type": "string",
			"subPage": "plugin",
			"defValue": "E:\\GitHub\\CherrySnippet\\CherryCode.ctb"
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.cherrytreeplugin.start_str").stringValue);
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
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()),allPluginActions);
		}
		// 由插件系统管理
		return {};
	}
	

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<CherryTreeAction>(action);
		if (!action1) return false;

		// 获取数据库路径
		std::wstring db_path = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.cherrytreeplugin.db_path").stringValue);

		// 构造 CherryTree 命令行参数：cherrytree.exe dbfile --node=nodeId
		std::wstring cmdLine = L"\"" + db_path + L"\" --node=" + std::to_wstring(action1->nodeId);

		// 使用 ShellExecute 打开 CherryTree
		// 假设 cherrytree.exe 在系统 PATH 中或用户已安装
		HINSTANCE result = ShellExecute(
			nullptr,
			L"open",
			L"cherrytree.exe",
			cmdLine.c_str(),
			nullptr,
			SW_SHOW
		);

		if (reinterpret_cast<INT_PTR>(result) <= 32) {
			// 失败，尝试直接打开数据库文件
			std::wcerr << L"[CherryTree Plugin] Failed to open with cherrytree.exe, trying to open file directly" << std::endl;
			ShellExecute(nullptr, L"open", db_path.c_str(), nullptr, nullptr, SW_SHOW);
		}
		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new CherryTreePlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
