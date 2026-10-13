#include <windows.h>
#include <algorithm>
#include <memory>
#include <future>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "NodeJsAction.hpp"
#include "NodeJsBridge.hpp"
#include "NodeJsScriptManager.hpp"
#include "NodeJsPluginData.hpp"
#include "../../util/BitmapUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "../../util/FileUtil.hpp"

namespace {
// 工作线程 / 读取线程把需要在 UI 线程执行的事情投递到一个 message-only 窗口：
// show_action_set 只能在 UI 线程调用，set_edit_text 也统一在 UI 线程执行
constexpr UINT WM_NODEJS_SHOW_RESULTS = WM_APP + 1; // wParam = queryId 低 32 位，lParam = 高 32 位
constexpr UINT WM_NODEJS_CHANGE_QUERY = WM_APP + 2;
const wchar_t* const NODEJS_MSG_WINDOW_CLASS = L"CandyNodeJsPluginMsgWindow";

bool IsImageIconPath(const std::wstring& path) {
    return EndsWithAnyIgnoreCase(path, {L".ico", L".png", L".jpeg", L".jpg", L".bmp", L".tiff", L".gif", L".svg"});
}
}

class NodeJsPlugin : public candy::Plugin {

private:
    // 0 号集合：关键字脚本的查询结果，不参与全局搜索，查询完成后用 ShowActionSet 整表替换列表；
    // 拦截输入时若仍是同一个脚本，直接用这里的上次结果填充列表避免闪烁
    candy::ActionSet<NodeJsAction> m_results{0, 0u};
    // 1 号：拦截输入时的 "查询中" 占位；2 号：通配符（*）脚本通过 PostResults 追加到当前列表的结果
    candy::TransientSet<NodeJsAction> m_placeholder{1};
    candy::TransientSet<NodeJsAction> m_appended{2};

    std::shared_ptr<NodeJsBridge> m_bridge;
    std::shared_ptr<NodeJsScriptManager> m_scriptManager;
    std::wstring m_scriptsDir;
    std::wstring m_nodeExePath = L"node.exe";
    HWND m_msgHwnd = nullptr;
    HINSTANCE m_moduleInstance = nullptr;

    std::mutex m_stateMutex;
    uint64_t m_currentQueryId = 0;      // 最近一次 OnQuery 的 id
    bool m_currentQuerySent = false;    // 该 id 是否真的向 node 发出了查询
    bool m_currentTakeOver = false;     // 本次输入是否由拦截接管了列表（有关键字脚本匹配）
    std::wstring m_lastResultScriptId;  // m_results 里的结果来自哪个脚本
    std::wstring m_pendingEditText;     // changeQuery 通知要写入输入框的文本
    std::vector<std::future<void>> m_workers;

    // 结果图标缓存：图片路径 -> 位图，条目只拿副本；同一次发布里相同路径通过 iconKey 共享
    std::mutex m_iconMutex;
    std::unordered_map<std::wstring, UniqueHBitmap> m_iconCache;

    struct MatchedScript {
        std::shared_ptr<NodeJsScript> script;
        bool wildcard = false;
    };

    std::vector<MatchedScript> MatchScripts(const std::wstring& input) const {
        std::vector<MatchedScript> matched;
        for (const auto& script : m_scriptManager->GetAllScripts()) {
            for (const auto& kw : script->GetTriggerKeywords()) {
                if (kw == L"*") {
                    matched.push_back({script, true});
                    break;
                }
                if (StartsWithIgnoreCase(input, kw)) {
                    matched.push_back({script, false});
                    break;
                }
            }
        }
        return matched;
    }

    // 有关键字脚本匹配时整个列表交给本插件（通配符脚本只追加结果，不接管列表）
    static const MatchedScript* FirstKeywordScript(const std::vector<MatchedScript>& matched) {
        for (const auto& m : matched) {
            if (!m.wildcard) return &m;
        }
        return nullptr;
    }

    // 任意线程；png/svg 等图片只加载一次
    HBITMAP GetCachedIcon(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(m_iconMutex);
        const auto it = m_iconCache.find(path);
        if (it != m_iconCache.end()) return it->second.get();
        HBITMAP bitmap = nullptr;
        if (EndsWithIgnoreCase(path, L".svg")) {
            bitmap = GetHost().LoadSvgBitmap(path, 48, 48);
        } else {
            bitmap = LoadPngAsHBITMAP(path.c_str(), 48, 48);
        }
        m_iconCache.emplace(path, UniqueHBitmap(bitmap));
        return bitmap;
    }

    std::vector<std::shared_ptr<NodeJsAction>> ParseActionsFromJson(const std::string& jsPluginId, const nlohmann::json& dataArray) {
        std::vector<std::shared_ptr<NodeJsAction>> results;
        if (!dataArray.is_array()) return results;

        std::wstring jsPluginIdW = utf8_to_wide(jsPluginId);
        auto script = m_scriptManager->GetScriptById(jsPluginIdW);
        // 同一批结果里相同图片路径只有第一个条目带位图，其余靠 iconKey 共享
        std::unordered_set<std::wstring> seenIconKeys;
        // json解析，3000项占40ms，nodejs插件自己的查询耗时才是大头
        for (const auto& item : dataArray) {
            auto action = std::make_shared<NodeJsAction>();
            action->jsPluginId = jsPluginIdW;

            if (item.contains("Title")) action->title = utf8_to_wide(item["Title"]);

            if (item.contains("SubTitle")) action->subtitle = utf8_to_wide(item["SubTitle"]);
            else if (item.contains("Subtitle")) action->subtitle = utf8_to_wide(item["Subtitle"]);

            if (item.contains("ActionData")) action->actionData = item["ActionData"];
            else if (item.contains("JsonRPCAction")) action->actionData = item["JsonRPCAction"];

            if (item.contains("IcoPath")) {
                std::wstring icoPath = utf8_to_wide(item["IcoPath"]);
                std::wstring iconFilePath;

                if (icoPath.find(L":\\") == std::wstring::npos &&
                    icoPath.find(L'/') != 0) {
                    if (script) {
                        iconFilePath = script->GetDir() + L"\\" + icoPath;
                    }
                } else {
                    iconFilePath = icoPath;
                }
                if (!iconFilePath.empty()) {
                    if (IsImageIconPath(iconFilePath)) {
                        action->iconKey = iconFilePath;
                        if (seenIconKeys.insert(iconFilePath).second) {
                            action->iconBitmap = CopyHBitmap(GetCachedIcon(iconFilePath));
                        }
                    } else {
                        // 其它文件由主程序延迟解析系统图标
                        action->iconPath = iconFilePath;
                    }
                }
            }
            results.push_back(action);
        }
        return results;
    }

    bool PostToUiThread(const UINT message, const uint64_t queryId) const {
        if (!m_msgHwnd) return false;
        return PostMessageW(m_msgHwnd, message, static_cast<WPARAM>(static_cast<uint32_t>(queryId)),
                            static_cast<LPARAM>(static_cast<uint32_t>(queryId >> 32))) != FALSE;
    }

    // 任意线程。takeOver：发布到 m_results 并让 UI 线程整表显示（替换占位）；否则追加到当前列表。
    // 接管时空结果也要投递，否则 "查询中" 占位或上一次的旧结果会一直留在列表里
    void DeliverResults(const uint64_t queryId, const std::wstring& scriptId,
                        std::vector<std::shared_ptr<NodeJsAction>> items, const bool takeOver) {
        if (queryId != GetHost().GetCurrentQueryId()) return;
        if (!takeOver && items.empty()) return;
        if (takeOver) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_lastResultScriptId = scriptId;
            }
            m_results.Publish(GetHost(), std::move(items));
            PostToUiThread(WM_NODEJS_SHOW_RESULTS, queryId);
        } else {
            m_appended.PostResults(GetHost(), queryId, items, true, true);
        }
    }

    // 读取线程：JS 插件主动推送的结果 / 修改输入框
    void OnJsNotification(const std::string& method, const nlohmann::json& params) {
        if (method == "showResults") {
            if (params.contains("pluginId") && params["pluginId"].is_string() &&
                params.contains("results") && params["results"].is_array()) {

                std::string jsPluginId = params["pluginId"];

                // node-host 在知道来源查询时会带上 queryId；旧查询的异步推送不能被当成当前查询的结果
                uint64_t queryId = 0;
                bool takeOver = false;
                {
                    std::lock_guard<std::mutex> lock(m_stateMutex);
                    if (params.contains("queryId") && params["queryId"].is_number_unsigned() &&
                        params["queryId"].get<uint64_t>() != m_currentQueryId) {
                        return;
                    }
                    queryId = m_currentQueryId;
                    takeOver = m_currentTakeOver;
                }
                auto actions = ParseActionsFromJson(jsPluginId, params["results"]);
                DeliverResults(queryId, utf8_to_wide(jsPluginId), std::move(actions), takeOver);
            }
        } else if (method == "changeQuery") {
            if (params.contains("query") && params["query"].is_string()) {
                {
                    std::lock_guard<std::mutex> lock(m_stateMutex);
                    m_pendingEditText = utf8_to_wide(params["query"]);
                }
                PostToUiThread(WM_NODEJS_CHANGE_QUERY, 0);
            }
        }
    }

    // UI 线程
    void OnShowResultsMessage(const uint64_t queryId) {
        if (queryId != GetHost().GetCurrentQueryId()) return;
        GetHost().ShowActionSet(m_results.Id());
    }

    void OnChangeQueryMessage() {
        std::wstring text;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            text.swap(m_pendingEditText);
        }
        GetHost().SetEditText(text);
    }

    static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<NodeJsPlugin*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message) {
        case WM_NCCREATE:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
            return TRUE;
        case WM_NODEJS_SHOW_RESULTS:
            if (self) {
                const uint64_t queryId = static_cast<uint64_t>(static_cast<uint32_t>(wparam)) |
                    (static_cast<uint64_t>(static_cast<uint32_t>(lparam)) << 32);
                self->OnShowResultsMessage(queryId);
            }
            return 0;
        case WM_NODEJS_CHANGE_QUERY:
            if (self) self->OnChangeQueryMessage();
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    bool CreateMessageWindow() {
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&NodeJsPlugin::MsgWndProc), &m_moduleInstance);
        WNDCLASSW wc{};
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = m_moduleInstance;
        wc.lpszClassName = NODEJS_MSG_WINDOW_CLASS;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        m_msgHwnd = CreateWindowExW(0, NODEJS_MSG_WINDOW_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    m_moduleInstance, this);
        return m_msgHwnd != nullptr;
    }

    void DestroyMessageWindow() {
        if (m_msgHwnd) {
            DestroyWindow(m_msgHwnd);
            m_msgHwnd = nullptr;
        }
        UnregisterClassW(NODEJS_MSG_WINDOW_CLASS, m_moduleInstance);
    }

    // 回收已经结束的查询线程
    void ReapWorkers() {
        m_workers.erase(std::remove_if(m_workers.begin(), m_workers.end(), [](std::future<void>& f) {
            return !f.valid() || f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }), m_workers.end());
    }

    void WaitWorkers() {
        for (auto& f : m_workers) {
            if (f.valid()) f.wait();
        }
        m_workers.clear();
    }

    std::shared_ptr<NodeJsAction> FindAction(const uint64_t userData) const {
        if (auto action = m_results.Find(userData)) return action;
        if (auto action = m_appended.Find(userData)) return action;
        return m_placeholder.Find(userData);
    }

public:
    NodeJsPlugin() = default;

    ~NodeJsPlugin() override {
        if (m_bridge) {
            m_bridge->Stop();
        }
    }

    bool Initialize() override {
        m_host = &GetHost();
        m_scriptManager = std::make_unique<NodeJsScriptManager>();

        std::wstring exeDir = GetExecutableFolder();
        m_scriptsDir = exeDir + L"\\plugins\\NodeJsPlugin\\scripts";

        if (!CreateMessageWindow()) {
            Loge(L"NodeJsPlugin", L"创建消息窗口失败");
            return false;
        }

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

        Loge(L"NodeJsPlugin", L"启动 node.exe 失败: ", hostScriptPath);
        DestroyMessageWindow();
        return false;
    }

    void OnUserSettingsLoaded() override {
        // 只有以脚本关键字开头的输入才路由到本插件；有通配符脚本时所有输入都要收到
        std::vector<std::wstring> keywords;
        bool hasWildcard = false;
        for (const auto& script : m_scriptManager->GetAllScripts()) {
            for (const auto& kw : script->GetTriggerKeywords()) {
                if (kw == L"*") hasWildcard = true;
                else keywords.push_back(kw);
            }
        }
        GetHost().SetTriggerKeywords(keywords, hasWildcard ? CL_TRIGGER_ALSO_GLOBAL : 0u);
    }

    void RefreshActions() override {
    }

    void Shutdown() override {
        // 先停掉 node（未完成的请求随之失效，查询线程会立即退出），再销毁窗口，最后释放条目
        if (m_bridge) {
            m_bridge->Stop();
        }
        WaitWorkers();
        DestroyMessageWindow();
        m_results.ClearLocal();
        m_placeholder.Begin();
        m_appended.Begin();
        {
            std::lock_guard<std::mutex> lock(m_iconMutex);
            m_iconCache.clear();
        }
        pluginIconMap.clear();
        m_host = nullptr;
    }

    void OnMainWindowShow(bool isShow) override {
        if (!isShow) {
            Logi(L"NodeJsPlugin", L"MainWindow hidden - cleanup");
        }
    }

    bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
        const auto jsAction = FindAction(userData);
        // 占位条目没有脚本 id
        if (!jsAction || jsAction->jsPluginId.empty() || !m_bridge) return false;

        nlohmann::json params;
        params["pluginId"] = wide_to_utf8(jsAction->jsPluginId);
        params["actionData"] = jsAction->actionData;

        m_bridge->CallAsync("execute", params);
        return true;
    }

    // 同步路径：有关键字脚本匹配时立即接管列表。还是上次那个脚本就直接显示上次结果（不闪烁），
    // 否则显示 "查询中" 占位；真正的结果由 OnQuery 异步取回后替换
    void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
        if (!m_bridge) return;
        const auto matched = MatchScripts(input);
        const MatchedScript* keywordScript = FirstKeywordScript(matched);
        if (!keywordScript) return;

        const std::wstring& currectId = keywordScript->script->GetId();
        std::wstring lastResultId;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            lastResultId = m_lastResultScriptId;
        }
        Logi(L"NodeJsPlugin", L"上次插件id: ", lastResultId);
        Logi(L"NodeJsPlugin", L"本次插件id: ", currectId);
        if (currectId == lastResultId && m_results.Size() > 0) {
            // 缓存上次结果，这样查询就不会闪烁了
            Logi(L"NodeJsPlugin", L"返回上次结果插件: ", lastResultId);
            sink.AddAll(m_results.Id());
            return;
        }

        // 立即返回占位等待action
        auto action = std::make_shared<NodeJsAction>();
        action->title = L"查询中";
        if (const auto it = pluginIconMap.find(currectId); it != pluginIconMap.end()) {
            HBITMAP src = it->second.get();
            if (src) {
                action->iconBitmap = CopyHBitmap(src);
            }
            Logi(L"NodeJsPlugin", L"找到插件icon");
        }
        sink.AddTransient(m_placeholder, {action});
    }

    // 异步路径（2.2）：把查询交给 node，工作线程等到所有脚本回复后投递结果
    void OnQuery(const uint64_t queryId, const std::wstring& input, const std::wstring& arg) override {
        const auto matched = m_bridge ? MatchScripts(input) : std::vector<MatchedScript>{};
        const bool takeOver = FirstKeywordScript(matched) != nullptr;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_currentQueryId = queryId;
            m_currentQuerySent = !matched.empty();
            m_currentTakeOver = takeOver;
        }
        if (matched.empty()) return;

        auto futuresPtr = std::make_shared<std::vector<std::pair<std::wstring, std::future<nlohmann::json>>>>();
        for (const auto& m : matched) {
            nlohmann::json params;
            params["query"] = wide_to_utf8(input);
            params["pluginId"] = wide_to_utf8(m.script->GetId());
            params["queryId"] = queryId;

            futuresPtr->emplace_back(m.script->GetId(), m_bridge->CallAsync("query", params));
        }

        MethodTimerStart(L"nodejsplugin query");
        Logi(L"NodeJsPlugin", L"查询 ", futuresPtr->size());

        ReapWorkers();
        m_workers.push_back(std::async(std::launch::async, [this, futuresPtr, queryId, takeOver]() {
            std::vector<std::shared_ptr<NodeJsAction>> finalResults;

            for (auto& f : *futuresPtr) {
                try {
                    // 后台线程里可以等，不会阻塞 UI/input
                    auto response = f.second.get();
                    if (queryId != GetHost().GetCurrentQueryId()) {
                        Logi(L"NodeJsPlugin", L"Query sequence superseded (loss)");
                        return;
                    }
                    if (!response.contains("data") || !response["data"].is_array()) {
                        Loge(L"NodeJsPlugin", L"Plugin ", f.first, L" 响应格式错误: ", response.dump());
                        continue;
                    }

                    auto actions = ParseActionsFromJson(wide_to_utf8(f.first), response["data"]);
                    finalResults.insert(finalResults.end(), actions.begin(), actions.end());

                } catch (const std::exception& e) {
                    // 桥接停止时未完成的请求会抛 future_error，此时静默退出
                    if (queryId != GetHost().GetCurrentQueryId()) return;
                    Loge(L"NodeJsPlugin", L"Plugin ", f.first, L" query 异步异常: ", e.what());
                }
            }

            MethodTimerEnd(L"nodejsplugin query");
            // 关键：异步完成后刷新结果；接管列表时即使为空（无结果 / 脚本出错）也要投递以清掉占位
            DeliverResults(queryId, futuresPtr->at(0).first, std::move(finalResults), takeOver);
        }));
    }

    void OnQueryCancel(const uint64_t queryId) override {
        bool sent = false;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            sent = m_currentQuerySent && m_currentQueryId == queryId;
            if (sent) m_currentQuerySent = false;
        }
        if (!sent || !m_bridge) return;
        nlohmann::json params;
        params["queryId"] = queryId;
        m_bridge->Notify("cancel", params);
    }
};

CANDY_PLUGIN_EXPORT(NodeJsPlugin, L"Node.js Plugin Bridge", L"Run script plugins through Node.js", L"1.1.0", L"com.gemini.nodejsplugin", nullptr)
