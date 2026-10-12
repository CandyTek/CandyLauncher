#include <memory>

#include "JbAction.hpp"
#include "JbPluginData.hpp"
#include "JbPluginUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "util/json.hpp"

class JetbrainsWorkspacesPlugin : public candy::Plugin {
private:
	candy::ActionSet<JbAction> workspaces{0};
	std::wstring startStr = L"jb ";

	// 设置了激活命令时，项目只通过激活命令显示，不参与全局搜索
	uint32_t WorkspaceSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		// Get all JetBrains workspace projects
		workspaces.SetFlags(GetHost(), WorkspaceSetFlags());
		workspaces.Publish(GetHost(), GetAllJetBrainsWorkspaces());
	}

	void OnSettingItemExecute(const std::wstring& key, HWND parentHwnd) override {
		if (key == L"com.candytek.jetbrainsworkspacesplugin.edit_config_ide_paths") {
			wchar_t path[MAX_PATH];
			wcsncpy_s(path, CONFIG_IDE_PATHS_PATH.c_str(), MAX_PATH - 1);
			ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
		}
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.jetbrainsworkspacesplugin.start_str", L"jb ");
		workspaces.SetFlags(host, WorkspaceSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	void Shutdown() override {
		workspaces.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(workspaces.Id(), input.substr(startStr.size()));
		}
	}

	bool OnHotKey(const uint64_t userData, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		if (currentModifiers == MOD_ALT && vk == VK_RETURN) {
			const auto workspaceAction = workspaces.Find(userData);
			if (!workspaceAction) return false;
			// Alt+Enter: 直接用对应的 IDE 打开项目
			if (workspaceAction->iconPath.empty() || workspaceAction->projectPath.empty()) {
				// GetHost().ShowToast(L"无法打开项目", L"未找到 " + workspaceAction->ideName + L" 的安装路径");
				return false;
			}
			ShellExecuteW(nullptr, L"open", workspaceAction->iconPath.c_str(),
						(L"\"" + workspaceAction->projectPath + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
			GetHost().TaskDone();
			return true;
		}
		return false;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto workspaceAction = workspaces.Find(userData);
		if (!workspaceAction || workspaceAction->projectPath.empty()) return false;
		text = workspaceAction->projectPath;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto workspaceAction = workspaces.Find(userData);
		if (!workspaceAction) return false;

		// 执行时把项目路径作为参数填入输入框，由用户再选择用哪个软件打开
		if (const std::wstring& projectPath = workspaceAction->projectPath; !projectPath.empty()) {
			nlohmann::json j;
			j["arg"] = wide_to_utf8(projectPath);
			GetHost().SetEditText(utf8_to_wide(j.dump()) + L" " + workspaceAction->ideName);
		}
		return false;
	}
};

// 只读取 Roaming / LocalAppData 下的配置文件和 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(JetbrainsWorkspacesPlugin, L"JetBrains Workspaces", L"Search recent projects of JetBrains IDEs", L"1.0.0",
						L"com.candytek.jetbrainsworkspacesplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
