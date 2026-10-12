#include <memory>
#include <fstream>

#include "VisualStudioAction.hpp"
#include "VisualStudioPluginData.hpp"
#include "VisualStudioUtil.hpp"
#include "../../util/StringUtil.hpp"

class VisualStudioPlugin : public candy::Plugin {
private:
	candy::ActionSet<VisualStudioAction> projects{0};
	std::wstring startStr = L"vs ";
	// 影响索引的设置项变化后需要重新索引（主程序保存设置后不会自动刷新插件）
	// 设置了激活命令时，项目只通过激活命令显示，不参与全局搜索
	uint32_t ProjectSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		Logi(L"VisualStudio", L"RefreshActions start");

		// 获取所有 Visual Studio 项目
		auto items = GetAllVisualStudioProjects();
		Logi(L"VisualStudio", L"Loaded ", items.size(), L" projects");

		projects.SetFlags(GetHost(), ProjectSetFlags());
		projects.Publish(GetHost(), std::move(items));
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.visualstudioplugin.start_str", L"vs ");
		showPrerelease = host.GetBool(L"com.candytek.visualstudioplugin.show_prerelease", true);
		projects.SetFlags(host, ProjectSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
		// 设置保存后主程序会在工作线程上重新调用 RefreshActions，这里不用自己重建索引
	}

	void Shutdown() override {
		projects.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(projects.Id(), input.substr(startStr.size()));
		}
	}

	bool OnHotKey(const uint64_t userData, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		if (currentModifiers == MOD_ALT && vk == 'A') {
			const auto action1 = projects.Find(userData);
			if (!action1) return false;
			// Alt+A: 把项目路径作为参数填入输入框，由用户再选择用哪个软件打开
			json j;
			j["arg"] = wide_to_utf8(action1->projectPath);
			GetHost().SetEditText(utf8_to_wide(j.dump()) + GetHost().GetEditText());
			return true;
		}
		return false;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = projects.Find(userData);
		if (!action1 || action1->projectPath.empty()) return false;
		text = action1->projectPath;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = projects.Find(userData);
		if (!action1) return false;

		try {
			// 使用 Visual Studio 打开项目/解决方案
			ShellExecuteW(NULL, L"open", action1->visualStudioPath.c_str(),
						(L"\"" + action1->projectPath + L"\"").c_str(), NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}

		return true;
	}
};

// 刷新只读取 ApplicationPrivateSettings.xml 并调用 Setup Configuration COM（进程内，ThreadingModel=Both，
// QueryInstancesFromSetupApi 已兼容主程序工作线程的 MTA），可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(VisualStudioPlugin, L"Visual Studio Projects", L"Index recently opened Visual Studio projects and solutions", L"1.0.0",
						L"com.candytek.visualstudioplugin", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
