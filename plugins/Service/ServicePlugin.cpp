#include "../Plugin.hpp"
#include <memory>
#include <iostream>
#include <Windows.h>

#include "ServiceAction.hpp"
#include "ServicePluginData.hpp"
#include "ServiceUtil.hpp"
#include "../../util/StringUtil.hpp"

class ServicePlugin final : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"service ";

public:
	ServicePlugin() = default;
	~ServicePlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"Windows 服务";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.serviceplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"索引和管理 Windows 服务";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		g_worker.init();
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) {
			Loge(L"Service Plugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"Service Plugin", L"RefreshAllActions start");
		allPluginActions.clear();

		// 获取所有 Windows 服务
		allPluginActions = GetAllWindowsServices();

		ConsolePrintln(L"Service Plugin", L"Loaded " + std::to_wstring(allPluginActions.size()) + L" services");
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.serviceplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "service "
		},
		{
			"key": "com.candytek.serviceplugin.matchtext_servicename",
			"type": "bool",
			"title": "搜索匹配服务内部名称",
			"subPage": "plugin",
			"defValue": true
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.serviceplugin.start_str").stringValue);
		isMatchTextServiceName = m_host->GetSettingsMap().at("com.candytek.serviceplugin.matchtext_servicename").boolValue;
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
			g_worker.stop();
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
			std::wstring searchText = input.substr(startStr.size());

			// 支持状态过滤：status:运行中
			if (MyStartsWith2(searchText, L"status:") || MyStartsWith2(searchText, L"状态:")) {
				size_t colonPos = searchText.find(L':');
				if (colonPos != std::wstring::npos && colonPos + 1 < searchText.size()) {
					std::wstring statusFilter = searchText.substr(colonPos + 1);

					// 过滤出匹配状态的服务
					std::vector<std::shared_ptr<BaseAction>> filtered;
					for (auto& action : allPluginActions) {
						auto serviceAction = std::dynamic_pointer_cast<ServiceAction>(action);
						if (serviceAction && serviceAction->status.find(statusFilter) != std::wstring::npos) {
							filtered.push_back(action);
						}
					}
					return filtered;
				}
			}

			// 支持启动模式过滤：startup:自动
			if (MyStartsWith2(searchText, L"startup:") || MyStartsWith2(searchText, L"启动:")) {
				size_t colonPos = searchText.find(L':');
				if (colonPos != std::wstring::npos && colonPos + 1 < searchText.size()) {
					std::wstring startupFilter = searchText.substr(colonPos + 1);

					// 过滤出匹配启动模式的服务
					std::vector<std::shared_ptr<BaseAction>> filtered;
					for (auto& action : allPluginActions) {
						auto serviceAction = std::dynamic_pointer_cast<ServiceAction>(action);
						if (serviceAction && serviceAction->startMode.find(startupFilter) != std::wstring::npos) {
							filtered.push_back(action);
						}
					}
					return filtered;
				}
			}

			return m_host->GetSuccessfullyMatchingTextActions(searchText, allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto serviceAction = std::dynamic_pointer_cast<ServiceAction>(action);
		if (!serviceAction) return false;

		try {
			// 根据服务当前状态决定操作
			// 如果正在运行，则停止；如果已停止，则启动
			bool success = false;
			std::wstring message;

			if (serviceAction->isRunning) {
				// 停止服务
				success = StopWindowsService(serviceAction->serviceName);
				message = success ? L"成功停止服务: " + serviceAction->displayName : L"停止服务失败: " + serviceAction->displayName;
			} else {
				// 启动服务
				success = StartWindowsService(serviceAction->serviceName);
				message = success ? L"成功启动服务: " + serviceAction->displayName : L"启动服务失败: " + serviceAction->displayName;
			}

			// 显示消息
			// m_host->ShowMessage(L"服务操作", message);
			m_host->ShowSimpleToast(L"服务操作", message);

			// 只更新当前服务的状态，而不是重新枚举所有服务（性能优化）
			if (success) {
				// 短暂延迟后更新状态，这里不能传递&号，本方法结束后serviceAction已经被销毁了
				g_worker.submit([serviceAction] {
					Sleep(500);
					UpdateSingleServiceStatus(serviceAction);
				});
			} else {
				UpdateSingleServiceStatus(serviceAction);
			}

			return success;
		} catch (...) {
			// m_host->ShowMessage(L"错误", L"操作服务时发生错误");
			return false;
		}
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new ServicePlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
