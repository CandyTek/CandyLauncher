#pragma once
#include <memory>
#include <string>
#include <vector>
#include "ActionBase.hpp"
#include "../common/DataKeeper.hpp"
#include <functional>
#include <Shobjidl.h>
#include <string>
#include <variant>

class IPluginHost {
public:
	virtual ~IPluginHost() = default;

	// virtual void ClearActions(const std::wstring& pluginName){}
	virtual void ShowMessage(const std::wstring& title, const std::wstring& message) = 0;
	virtual const std::unordered_map<std::string, SettingItem>& GetSettingsMap() = 0;

	virtual std::wstring GetTheProcessedMatchingText(const std::wstring& source) =0;
	virtual void TraverseFilesForEverythingSDK(
		const std::wstring& folderPath,
		const TraverseOptions& options,
		std::function<void(const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext)>&& callback) =0;
};

class IPlugin {
public:
	virtual ~IPlugin() = default;

	[[nodiscard]] virtual std::wstring GetPluginName() const = 0;
	[[nodiscard]] virtual std::wstring GetPluginDescription() const = 0;
	[[nodiscard]] virtual std::wstring GetPluginVersion() const = 0;
	[[nodiscard]] virtual std::wstring GetPluginPackageName() const = 0;

	virtual bool Initialize(IPluginHost* host) = 0;
	virtual void Shutdown() = 0;
	virtual void OnActionExecute(std::shared_ptr<ActionBase>& action) = 0;

	// virtual std::wstring DefaultSettingJson() = 0;
	virtual std::wstring DefaultSettingJson() {
		return L"{}";
	}

	virtual void OnUserSettingsLoadDone() {
	}

	virtual std::vector<std::shared_ptr<ActionBase>> GetAllActions() {
		return {};
	}

	virtual void RefreshAllActions() {
	}

	virtual void OnMainWindowShow(bool isShow) {
	}

	virtual void OnUserInput(const std::wstring& input) {
	}

	virtual bool InterceptInputShowResultsDirectly(const std::wstring& input) {
		return false;
	}
};

#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

typedef IPlugin* (*CreatePluginFunc)();
typedef void (*DestroyPluginFunc)(IPlugin*);
typedef int (*GetPluginApiVersionFunc)();
