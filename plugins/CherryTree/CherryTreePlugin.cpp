#include "../Plugin.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>
#include <utility>

#include "CherryTreeAction.hpp"
#include "CherryTreePluginData.hpp"
#include "CherryTreeUtil.hpp"
#include "../../util/ClipboardUtil.hpp"
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
        return L"1.2.0";
    }

    std::wstring GetPluginDescription() const override {
        return L"CherryTree 数据库及多文件目录索引";
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
            Loge(L"CherryTree Plugin", L"RefreshAllActions: m_host is null");
            return;
        }

        std::wcout << L"[CherryTree Plugin] RefreshAllActions start" << std::endl;
        allPluginActions.clear();

        // 支持 CherryTree SQLite 数据库和多文件目录两种存储格式
        std::string db_path = m_host->GetSettingsMap().at("com.candytek.cherrytreeplugin.db_path").stringValue;
        std::filesystem::path storagePath = std::filesystem::u8path(db_path);
        std::wcout << L"[CherryTree Plugin] Storage path: " << storagePath.wstring() << std::endl;

        std::error_code pathError;
        if (std::filesystem::is_directory(storagePath, pathError)) {
            std::wcout << L"[CherryTree Plugin] Multi-file directory opened successfully" << std::endl;
            folder_parse(storagePath, allPluginActions);
            std::wcout << L"[CherryTree Plugin] Loaded " << allPluginActions.size() << L" actions" << std::endl;
            return;
        }

        if (!std::filesystem::is_regular_file(storagePath, pathError)) {
            std::wcerr << L"[CherryTree Plugin] Storage path does not exist: " << storagePath.wstring() << std::endl;
            return;
        }

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
			"title": "CherryTree 数据库文件或多文件目录路径",
			"type": "string",
			"subPage": "plugin",
			"defValue": "E:\\GitHub\\CherrySnippet\\CherryCode"
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
        if (!startStr.empty() && StartsWith(input, startStr)) {
            // 单独显示本插件actions
            return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()), allPluginActions);
        }
        // 由插件系统管理
        return {};
    }


    bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
        if (!m_host) return false;
        auto action1 = std::dynamic_pointer_cast<CherryTreeAction>(action);
        if (!action1) return false;

        std::wstring text = action1->text;

        std::thread([text = std::move(text)]() {
            if (!CopyTextToClipboard(nullptr, text)) {
                std::wcerr << L"[CherryTree Plugin] Failed to copy action text to clipboard" << std::endl;
                return;
            }

            // Give the clipboard time to lock/settle
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

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

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
                std::wcerr << L"[CherryTree Plugin] SendInput failed. Sent: " << sent << std::endl;
            }
            keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_RSHIFT, 0, KEYEVENTF_KEYUP, 0);
        }).detach();
        // std::thread([text = std::move(text)]() {
        // 	if (!CopyTextToClipboard(nullptr, text)) {
        // 		std::wcerr << L"[CherryTree Plugin] Failed to copy action text to clipboard" << std::endl;
        // 		return;
        // 	}
        //
        // 	std::this_thread::sleep_for(std::chrono::milliseconds(200));
        //
        // 	INPUT inputs[4]{};
        // 	inputs[0].type = INPUT_KEYBOARD;
        // 	inputs[0].ki.wVk = VK_SHIFT;
        // 	inputs[1].type = INPUT_KEYBOARD;
        // 	inputs[1].ki.wVk = VK_INSERT;
        // 	inputs[2].type = INPUT_KEYBOARD;
        // 	inputs[2].ki.wVk = VK_INSERT;
        // 	inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        // 	inputs[3].type = INPUT_KEYBOARD;
        // 	inputs[3].ki.wVk = VK_SHIFT;
        // 	inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        // 	SendInput(4, inputs, sizeof(INPUT));
        //
        // 	// 强制弹起所有 Shift 虚拟键，避免模拟粘贴后修饰键卡住。
        // 	keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
        // 	keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0);
        // 	keybd_event(VK_RSHIFT, 0, KEYEVENTF_KEYUP, 0);
        // }).detach();
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
