#pragma once

#include "VisualStudioAction.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <array>
#include <pugixml.hpp>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/BitmapUtil.hpp"
#include "util/json.hpp"

using json = nlohmann::json;

// 存储 Visual Studio 实例信息
struct VSInstance {
	std::wstring instanceId;
	std::wstring displayName;
	std::wstring productPath;
	std::wstring productLineVersion;
	bool isPrerelease = false;
	std::wstring applicationPrivateSettingsPath;
};

// 执行命令并获取输出
static std::string ExecuteCommand(const std::wstring& command) {
	std::string result;

	// 转换为窄字符串
	std::string cmdA = wide_to_utf8(command);

	std::array<char, 128> buffer;
	FILE* pipe = _popen(cmdA.c_str(), "r");

	if (!pipe) {
		Loge(L"VisualStudio", L"Failed to execute command: ", command);
		return result;
	}

	try {
		while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr) {
			result += buffer.data();
		}
	} catch (...) {
		_pclose(pipe);
		throw;
	}

	_pclose(pipe);
	return result;
}

// ---- Visual Studio Setup Configuration COM API（vswhere.exe 内部使用的就是这套接口）----
// 接口定义来自 Microsoft.VisualStudio.Setup.Configuration.Native 的 Setup.Configuration.h

struct __declspec(uuid("C601C175-A3BE-44BC-91F6-4568D230FC83")) ISetupPropertyStore : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetNames(LPSAFEARRAY* ppsaNames) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetValue(LPCOLESTR pwszName, LPVARIANT pvtValue) = 0;
};

struct __declspec(uuid("B41463C3-8866-43B5-BC33-2B0676F7F42E")) ISetupInstance : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetInstanceId(BSTR* pbstrInstanceId) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInstallDate(LPFILETIME pInstallDate) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInstallationName(BSTR* pbstrInstallationName) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInstallationPath(BSTR* pbstrInstallationPath) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInstallationVersion(BSTR* pbstrInstallationVersion) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayName(LCID lcid, BSTR* pbstrDisplayName) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDescription(LCID lcid, BSTR* pbstrDescription) = 0;
	virtual HRESULT STDMETHODCALLTYPE ResolvePath(LPCOLESTR pwszRelativePath, BSTR* pbstrAbsolutePath) = 0;
};

struct __declspec(uuid("89143C9A-05AF-49B0-B717-72E218A2185C")) ISetupInstance2 : ISetupInstance {
	virtual HRESULT STDMETHODCALLTYPE GetState(ULONG* pState) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPackages(LPSAFEARRAY* ppsaPackages) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProduct(IUnknown** ppPackage) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProductPath(BSTR* pbstrProductPath) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetErrors(IUnknown** ppErrorState) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsLaunchable(VARIANT_BOOL* pfIsLaunchable) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsComplete(VARIANT_BOOL* pfIsComplete) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProperties(ISetupPropertyStore** ppProperties) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetEnginePath(BSTR* pbstrEnginePath) = 0;
};

struct __declspec(uuid("9AD8E40F-39A2-40F1-BF64-0A6C50DD9EEB")) ISetupInstanceCatalog : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetCatalogInfo(ISetupPropertyStore** ppCatalogInfo) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsPrerelease(VARIANT_BOOL* pfIsPrerelease) = 0;
};

struct __declspec(uuid("6380BCFF-41D3-4B2E-8B2E-BF8A6810C848")) IEnumSetupInstances : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, ISetupInstance** rgelt, ULONG* pceltFetched) = 0;
	virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt) = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IEnumSetupInstances** ppenum) = 0;
};

struct __declspec(uuid("42843719-DB4C-46C2-8E7C-64F1816EFD5B")) ISetupConfiguration : IUnknown {
	virtual HRESULT STDMETHODCALLTYPE EnumInstances(IEnumSetupInstances** ppEnumInstances) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInstanceForCurrentProcess(ISetupInstance** ppInstance) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInstanceForPath(LPCWSTR wzPath, ISetupInstance** ppInstance) = 0;
};

struct __declspec(uuid("26AAB78C-4A60-49D6-AF3B-3C35BC93365D")) ISetupConfiguration2 : ISetupConfiguration {
	virtual HRESULT STDMETHODCALLTYPE EnumAllInstances(IEnumSetupInstances** ppEnumInstances) = 0;
};

class __declspec(uuid("177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D")) SetupConfiguration;

// 持有 COM 接口指针，离开作用域时自动 Release
template <class T>
struct VsComPtr {
	T* p = nullptr;
	VsComPtr() = default;
	VsComPtr(const VsComPtr&) = delete;
	VsComPtr& operator=(const VsComPtr&) = delete;
	~VsComPtr() { if (p) p->Release(); }
	T* operator->() const { return p; }
	explicit operator bool() const { return p != nullptr; }
	T** put() { return &p; }
	void** putVoid() { return reinterpret_cast<void**>(&p); }
};

// 读取 BSTR 并释放
static std::wstring TakeBstr(BSTR bstr) {
	std::wstring result = bstr ? std::wstring(bstr, SysStringLen(bstr)) : std::wstring();
	SysFreeString(bstr);
	return result;
}

// 通过 Setup Configuration COM API 在进程内获取实例列表（等价于 vswhere -all -prerelease），
// 避免启动 cmd.exe + vswhere.exe 两个进程
static bool QueryInstancesFromSetupApi(std::vector<VSInstance>& out) {
	const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) return false;

	bool ok = false;
	{
		VsComPtr<ISetupConfiguration2> config;
		VsComPtr<IEnumSetupInstances> enumInstances;
		if (SUCCEEDED(CoCreateInstance(__uuidof(SetupConfiguration), nullptr, CLSCTX_INPROC_SERVER,
				__uuidof(ISetupConfiguration2), config.putVoid())) &&
			SUCCEEDED(config->EnumAllInstances(enumInstances.put()))) {
			ok = true;
			const LCID lcid = GetUserDefaultLCID();
			while (true) {
				VsComPtr<ISetupInstance> setupInstance;
				ULONG fetched = 0;
				if (enumInstances->Next(1, setupInstance.put(), &fetched) != S_OK || fetched == 0) break;

				VSInstance instance;
				BSTR bstr = nullptr;
				if (FAILED(setupInstance->GetInstanceId(&bstr))) continue;
				instance.instanceId = TakeBstr(bstr);

				bstr = nullptr;
				if (SUCCEEDED(setupInstance->GetDisplayName(lcid, &bstr))) instance.displayName = TakeBstr(bstr);

				VsComPtr<ISetupInstance2> instance2;
				if (SUCCEEDED(setupInstance->QueryInterface(__uuidof(ISetupInstance2), instance2.putVoid()))) {
					bstr = nullptr;
					if (SUCCEEDED(instance2->GetProductPath(&bstr))) {
						// GetProductPath 返回相对于安装目录的路径
						const std::wstring relativePath = TakeBstr(bstr);
						bstr = nullptr;
						if (!relativePath.empty() && SUCCEEDED(setupInstance->ResolvePath(relativePath.c_str(), &bstr))) {
							instance.productPath = TakeBstr(bstr);
						}
					}
				}

				VsComPtr<ISetupInstanceCatalog> catalog;
				if (SUCCEEDED(setupInstance->QueryInterface(__uuidof(ISetupInstanceCatalog), catalog.putVoid()))) {
					VARIANT_BOOL isPrerelease = VARIANT_FALSE;
					if (SUCCEEDED(catalog->IsPrerelease(&isPrerelease))) {
						instance.isPrerelease = isPrerelease != VARIANT_FALSE;
					}
					VsComPtr<ISetupPropertyStore> catalogInfo;
					if (SUCCEEDED(catalog->GetCatalogInfo(catalogInfo.put())) && catalogInfo) {
						VARIANT value;
						VariantInit(&value);
						if (SUCCEEDED(catalogInfo->GetValue(L"productLineVersion", &value)) && value.vt == VT_BSTR) {
							instance.productLineVersion = std::wstring(value.bstrVal, SysStringLen(value.bstrVal));
						}
						VariantClear(&value);
					}
				}

				out.push_back(std::move(instance));
			}
		}
	}

	if (SUCCEEDED(initHr)) CoUninitialize();
	return ok;
}

// 使用 vswhere.exe 获取实例列表（Setup Configuration COM API 不可用时的回退方案）
static bool QueryInstancesFromVsWhere(std::vector<VSInstance>& out) {
	namespace fs = std::filesystem;

	// 尝试从两个位置查找 vswhere.exe
	std::vector<std::wstring> vsWherePaths = {
		LR"(C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe)",
		L"vswhere.exe" // 从 PATH 查找
	};

	std::wstring vsWherePath;
	for (const auto& path : vsWherePaths) {
		if (path == L"vswhere.exe" || fs::exists(path)) {
			vsWherePath = path;
			break;
		}
	}

	if (vsWherePath.empty()) {
		Loge(L"VisualStudio", L"vswhere.exe not found");
		return false;
	}

	// 执行 vswhere.exe 并获取 JSON 输出
	std::string output = ExecuteCommand(L"\"" + vsWherePath + L"\" -all -prerelease -format json -utf8");

	if (output.empty()) {
		Logi(L"VisualStudio", L"vswhere.exe returned empty output");
		return false;
	}

	// 解析 JSON 输出
	json vswhereJson = json::parse(output);

	if (!vswhereJson.is_array()) {
		Loge(L"VisualStudio", L"vswhere.exe output is not an array");
		return false;
	}

	for (const auto& item : vswhereJson) {
		VSInstance instance;

		// 读取实例 ID
		if (item.contains("instanceId") && item["instanceId"].is_string()) {
			instance.instanceId = utf8_to_wide(item["instanceId"].get<std::string>());
		} else {
			continue;
		}

		// 读取显示名称
		if (item.contains("displayName") && item["displayName"].is_string()) {
			instance.displayName = utf8_to_wide(item["displayName"].get<std::string>());
		}

		// 读取产品路径
		if (item.contains("productPath") && item["productPath"].is_string()) {
			instance.productPath = utf8_to_wide(item["productPath"].get<std::string>());
		}

		// 读取产品线版本
		if (item.contains("catalog") && item["catalog"].is_object()) {
			const auto& catalog = item["catalog"];
			if (catalog.contains("productLineVersion") && catalog["productLineVersion"].is_string()) {
				instance.productLineVersion = utf8_to_wide(catalog["productLineVersion"].get<std::string>());
			}
		}

		// 读取是否为预发布版本
		if (item.contains("isPrerelease") && item["isPrerelease"].is_boolean()) {
			instance.isPrerelease = item["isPrerelease"].get<bool>();
		}

		out.push_back(std::move(instance));
	}
	return true;
}

// 获取 Visual Studio 实例列表
static std::vector<VSInstance> GetVisualStudioInstances() {
	namespace fs = std::filesystem;
	std::vector<VSInstance> instances;

	try {
		std::vector<VSInstance> found;
		if (!QueryInstancesFromSetupApi(found)) {
			Logi(L"VisualStudio", L"Setup Configuration API unavailable, fallback to vswhere.exe");
			found.clear();
			if (!QueryInstancesFromVsWhere(found)) {
				return instances;
			}
		}

		// 获取 LOCALAPPDATA 环境变量
		wchar_t localAppData[MAX_PATH];
		ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", localAppData, MAX_PATH);
		fs::path vsDataDir = fs::path(localAppData) / L"Microsoft" / L"VisualStudio";

		// 查找 ApplicationPrivateSettings.xml
		if (!fs::exists(vsDataDir)) {
			return instances;
		}

		for (auto& instance : found) {
			try {
				for (const auto& entry : fs::directory_iterator(vsDataDir)) {
					if (entry.is_directory()) {
						std::wstring dirName = entry.path().filename().wstring();

						// 检查目录名是否包含实例 ID，且不是备份目录
						if (dirName.find(instance.instanceId) != std::wstring::npos &&
							dirName.find(L"SettingsBackup_") == std::wstring::npos) {
							fs::path settingsPath = entry.path() / L"ApplicationPrivateSettings.xml";
							if (fs::exists(settingsPath)) {
								instance.applicationPrivateSettingsPath = settingsPath.wstring();
								break;
							}
						}
					}
				}

				// 只添加找到设置文件的实例
				if (!instance.applicationPrivateSettingsPath.empty()) {
					Logi(L"VisualStudio", L"Found VS instance: ", instance.displayName);
					instances.push_back(std::move(instance));
				}
			} catch (const std::exception& e) {
				Loge(L"VisualStudio", L"Error parsing VS instance: ", e.what());
				continue;
			}
		}
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error getting VS instances: ", e.what());
	}

	return instances;
}

// 从 ApplicationPrivateSettings.xml 中提取 CodeContainers JSON 字符串
static std::string ExtractCodeContainersJson(const std::wstring& xmlPath) {
	try {
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());
		if (!result) {
			Loge(L"VisualStudio", L"Failed to load XML: ", xmlPath);
			return "";
		}

		// 查找 collection name="CodeContainers.Offline"
		pugi::xpath_node collectionNode = doc.select_node("/content/indexed/collection[@name='CodeContainers.Offline']");
		if (!collectionNode) {
			return "";
		}

		// 找到第一个 <value name="value"> 节点
		pugi::xpath_node valueNode = collectionNode.node().select_node("value[@name='value']");
		if (!valueNode) {
			return "";
		}

		std::string jsonStr = valueNode.node().child_value();

		// XML 解码（pugixml 不会自动解码 &quot; 之类的）
		size_t pos = 0;
		while ((pos = jsonStr.find("&quot;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 6, "\"");
		}
		while ((pos = jsonStr.find("&amp;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 5, "&");
		}
		while ((pos = jsonStr.find("&lt;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 4, "<");
		}
		while ((pos = jsonStr.find("&gt;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 4, ">");
		}

		return jsonStr;
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error extracting CodeContainers: ", e.what());
		return "";
	}
}

// 从 CodeContainers JSON 中解析项目列表
static std::vector<std::shared_ptr<BaseAction>> ParseCodeContainers(
	const std::string& jsonStr,
	const VSInstance& instance,
	int maxResults = 1000) {
	namespace fs = std::filesystem;
	std::vector<std::shared_ptr<BaseAction>> result;

	try {
		if (jsonStr.empty()) {
			return result;
		}

		json containersJson = json::parse(jsonStr);

		if (!containersJson.is_array()) {
			return result;
		}

		// 所有项目共享 Visual Studio 图标，显示时再获取
		const auto icon = std::make_shared<LazySysImageIndex>(instance.productPath);
		int count = 0;

		for (const auto& container : containersJson) {
			if (maxResults > 0 && count >= maxResults) {
				break;
			}

			try {
				// 解析 Value.LocalProperties.FullPath
				if (!container.contains("Value") || !container["Value"].is_object()) {
					continue;
				}

				const auto& value = container["Value"];

				if (!value.contains("LocalProperties") || !value["LocalProperties"].is_object()) {
					continue;
				}

				const auto& localProps = value["LocalProperties"];

				if (!localProps.contains("FullPath") || !localProps["FullPath"].is_string()) {
					continue;
				}

				std::wstring fullPath = utf8_to_wide(localProps["FullPath"].get<std::string>());

				// 检查路径是否存在
				if (!fs::exists(fullPath)) {
					continue;
				}

				auto action = std::make_shared<VisualStudioAction>();

				action->projectPath = fullPath;
				action->visualStudioPath = instance.productPath;
				action->displayName = instance.displayName;
				action->isPrerelease = instance.isPrerelease;

				// 读取 IsFavorite
				if (value.contains("IsFavorite") && value["IsFavorite"].is_boolean()) {
					action->isFavorite = value["IsFavorite"].get<bool>();
				}

				// 提取文件名作为标题
				fs::path p(fullPath);
				action->title = p.filename().wstring();
				// ConsolePrintln(L"VisualStudio",action->title);

				// 副标题显示完整路径
				action->subTitle = fullPath;

				// 设置图标为 Visual Studio 可执行文件
				action->icon = icon;

				// 设置匹配文本
				try {
					action->matchText = m_host->GetTheProcessedMatchingText(action->title) + fullPath;
				} catch (...) {
					action->matchText = action->title;
				}

				result.push_back(action);
				count++;
			} catch (const std::exception& e) {
				Loge(L"VisualStudio", L"Error parsing code container: ", e.what());
				continue;
			}
		}

		Logi(L"VisualStudio",
						L"Loaded ", result.size(),
						L" projects from ", instance.displayName);
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error parsing CodeContainers JSON: ", e.what());
	}

	return result;
}

// 获取所有 Visual Studio 项目
static std::vector<std::shared_ptr<BaseAction>> GetAllVisualStudioProjects() {
	std::vector<std::shared_ptr<BaseAction>> result;

	if (!m_host) {
		Loge(L"VisualStudio", L"m_host is null");
		return result;
	}

	try {
		// 从配置读取最大结果数
		int maxResults = static_cast<int>(
			m_host->GetSettingsMap().at("com.candytek.visualstudioplugin.max_results").intValue);

		// 获取所有 Visual Studio 实例
		std::vector<VSInstance> instances = GetVisualStudioInstances();

		Logi(L"VisualStudio", L"Found ", instances.size(), L" VS instances");

		for (const auto& instance : instances) {
			// 如果不显示预发布版本，则跳过
			if (instance.isPrerelease && !showPrerelease) {
				continue;
			}

			// 提取 CodeContainers JSON
			std::string jsonStr = ExtractCodeContainersJson(instance.applicationPrivateSettingsPath);

			// 解析项目列表
			auto projects = ParseCodeContainers(jsonStr, instance, maxResults);

			result.insert(result.end(),
						std::make_move_iterator(projects.begin()),
						std::make_move_iterator(projects.end()));
		}

		Logi(L"VisualStudio", L"Total loaded ", result.size(), L" projects");
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error getting all VS projects: ", e.what());
	}

	return result;
}
