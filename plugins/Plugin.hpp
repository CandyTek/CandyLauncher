#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "BaseAction.hpp"
#include <functional>

#include "model/FileInfo.hpp"
#include "model/SettingItem.hpp"
#include "model/TraverseOptions.hpp"

class IPluginHost {
public:
	virtual ~IPluginHost() = default;

	// virtual void ClearActions(const std::wstring& pluginName){}
	virtual void ShowMessage(const std::wstring& title, const std::wstring& message) = 0;
	virtual const std::unordered_map<std::string, SettingItem>& GetSettingsMap() = 0;
	virtual const std::unordered_map<std::string, std::function<void()>>& GetAppLaunchActionCallbacks() = 0;
	virtual void RegisterAppLaunchActionCallback(const std::string& key, std::function<void()> callback) = 0;
	virtual void UnregisterAppLaunchActionCallback(const std::string& key) = 0;
	virtual bool IsPluginEnabledByPackageName(const std::wstring& packageName) const = 0;

	virtual std::wstring GetTheProcessedMatchingText(const std::wstring& source) =0;
	virtual void TraverseFilesSimpleForEverythingSDK(
		const std::wstring& folderPath, // 起始目录
		bool recursive, // 是否递归
		const std::vector<std::wstring>& extensions, // 扩展名过滤
		const std::wstring& nameFilter, // 文件名关键字，可为空
		bool isIndexOnlyFile, // 是否只索引文件
		std::function<void(const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext)> callback) =0;

	virtual void TraverseFilesForEverythingSDK(
		const std::wstring& folderPath,
		const TraverseOptions& options,
		std::function<void(const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext)>&& callback) =0;

	virtual void ChangeEditTextText(const std::wstring& basic_string) =0;
	virtual std::wstring& GetEditTextText() =0;
	virtual std::vector<std::shared_ptr<BaseAction>> GetSuccessfullyMatchingTextActions(
		const std::wstring& input, const std::vector<std::shared_ptr<BaseAction>>& actions) =0;

	// 搜索可能的目标文件
	virtual std::vector<FileInfo> SearchPossibleTargets(const std::wstring& fileName, uint64_t resultCount) =0;

	virtual HBITMAP LoadResIconAsBitmap(int nResID, int cx, int cy) = 0;
	virtual void ShowSimpleToast(const std::wstring& title,const std::wstring& msg) = 0;
};

class IPlugin {
public:
	virtual ~IPlugin() = default;

	[[nodiscard]] virtual std::wstring GetPluginName() const = 0;
	[[nodiscard]] virtual std::wstring GetPluginDescription() const = 0;
	[[nodiscard]] virtual std::wstring GetPluginVersion() const = 0;
	[[nodiscard]] virtual std::wstring GetPluginPackageName() const = 0;
	virtual void OnPluginIdChange(uint16_t pluginId) =0;

	// 不要在初始化里加载刷新Actions，由主程序在启动时的某个时机调用 RefreshAllActions，此时用户设置项已经加载
	virtual bool Initialize(IPluginHost* host) = 0;

	// 可在此注销资源
	virtual void Shutdown() = 0;

	virtual bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) = 0;

	virtual std::wstring DefaultSettingJson() {
		return L"{}";
	}

	virtual void OnSettingItemExecute(const SettingItem* setting, HWND parentHwnd) {
	}

	virtual void OnUserSettingsLoadDone() {
	}

	// 用于全局的正常搜索的项目，调用这个方法获取每一个插件的actions并合到一起。不要在这里面实现懒加载，应该直接返回结果列表，在其他方法里实现 action 的更新
	virtual std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() {
		return {};
	}

	// 如果插件选择InterceptInputShowResultsDirectly 拦截输入，那么应该在这里返回内容

	virtual void RefreshAllActions() {
	}

	virtual void OnMainWindowShow(bool isShow) {
	}

	virtual void OnUserInput(const std::wstring& input) {
	}
	
	virtual int OnSendHotKey(const std::shared_ptr<BaseAction> action,const UINT vk,const UINT currentModifiers,const WPARAM wparam) {
		return 0;
	}

	// 一定一定不要在这里面实现复杂算法，会影响用户输入体验
	virtual std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) {
		return {};
	}

	// 右键点击列表项时调用，返回 true 表示已处理（阻止默认行为）
	virtual bool OnItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) {
		return false;
	}

	// Shift+右键点击列表项时调用，返回 true 表示已处理（阻止默认行为）
	virtual bool OnItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) {
		return false;
	}
};

#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

typedef IPlugin* (*CreatePluginFunc)();
typedef void (*DestroyPluginFunc)(IPlugin*);
typedef int (*GetPluginApiVersionFunc)();
