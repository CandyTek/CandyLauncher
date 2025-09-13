#pragma once
#include "SimplePlugin.h"
#include <windows.h>

class ExamplePlugin : public ISimplePlugin {
private:
    ISimplePluginHost* m_host = nullptr;

public:
    ExamplePlugin() = default;
    ~ExamplePlugin() override = default;

    std::string GetName() const override {
        return "ExamplePlugin";
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
        action1.id = "ExamplePlugin_hello";
        action1.title = "Hello World";
        action1.subtitle = "显示Hello World消息";
        action1.priority = 0;
        m_host->AddAction(action1);

        SimpleActionItem action2;
        action2.id = "ExamplePlugin_time";
        action2.title = "显示时间";
        action2.subtitle = "显示当前系统时间";
        action2.priority = 1;
        m_host->AddAction(action2);
    }

    void OnUserInput(const std::string& input) override {
        if (!m_host) return;

        // 只在输入包含特定关键词时才添加动态actions，并且使用固定ID避免重复
        if (input.find("test") != std::string::npos) {
            SimpleActionItem action;
            action.id = "ExamplePlugin_dynamic_test";
            action.title = "测试功能";
            action.subtitle = "执行测试功能 (输入包含test)";
            action.priority = 2;
            m_host->AddAction(action); // AddAction会处理重复ID的情况
        }

        if (input.find("calc") != std::string::npos) {
            SimpleActionItem action;
            action.id = "ExamplePlugin_calculator";
            action.title = "计算器";
            action.subtitle = "打开系统计算器 (输入包含calc)";
            action.priority = 3;
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
        else if (actionId == "ExamplePlugin_calculator") {
            ShellExecuteA(nullptr, "open", "calc.exe", nullptr, nullptr, SW_SHOWNORMAL);
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