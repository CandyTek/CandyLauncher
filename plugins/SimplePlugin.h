#pragma once
#include <string>
#include <vector>

struct SimpleActionItem {
    std::wstring id;
    std::wstring title;
    std::wstring subtitle;
    std::wstring iconPath;
    int priority = 0;
};

class ISimplePluginHost {
public:
    virtual ~ISimplePluginHost() = default;

    virtual void AddAction(const SimpleActionItem& item) = 0;
    virtual void RemoveAction(const std::wstring& itemId) = 0;
    virtual void UpdateAction(const SimpleActionItem& item) = 0;
    virtual void ClearActions(const std::wstring& pluginName) = 0;

    virtual void ShowMessage(const std::string& title, const std::string& message) = 0;
};

class ISimplePlugin {
public:
    virtual ~ISimplePlugin() = default;

    virtual std::wstring GetName() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::string GetDescription() const = 0;

    virtual bool Initialize(ISimplePluginHost* host) = 0;
    virtual void Shutdown() = 0;

    virtual void LoadActions() = 0;
    virtual void OnUserInput(const std::wstring& input) = 0;
    virtual void OnActionExecute(const std::string& actionId) = 0;
};

#define SIMPLE_PLUGIN_EXPORT extern "C" __declspec(dllexport)

typedef ISimplePlugin* (*CreateSimplePluginFunc)();
typedef void (*DestroySimplePluginFunc)(ISimplePlugin*);
typedef const char* (*GetSimplePluginApiVersionFunc)();
