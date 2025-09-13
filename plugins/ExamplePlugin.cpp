#include "SimplePlugin.h"
#include <windows.h>

class ExamplePlugin : public ISimplePlugin {
private:
    ISimplePluginHost* m_host = nullptr;

public:
    ExamplePlugin() = default;
    ~ExamplePlugin() override = default;

    std::wstring GetName() const override {
        return L"ExamplePlugin";
    }

    std::string GetVersion() const override {
        return "1.0.0";
    }

    std::string GetDescription() const override {
        return "简单示例插件";
    }

    bool Initialize(ISimplePluginHost* host) override {
        m_host = host;
        return m_host != nullptr;
    }

    void Shutdown() override {
        if (m_host) {
            m_host->ClearActions(GetName());
        }
        m_host = nullptr;
    }

    void LoadActions() override {
        if (!m_host) return;

        SimpleActionItem action1;
        action1.id = L"ExamplePlugin_hello";
        action1.title = L"Hello World";
        action1.subtitle = L"显示Hello World消息";
        action1.priority = 0;
        m_host->AddAction(action1);

        SimpleActionItem action2;
        action2.id = L"ExamplePlugin_time";
        action2.title = L"显示时间";
        action2.subtitle = L"显示当前系统时间";
        action2.priority = 1;
        m_host->AddAction(action2);

        SimpleActionItem action3;
        action3.id = L"ExamplePlugin_time_en";
        action3.title = L"Show Time";
        action3.subtitle = L"Show current system time";
        action3.priority = 2;
        m_host->AddAction(action3);
    }

    void OnUserInput(const std::wstring& input) override {
        if (!m_host) return;

        if (input.find(L"test") != std::string::npos) {
            SimpleActionItem action;
            action.id = L"ExamplePlugin_dynamic_test";
            action.title = L"测试: " + input;
            action.subtitle = L"这是动态生成的测试动作";
            action.priority = 2;
            m_host->AddAction(action);
        }
    }

    void OnActionExecute(const std::string& actionId) override {
        if (!m_host) return;

        if (actionId == "ExamplePlugin_hello") {
            m_host->ShowMessage("示例插件", "Hello World from Plugin!");
        }
        else if (actionId == "ExamplePlugin_time") {
            SYSTEMTIME st;
            GetSystemTime(&st);
            char timeStr[256];
            sprintf_s(timeStr, "当前时间: %04d-%02d-%02d %02d:%02d:%02d UTC",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            m_host->ShowMessage("时间", timeStr);
        }
        else if (actionId == "ExamplePlugin_dynamic_test") {
            m_host->ShowMessage("测试", "这是动态生成的测试动作被执行了！");
        }
    }
};

SIMPLE_PLUGIN_EXPORT ISimplePlugin* CreateSimplePlugin() {
    return new ExamplePlugin();
}

SIMPLE_PLUGIN_EXPORT void DestroySimplePlugin(ISimplePlugin* plugin) {
    delete plugin;
}

SIMPLE_PLUGIN_EXPORT const char* GetSimplePluginApiVersion() {
    return "1.0.0";
}
