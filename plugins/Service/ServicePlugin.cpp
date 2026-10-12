#include <windows.h>
#include <memory>

#include "ServiceAction.hpp"
#include "ServicePluginData.hpp"
#include "ServiceUtil.hpp"
#include "../../util/StringUtil.hpp"

class ServicePlugin final : public candy::Plugin {
private:
	candy::ActionSet<ServiceAction> services{0};
	// 按状态 / 启动模式过滤出的条目只为本次输入存在；集合 id 不能和 ActionSet 重复
	candy::TransientSet<ServiceAction> filtered{1};
	std::wstring startStr = L"service ";

	// 设置了激活命令时，服务只通过激活命令显示，不参与全局搜索
	uint32_t ServiceSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	std::shared_ptr<ServiceAction> FindAction(const uint64_t userData) const {
		if (auto action = services.Find(userData)) return action;
		return filtered.Find(userData);
	}

	// 重新枚举全部服务并发布，可在工作线程调用
	void RefreshServices() {
		auto items = GetAllWindowsServices();
		Logi(L"Service", L"Loaded ", items.size(), L" services");
		services.Publish(GetHost(), std::move(items));
	}

	// 主程序持有条目副本，单个服务状态变化后需要重新发布集合并通知刷新列表
	void RepublishServices() {
		services.Publish(GetHost(), services.Items());
		GetHost().NotifyActionsChanged();
	}

	// 按字段包含过滤（冒号后的文本）；冒号后为空时返回 false，退回普通文本匹配
	bool AddFilteredServices(candy::ResultSink& sink, const std::wstring& searchText, const bool byStatus) {
		const size_t colonPos = searchText.find(L':');
		if (colonPos == std::wstring::npos || colonPos + 1 >= searchText.size()) return false;
		const std::wstring filter = searchText.substr(colonPos + 1);

		std::vector<std::shared_ptr<ServiceAction>> items;
		for (const auto& action : services.Items()) {
			const std::wstring& field = byStatus ? action->status : action->startMode;
			if (field.find(filter) != std::wstring::npos) {
				items.push_back(action);
			}
		}
		sink.AddTransient(filtered, items);
		return true;
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		g_worker.init();
		return true;
	}

	void RefreshActions() override {
		Logi(L"Service", L"RefreshActions start");
		services.SetFlags(GetHost(), ServiceSetFlags());
		RefreshServices();
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.serviceplugin.start_str", L"service ");
		isMatchTextServiceName = host.GetBool(L"com.candytek.serviceplugin.matchtext_servicename", true);
		services.SetFlags(host, ServiceSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	// 匹配文本的设置变化后在工作线程重建索引，再通知主程序刷新列表
	void OnSettingChanged(const std::wstring& key) override {
		if (key != L"com.candytek.serviceplugin.matchtext_servicename") return;
		isMatchTextServiceName = GetHost().GetBool(key.c_str(), true);
		g_worker.submit([this] {
			RefreshServices();
			GetHost().NotifyActionsChanged();
		});
	}

	void Shutdown() override {
		g_worker.stop();
		services.ClearLocal();
		filtered.Begin();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (startStr.empty() || !StartsWith(input, startStr)) return;

		// 单独显示本插件actions
		const std::wstring searchText = input.substr(startStr.size());

		// 支持状态过滤：status:运行中
		if (StartsWithAny(searchText, {L"status:", L"状态:"})) {
			if (AddFilteredServices(sink, searchText, true)) return;
		}

		// 支持启动模式过滤：startup:自动
		if (StartsWithAny(searchText, {L"启动:", L"startup:"})) {
			if (AddFilteredServices(sink, searchText, false)) return;
		}

		sink.AddMatches(services.Id(), searchText);
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto serviceAction = FindAction(userData);
		if (!serviceAction) return false;

		try {
			// 根据服务当前状态决定操作
			// 如果正在运行，则停止；如果已停止，则启动
			bool success = false;
			std::wstring message;

			// 启停服务会影响系统，执行前弹窗让用户再次确认
			const std::wstring confirmText = serviceAction->isRunning
				? Tr(L"msg.confirm_stop", L"Stop service \"{0}\"?")
				: Tr(L"msg.confirm_start", L"Start service \"{0}\"?");
			const int choice = MessageBoxW(nullptr, candy::Format(confirmText, {serviceAction->displayName}).c_str(),
										   Tr(L"msg.title", L"Service").c_str(), MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
			if (choice != IDYES) return false;

			if (serviceAction->isRunning) {
				// 停止服务
				success = StopWindowsService(serviceAction->serviceName);
				message = success ? Tr(L"msg.stop_ok", L"Service stopped: {0}") : Tr(L"msg.stop_fail", L"Failed to stop service: {0}");
			} else {
				// 启动服务
				success = StartWindowsService(serviceAction->serviceName);
				message = success ? Tr(L"msg.start_ok", L"Service started: {0}") : Tr(L"msg.start_fail", L"Failed to start service: {0}");
			}

			// 显示消息
			GetHost().ShowToast(Tr(L"msg.title", L"Service"), candy::Format(message, {serviceAction->displayName}));

			// 只更新当前服务的状态，而不是重新枚举所有服务（性能优化）
			if (success) {
				// 短暂延迟后更新状态，这里不能传递&号，本方法结束后serviceAction已经被销毁了
				g_worker.submit([this, serviceAction] {
					Sleep(500);
					UpdateSingleServiceStatus(serviceAction);
					RepublishServices();
				});
			} else {
				UpdateSingleServiceStatus(serviceAction);
				RepublishServices();
			}

			return success;
		} catch (...) {
			return false;
		}
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto serviceAction = FindAction(userData);
		if (!serviceAction || serviceAction->serviceName.empty()) return false;
		text = serviceAction->serviceName;
		return true;
	}
};

// 枚举服务只用到 SCM / 注册表和 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动。
// 名称 / 描述由清单 strings 里的 plugin.name / plugin.description 按界面语言覆盖；默认设置项在清单里，传 nullptr
CANDY_PLUGIN_EXPORT_EX(ServicePlugin, L"Windows Services", L"Index and manage Windows services", L"1.1.0",
						L"com.candytek.serviceplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
