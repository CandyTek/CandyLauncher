#include <memory>

#include "VscAction.hpp"
#include "VscPluginData.hpp"
#include "VscUtil.hpp"
#include "../../util/StringUtil.hpp"

class VSCodeWorkspacesPlugin : public candy::Plugin {
private:
	candy::ActionSet<VscAction> workspaces{0};
	std::wstring startStr = L"vsc ";

	// 设置了激活命令时，工作区只通过激活命令显示，不参与全局搜索
	uint32_t WorkspaceSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	// 条目图标就是对应的 VSCode 程序，存在时可直接用来启动
	static std::wstring GetActionFileIconPath(const std::shared_ptr<VscAction>& workspaceAction) {
		const std::wstring& exePath = workspaceAction->iconPath;
		if (!exePath.empty() && GetFileAttributesW(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
			return exePath;
		}
		return L"";
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		workspaces.SetFlags(GetHost(), WorkspaceSetFlags());
		workspaces.Publish(GetHost(), GetAllVSCodeWorkspaces());
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.vscodeworkspacesplugin.start_str", L"vsc ");
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

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto workspaceAction = workspaces.Find(userData);
		if (!workspaceAction || workspaceAction->projectPath.empty()) return false;
		text = workspaceAction->projectPath;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto workspaceAction = workspaces.Find(userData);
		if (!workspaceAction) return false;

		// Determine VSCode executable path
		std::wstring vscodePath;
		const std::wstring configuredPath = GetHost().GetString(L"com.candytek.vscodeworkspacesplugin.vscode_path");

		if (configuredPath.empty()) {
			vscodePath = GetActionFileIconPath(workspaceAction);
		} else {
			// 优先使用用户配置的路径
			wchar_t expandedPath[MAX_PATH];
			ExpandEnvironmentStringsW(configuredPath.c_str(), expandedPath, MAX_PATH);

			// 当用户配置的路径有误
			if (GetFileAttributesW(expandedPath) == INVALID_FILE_ATTRIBUTES) {
				vscodePath = GetActionFileIconPath(workspaceAction);
			} else {
				vscodePath = expandedPath;
			}
		}

		if (vscodePath.empty()) {
			GetHost().ShowMessage(L"Error", L"找不到 vscode 程序路径，请前往设置进行配置");
			return false;
		}


		// Build command line arguments
		const std::wstring arguments = L"--folder-uri '" + workspaceAction->originalUri + L"'";
		const std::wstring arguments2 = L"-Command \".'" + vscodePath + L"' " + arguments + L"\"";

		Logi(L"VSCode", L"run: ", vscodePath, L" arg: ", arguments2);

		// vscode 毛病比较多，使用普通的ShellExecuteW 和 CreateProcessW 都会有很大的概率打不开
		HINSTANCE hInstance = ShellExecuteW(
			nullptr,
			L"open",
			L"powershell.exe",
			arguments2.c_str(),
			nullptr,
			SW_HIDE
		);

		if ((INT_PTR)hInstance > 32) {
			return true;
		} else {
			GetHost().ShowMessage(L"Error", L"Failed to execute PowerShell command");
			return false;
		}
	}
};

// 只读取 storage.json 和 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(VSCodeWorkspacesPlugin, L"Visual Studio Code Workspaces", L"Search recent Visual Studio Code projects", L"1.0.0",
						L"com.candytek.vscodeworkspacesplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
