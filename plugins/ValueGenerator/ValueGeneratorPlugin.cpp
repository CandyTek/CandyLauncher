#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "ValueGeneratorAction.hpp"
#include "ValueGeneratorPluginData.hpp"
#include "ValueGeneratorUtil.hpp"
#include "../../util/ClipboardUtil.hpp"
#include "../../util/StringUtil.hpp"

inline constexpr wchar_t kValueGeneratorStartStrKey[] = L"com.candytek.valuegeneratorplugin.start_str";

struct ValueGeneratorEntry {
	const wchar_t* title;
	const wchar_t* subTitle;
	int iconIndex;
};

// 生成器列表，按 title 分派计算
inline const ValueGeneratorEntry valueGeneratorEntries[] = {
	// UUID 生成器
	{L"UUID v1", L"生成基于时间的 UUID", 70},
	{L"UUID v3", L"生成基于 MD5 的命名空间 UUID (需要命名空间和名称)", 70},
	{L"UUID v4", L"生成随机 UUID", 70},
	{L"UUID v5", L"生成基于 SHA1 的命名空间 UUID (需要命名空间和名称)", 70},
	{L"UUID v7", L"生成基于时间排序的 UUID", 70},
	// 哈希计算
	{L"MD5", L"计算字符串的 MD5 哈希值", 155},
	{L"SHA1", L"计算字符串的 SHA1 哈希值", 155},
	{L"SHA256", L"计算字符串的 SHA256 哈希值", 155},
	{L"SHA384", L"计算字符串的 SHA384 哈希值", 155},
	{L"SHA512", L"计算字符串的 SHA512 哈希值", 155},
	// Base64 编码/解码
	{L"Base64 Encode", L"将字符串编码为 Base64", 265},
	{L"Base64 Decode", L"将 Base64 字符串解码", 265},
	// URL 编码/解码
	{L"URL Encode", L"将字符串编码为 URL 格式", 14},
	{L"URL Decode", L"将 URL 编码的字符串解码", 14},
	// Data Escape/Unescape
	{L"Data Escape", L"转义数据字符串 (Uri.EscapeDataString)", 14},
	{L"Data Unescape", L"解除数据字符串转义 (Uri.UnescapeDataString)", 14},
	// Hex Escape/Unescape
	{L"Hex Escape", L"将单个字符转换为十六进制转义 (Uri.HexEscape)", 14},
	{L"Hex Unescape", L"解除十六进制转义为字符 (Uri.HexUnescape)", 14},
};

// 直接计算命令（格式：md5 abcd1234）→ 生成器 title
inline const std::pair<const wchar_t*, const wchar_t*> valueGeneratorDirectCommands[] = {
	{L"md5 ", L"MD5"},
	{L"sha1 ", L"SHA1"},
	{L"sha256 ", L"SHA256"},
	{L"sha384 ", L"SHA384"},
	{L"sha512 ", L"SHA512"},
	{L"base64 ", L"Base64 Encode"},
	{L"base64d ", L"Base64 Decode"},
	{L"url ", L"URL Encode"},
	{L"urld ", L"URL Decode"},
	{L"esc:hex ", L"Hex Escape"},
	{L"esc:data ", L"Data Escape"},
	{L"uesc:hex ", L"Hex Unescape"},
	{L"uesc:data ", L"Data Unescape"},
};

class ValueGeneratorPlugin : public candy::Plugin {
private:
	// 0 号集合：生成器列表，留空激活命令时参与全局搜索
	candy::ActionSet<ValueGeneratorAction> generators{0};
	// 1 号：直接计算命令每次按键生成的结果，不进入任何集合
	candy::TransientSet<ValueGeneratorAction> results{1};
	std::wstring startStr = L"value ";

	// 设置了激活命令时，生成器只通过激活命令显示，不参与全局搜索
	uint32_t GeneratorSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	void ApplyTriggerKeywords() const {
		if (startStr.empty()) GetHost().SetTriggerKeywords({});
		else GetHost().SetTriggerKeywords({startStr});
	}

	std::shared_ptr<ValueGeneratorAction> FindAction(const uint64_t userData) const {
		if (auto action = results.Find(userData)) return action;
		return generators.Find(userData);
	}

	static const ValueGeneratorEntry* FindEntry(const std::wstring& title) {
		for (const auto& entry : valueGeneratorEntries) {
			if (title == entry.title) return &entry;
		}
		return nullptr;
	}

	// 根据 title 创建字符串计算请求（不含 UUID），未知 title 返回 nullptr
	static std::shared_ptr<IComputeRequest> MakeStringRequest(const std::wstring& title, const std::wstring& data) {
		if (title == L"MD5") return std::make_shared<HashRequest>(HashAlgorithm::MD5, data);
		if (title == L"SHA1") return std::make_shared<HashRequest>(HashAlgorithm::SHA1, data);
		if (title == L"SHA256") return std::make_shared<HashRequest>(HashAlgorithm::SHA256, data);
		if (title == L"SHA384") return std::make_shared<HashRequest>(HashAlgorithm::SHA384, data);
		if (title == L"SHA512") return std::make_shared<HashRequest>(HashAlgorithm::SHA512, data);
		if (title == L"Base64 Encode") return std::make_shared<Base64Request>(data);
		if (title == L"Base64 Decode") return std::make_shared<Base64DecodeRequest>(data);
		if (title == L"URL Encode") return std::make_shared<UrlEncodeRequest>(data);
		if (title == L"URL Decode") return std::make_shared<UrlDecodeRequest>(data);
		if (title == L"Data Escape") return std::make_shared<DataEscapeRequest>(data);
		if (title == L"Data Unescape") return std::make_shared<DataUnescapeRequest>(data);
		if (title == L"Hex Escape") return std::make_shared<HexEscapeRequest>(data);
		if (title == L"Hex Unescape") return std::make_shared<HexUnescapeRequest>(data);
		return nullptr;
	}

	// 解析 "ns:<dns|url|oid|x500> 名称" 并创建 UUID v3 / v5 请求，参数不合法时提示并返回 nullptr
	std::shared_ptr<IComputeRequest> MakeNamespaceUuidRequest(const int version, const std::wstring& actualArg) const {
		const std::wstring name = L"UUID v" + std::to_wstring(version);
		if (actualArg.empty()) {
			GetHost().ShowMessage(L"提示", name + L" 需要命名空间和名称参数\n格式: ns:<dns|url|oid|x500> 名称\n示例: ns:dns example.com");
			return nullptr;
		}
		// 解析参数
		const size_t spacePos = actualArg.find(L' ');
		if (spacePos == std::wstring::npos) {
			GetHost().ShowMessage(L"错误", L"参数格式错误\n格式: ns:<dns|url|oid|x500> 名称");
			return nullptr;
		}
		const std::wstring namespacePart = actualArg.substr(0, spacePos);
		const std::wstring namePart = actualArg.substr(spacePos + 1);
		return std::make_shared<GUIDRequest>(version, namespacePart, namePart);
	}

	void CopyToClipboard(const std::wstring& text) const {
		// 主程序低于 2.1 时没有 set_clipboard_text，退回自行复制
		if (!GetHost().SetClipboardText(text)) CopyTextToClipboard(nullptr, text);
	}

public:
	ValueGeneratorPlugin() = default;
	~ValueGeneratorPlugin() override = default;

	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		std::vector<std::shared_ptr<ValueGeneratorAction>> items;
		for (const auto& entry : valueGeneratorEntries) {
			// matchText 留空，由主程序根据 title 生成（小写 + 拼音）
			const auto action = std::make_shared<ValueGeneratorAction>();
			action->title = entry.title;
			action->subtitle = entry.subTitle;
			action->iconPath = L"shell32.dll";
			action->iconIndex = entry.iconIndex;
			action->stableKey = std::wstring(L"ValueGeneratorPlugin_") + entry.title;
			items.push_back(action);
		}
		generators.SetFlags(GetHost(), GeneratorSetFlags());
		generators.Publish(GetHost(), std::move(items));
	}

	void OnUserSettingsLoaded() override {
		startStr = GetHost().GetString(kValueGeneratorStartStrKey, L"value ");
		generators.SetFlags(GetHost(), GeneratorSetFlags());
		ApplyTriggerKeywords();
	}

	void OnSettingChanged(const std::wstring& key) override {
		if (key == kValueGeneratorStartStrKey) {
			startStr = GetHost().GetString(kValueGeneratorStartStrKey, L"value ");
			generators.SetFlags(GetHost(), GeneratorSetFlags());
			ApplyTriggerKeywords();
		}
	}

	void Shutdown() override {
		generators.ClearLocal();
		results.Begin();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		// 由插件系统管理
		if (startStr.empty() || !StartsWith(input, startStr)) return;
		const std::wstring restInput = input.substr(startStr.size());

		// 检查是否是直接计算命令（格式：md5 abcd1234）
		for (const auto& [command, actionTitle] : valueGeneratorDirectCommands) {
			if (!StartsWith(restInput, command)) continue;
			const std::wstring dataToProcess = restInput.substr(std::wcslen(command));
			// 没有数据时继续显示生成器列表
			if (dataToProcess.empty()) continue;
			const ValueGeneratorEntry* entry = FindEntry(actionTitle);
			if (!entry) continue;

			// 创建一个新的action显示计算结果
			auto resultAction = std::make_shared<ValueGeneratorAction>();
			resultAction->title = entry->title;
			resultAction->iconPath = L"shell32.dll";
			resultAction->iconIndex = entry->iconIndex;

			// 执行计算
			const auto request = MakeStringRequest(actionTitle, dataToProcess);
			if (request && request->Compute()) {
				resultAction->result = request->ResultToString();
				resultAction->subtitle = resultAction->result;
				// 存储数据到action的arg中，以便后续执行时使用
				resultAction->arg = dataToProcess;
			} else if (request) {
				resultAction->subtitle = L"错误: " + request->GetErrorMessage();
			}
			sink.AddTransient(results, {resultAction});
			return;
		}

		// 否则显示本插件actions
		sink.AddMatches(generators.Id(), restInput);
	}

	// Ctrl+C 复制直接计算命令的结果
	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action = results.Find(userData);
		if (!action || action->result.empty()) return false;
		text = action->result;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto workspaceAction = FindAction(userData);
		if (!workspaceAction) return false;

		std::shared_ptr<IComputeRequest> request;
		const std::wstring& title = workspaceAction->title;

		// 如果PluginAction中已经有arg，使用它；否则使用参数传入的arg
		const std::wstring actualArg = workspaceAction->arg.empty() ? arg : workspaceAction->arg;

		// 根据 title 创建相应的请求
		if (title == L"UUID v1") {
			request = std::make_shared<GUIDRequest>(1);
		} else if (title == L"UUID v3") {
			request = MakeNamespaceUuidRequest(3, actualArg);
			if (!request) return true;
		} else if (title == L"UUID v4") {
			request = std::make_shared<GUIDRequest>(4);
		} else if (title == L"UUID v5") {
			request = MakeNamespaceUuidRequest(5, actualArg);
			if (!request) return true;
		} else if (title == L"UUID v7") {
			request = std::make_shared<GUIDRequest>(7);
		} else {
			if (actualArg.empty()) {
				return false;
			}
			request = MakeStringRequest(title, actualArg);
			if (!request) return false;
		}

		// 执行计算
		if (request && request->Compute()) {
			const std::wstring result = request->ResultToString();

			// 复制结果到剪贴板
			CopyToClipboard(result);

			GetHost().ShowMessage(L"成功", L"结果已复制到剪贴板:\n" + result);
		} else if (request) {
			GetHost().ShowMessage(L"错误", request->GetErrorMessage());
		}

		return true;
	}
};

CANDY_PLUGIN_EXPORT(ValueGeneratorPlugin, L"Value Generator", L"Hashes, UUIDs and other generated values", L"1.0.0", L"com.candytek.valuegeneratorplugin",
					nullptr)
