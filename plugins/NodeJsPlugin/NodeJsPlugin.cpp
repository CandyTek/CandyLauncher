#include "../Plugin.hpp"
#include <windows.h>
#include <memory>
#include <future>
#include <chrono>

#include "NodeJsAction.hpp"
#include "NodeJsBridge.hpp"
#include "NodeJsScriptManager.hpp"
#include "NodeJsPluginData.hpp"
#include "../../util/BitmapUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "../../util/FileUtil.hpp"


class NodeJsPlugin : public IPlugin {
    
private:
    IPluginHost* m_host = nullptr;
    uint16_t m_pluginId = 0;
    std::shared_ptr<NodeJsBridge> m_bridge;
    std::shared_ptr<NodeJsScriptManager> m_scriptManager;
    std::wstring m_scriptsDir;
    std::wstring m_nodeExePath = L"node.exe";
    std::atomic<uint64_t> m_querySeq{0};
    std::pair<std::wstring,std::vector<std::shared_ptr<BaseAction>>> lastResultActions;
    std::wstring lastResultId;

    std::vector<std::shared_ptr<BaseAction>> ParseActionsFromJson(const std::string& jsPluginId, const nlohmann::json& dataArray) {
        std::vector<std::shared_ptr<BaseAction>> results;
        if (!dataArray.is_array()) return results;

        std::wstring jsPluginIdW = utf8_to_wide(jsPluginId);
        auto script = m_scriptManager->GetScriptById(jsPluginIdW);
        // json解析，3000项占40ms，nodejs插件自己的查询耗时才是大头
        for (const auto& item : dataArray) {
            auto action = std::make_shared<NodeJsAction>();
            action->jsPluginId = jsPluginIdW;
            action->pluginId = m_pluginId;

            if (item.contains("Title")) action->title = utf8_to_wide(item["Title"]);

            if (item.contains("SubTitle")) action->subTitle = utf8_to_wide(item["SubTitle"]);
            else if (item.contains("Subtitle")) action->subTitle = utf8_to_wide(item["Subtitle"]);

            if (item.contains("ActionData")) action->actionData = item["ActionData"];
            else if (item.contains("JsonRPCAction")) action->actionData = item["JsonRPCAction"];

            if (item.contains("IcoPath")) {
                std::wstring icoPath = utf8_to_wide(item["IcoPath"]);

                if (icoPath.find(L":\\") == std::wstring::npos &&
                    icoPath.find(L'/') != 0) {
                    if (script) {
                        action->iconFilePath = script->GetDir() + L"\\" + icoPath;
                    }
                } else {
                    action->iconFilePath = icoPath;
                }
            }
            results.push_back(action);
        }
        return results;
    }

    void OnJsNotification(const std::string& method, const nlohmann::json& params) {
        if (method == "showResults") {
            if (params.contains("pluginId") && params["pluginId"].is_string() &&
                params.contains("results") && params["results"].is_array()) {
                
                std::string jsPluginId = params["pluginId"];
                auto actions = ParseActionsFromJson(jsPluginId, params["results"]);
                
                if (!actions.empty() && m_host) {
                    // For delayed refresh, we usually want to show results if they are still relevant.
                    // Since notifications don't have a query seq, we just push them.
                    // The host usually handles if it should display them based on focus/input.
                    lastResultActions =std::make_pair( utf8_to_wide(jsPluginId),actions);
                    // TODO: Should ensure this is called on the UI thread if ShowResultsDerectly is not thread-safe
                    m_host->ShowResultsDerectly(actions);
                }
            }
        } else if (method == "changeQuery") {
            if (params.contains("query") && params["query"].is_string()) {
                std::wstring query = utf8_to_wide(params["query"]);
                if (m_host) {
                    // TODO: Should ensure this is called on the UI thread
                    m_host->ChangeEditTextText(query);
                }
            }
        }
    }

public:
    NodeJsPlugin() = default;

    ~NodeJsPlugin() override {
        if (m_bridge) {
            m_bridge->Stop();
        }
    }

    std::wstring GetPluginName() const override {
        return L"NodeJsPlugin";
    }

    std::wstring GetPluginPackageName() const override {
        return L"com.gemini.nodejsplugin";
    }

    std::wstring GetPluginVersion() const override {
        return L"1.0.0";
    }

    std::wstring GetPluginDescription() const override {
        return L"Node.js Plugin Bridge";
    }

    bool Initialize(IPluginHost* host) override {
        m_host = host;
        m_scriptManager = std::make_unique<NodeJsScriptManager>();

        std::wstring exeDir = GetExecutableFolder();
        m_scriptsDir = exeDir + L"\\plugins\\NodeJsPlugin\\scripts";

        m_bridge = std::make_shared<NodeJsBridge>();
        m_bridge->SetNotificationHandler([this](const std::string& method, const nlohmann::json& params) {
            OnJsNotification(method, params);
        });

        std::wstring hostScriptPath = exeDir + L"\\plugins\\NodeJsPlugin\\node-host.js";

        if (m_bridge->Start(m_nodeExePath, hostScriptPath)) {
            m_scriptManager->ScanPlugins(m_scriptsDir);

            for (auto& script : m_scriptManager->GetAllScripts()) {
                nlohmann::json params;
                params["pluginDir"] = wide_to_utf8(script->GetDir());
                params["pluginId"] = wide_to_utf8(script->GetId());

                m_bridge->CallAsync("init", params);
            }
            return true;
        }

        return false;
    }

    void OnPluginIdChange(const uint16_t pluginId) override {
        m_pluginId = pluginId;
    }

    void RefreshAllActions() override {
    }

    // std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
    //     if (!m_host) return {};
    //     return allPluginActions;
    // }

    void Shutdown() override {
        if (m_bridge) {
            m_bridge->Stop();
        }
        m_host = nullptr;
    }

    void OnMainWindowShow(bool isShow) override {
        if (!isShow) {
            ConsolePrintln(L"清理");
            // lastResultActions.clear();
        }
    }

    bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
        auto jsAction = std::dynamic_pointer_cast<NodeJsAction>(action);
        if (!jsAction) return false;

        nlohmann::json params;
        params["pluginId"] = wide_to_utf8(jsAction->jsPluginId);
        params["actionData"] = jsAction->actionData;

        m_bridge->CallAsync("execute", params);
        return true;
    }

    std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
        if (!m_bridge) return {};

        std::vector<std::pair<std::wstring, std::future<nlohmann::json>>> futures;
        auto node_js_scripts = this->m_scriptManager->GetAllScripts();
        std::wstring lastResultIdTemp = lastResultId;
        std::wstring currectId;
        for (auto& script : node_js_scripts) {
            bool matched = false;
            std::wstring matchKw;
            for (const auto& kw : script->GetTriggerKeywords()) {
                if (StartsWithIgnoreCase(input, kw) || kw == L"*") {
                    matched = true;
                    matchKw = kw;
                    break;
                }
            }

            if (!matched) continue;

            nlohmann::json params;
            params["query"] = wide_to_utf8(input);
            params["pluginId"] = wide_to_utf8(script->GetId());

            futures.emplace_back(
                script->GetId(),
                m_bridge->CallAsync("query", params)
            );
            
        }

        if (futures.empty()) {
            return {};
        }
        currectId = futures.at(0).first;
        MethodTimerStart(L"nodejsplugin query");
        ConsolePrintln(L"查询"+std::to_wstring(futures.size()));
        // 拷贝必要数据，避免后台线程引用悬空
        auto bridge = m_bridge;
        auto pluginId = m_pluginId;
        const auto& queryInput = input;

        auto futuresPtr = std::make_shared<std::vector<std::pair<std::wstring, std::future<nlohmann::json>>>>(
            std::move(futures)
        );

        auto seq = ++m_querySeq;
        std::thread([this, futuresPtr , queryInput, seq]() {
            std::vector<std::shared_ptr<BaseAction>> finalResults;

            for (auto& f : *futuresPtr) {
                try {
                    // 后台线程里可以等，不会阻塞 UI/input
                    auto response = f.second.get();
                    if (seq != m_querySeq.load()) {
                        ConsolePrintln(L"loss");
                        return;
                    }
                    if (!response.contains("data") || !response["data"].is_array()) {
                        std::string s = response.dump();
                        ConsolePrintln(L"[ERROR] plugin=" + f.first + L" 响应格式错误: " + std::wstring(s.begin(), s.end()));
                        continue;
                    }
                    
                    auto actions = ParseActionsFromJson(wide_to_utf8(f.first), response["data"]);
                    finalResults.insert(finalResults.end(), actions.begin(), actions.end());
                    
                } catch (const std::exception& e) {
                    ConsolePrintln(
                        L"[ERROR] plugin=" + f.first +
                        L" query 异步异常: " +
                        std::wstring(e.what(), e.what() + strlen(e.what()))
                    );
                }
            }

            if (!finalResults.empty()) {
                if (this->m_host->GetEditTextText() == queryInput) {
                    MethodTimerEnd(L"nodejsplugin query");
                    // 关键：异步完成后刷新结果
                    lastResultActions =std::make_pair( futuresPtr->at(0).first,finalResults);
                    m_host->ShowResultsDerectly(finalResults);
                }
            }
        }).detach();

            ConsolePrintln(L"上次插件id"+lastResultIdTemp);
            ConsolePrintln(L"本次插件id"+currectId);
        if (currectId == lastResultActions.first && !lastResultActions.second.empty()) {
            // 缓存上次结果，这样查询就不会闪烁了
            ConsolePrintln(L"返回上次结果插件"+lastResultId);
            return lastResultActions.second;
        } else {
            lastResultId = currectId;
            // 立即返回占位等待action
            std::vector<std::shared_ptr<BaseAction>> initialResults;
            auto action = std::make_shared<NodeJsAction>();
            action->title = L"查询中";
            action->subTitle = L"";
            action->pluginId = m_pluginId;

            if (const auto it = pluginIconMap.find(currectId); it != pluginIconMap.end()) {
                HBITMAP src = it->second.get();
                if (src) {
                    action->iconBitmap = CopyHBitmap(src);
                }
                action->iconFilePathIndex = -1;
                ConsolePrintln(L"找到插件icon");
            }

            initialResults.push_back(action);
            return initialResults;
        }
    }
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
    return new NodeJsPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
    delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
    return 1;
}
