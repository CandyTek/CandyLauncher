#include "../Plugin.hpp"
#include <windows.h>
#include <memory>

#include "CalcEngine.hpp"
#include "CalcAction.hpp"
#include "CalcPluginData.hpp"
#include "latex.hpp"
#include "util/BaseTools.hpp"
#include "util/ClipboardUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/StringUtil.hpp"
#include <algorithm>
#include <string>
#include <cmath>

#include "util/BitmapUtil.hpp"


inline std::vector<std::shared_ptr<BaseAction>> allPluginActions;

inline std::wstring startStr = L"=";
inline bool isDetectReplace = true;
inline bool isShowExtraFormats = true;
// 只输入前缀时，顶部置顶项与计算历史的显示开关
inline bool isShowFunctionsSection = true;
inline bool isShowExamplesSection = true;
inline bool isShowVariablesSection = true;
inline bool isShowAngleUnitAction = true;
inline bool isShowHistory = true;

struct CalcExample {
	const wchar_t* expr;
	const wchar_t* desc;
};

// 输入前缀但没有表达式时展示的示例，回车填入输入框
inline const CalcExample calcExamples[] = {
	{L"2^10 + sqrt(16) * 5!", L"基础运算: + - * / ^ % n! 以及 abs round floor log exp 等函数"},
	{L"sin(pi/6) + atan2(1, 1) + sin(30°)", L"三角函数，角度单位可在设置中切换，° 始终表示角度"},
	{L"0xFF + 0b1010 + 0o17", L"进制字面量，整数结果会同时显示十六/二/八进制"},
	{L"r := 3; pi * r^2", L"变量赋值，回车保存变量；ans 为上一次复制的结果"},
	{L"f(x) := x^2 + 2x + 1", L"自定义函数，回车保存后可直接调用 f(3)"},
	{L"deriv('x^3 + sin(x)', 2)", L"数值求导: deriv 一阶 / deriv2 二阶，自变量为 x"},
	{L"integ('x^2', 0, 3)", L"定积分: integ('表达式', 下限, 上限)"},
	{L"solve('x^2 = 2', 0, 5)", L"方程求根: solve('方程', 下限, 上限) 或 solve('cos(x) - x', 初值)"},
	{L"sigma('1/n^2', 1, 1000)", L"级数求和 sigma / 连乘 prod，变量可用 x n k i"},
	{L"gcd(12, 18, 24) + lcm(4, 6) + ncr(5, 2)", L"数论与组合: gcd lcm ncr npr fact gamma isprime nextprime fib"},
	{L"median(3, 1, 4, 1, 5) + stddev(2, 4, 4, 4, 5, 5, 7, 9)", L"统计: avg mean median variance stddev min max sum"},
	{L"band(0xF0, 0x3C) + shl(1, 8)", L"位运算: band bor bxor bnot shl shr"},
	{L"var s := 0; for (var i := 1; i <= 100; i += 1) { s += i; }; s", L"支持 var / for / while / if / switch 等语句"},
	{L"var v[5] := {5, 3, 9, 1, 7}; sort(v); return [v]", L"向量与 vecops: sort reverse dot count iota ..."},
	{L"rand() + randint(1, 6)", L"随机数: rand() 返回 [0,1)，randint(a, b) 返回整数"},
};

struct CalcFunctionUsage {
	const wchar_t* usage;
	const wchar_t* desc;
	// 回车填入输入框的内容
	const wchar_t* insert;
};

// 常用函数用法，回车填入函数名
inline const CalcFunctionUsage calcFunctionUsages[] = {
	{L"abs(x)  ceil(x)  floor(x)  round(x)  trunc(x)  frac(x)  sgn(x)", L"取整与符号", L"round("},
	{L"roundn(x, n)", L"保留 n 位小数，如 roundn(pi, 3) = 3.142", L"roundn("},
	{L"sqrt(x)  cbrt(x)  root(x, n)  x^y  pow(x, y)", L"开方与幂，也可写作 √2、x²、2**3", L"root("},
	{L"exp(x)  log(x)  log10(x)  log2(x)  logn(x, n)  log1p(x)", L"指数与对数，log 为自然对数，logn 为以 n 为底", L"logn("},
	{L"sin cos tan cot sec csc (x)", L"三角函数，单位由设置决定；30° 始终表示角度", L"sin("},
	{L"asin acos atan (x)  atan2(y, x)", L"反三角函数，结果使用当前角度单位", L"atan2("},
	{L"sinh cosh tanh (x)  deg2rad(x)  rad2deg(x)", L"双曲函数与角度换算", L"deg2rad("},
	{L"fact(n)  n!  gamma(x)  ncr(n, r)  npr(n, r)", L"阶乘、Γ 函数、组合数、排列数", L"ncr("},
	{L"gcd(a, b, ...)  lcm(a, b, ...)", L"最大公约数、最小公倍数，参数个数不限", L"gcd("},
	{L"isprime(n)  nextprime(n)  fib(n)", L"素数判断（1/0）、下一个素数、斐波那契数", L"isprime("},
	{L"avg mean median variance stddev min max sum mul (a, b, ...)", L"统计，variance / stddev 为样本方差 / 标准差", L"median("},
	{L"deriv('f(x)', x0)  deriv2('f(x)', x0)", L"在 x0 处的一阶 / 二阶数值导数", L"deriv('"},
	{L"integ('f(x)', a, b)", L"在 [a, b] 上的定积分（辛普森法）", L"integ('"},
	{L"solve('方程', a, b)  solve('f(x)', x0)", L"在区间内二分求根，或从 x0 开始牛顿迭代；方程可写 'x^2 = 2'", L"solve('"},
	{L"sigma('f(n)', a, b)  prod('f(n)', a, b)", L"整数 a 到 b 的求和 / 连乘，变量可用 x n k i", L"sigma('"},
	{L"band bor bxor (a, b)  bnot(a)  shl shr (a, n)", L"整数位运算，可配合 0x / 0b / 0o 字面量", L"band("},
	{L"rand()  randint(a, b)", L"[0, 1) 随机小数、[a, b] 随机整数", L"randint("},
	{L"if(条件, a, b)  条件 ? a : b  clamp(lo, x, hi)  inrange(lo, x, hi)", L"条件与范围判断", L"clamp("},
	{L"erf(x)  erfc(x)  ncdf(x)  hypot(x, y)", L"误差函数、标准正态分布函数、斜边长", L"ncdf("},
	{L"pi  e  tau  phi  inf  ans", L"常量，ans 为上一次复制的结果", L"ans"},
	{L"x := 5    f(x) := x^2 + 1", L"定义变量 / 自定义函数，回车保存", L"f(x) := "},
};

enum CalcHelpSection : int8_t {
	kSectionFunctions = 0,
	kSectionExamples,
	kSectionVariables,
	kSectionCount,
};

struct AngleUnitInfo {
	calc::AngleUnit unit;
	const char* settingValue;
	const wchar_t* name;
};

// 与设置项 com.candytek.calc.trigonometric.units 的 entryValues 顺序一致，回车按此顺序循环切换
inline const AngleUnitInfo angleUnitInfos[] = {
	{calc::AngleUnit::Radian, "radian", L"弧度"},
	{calc::AngleUnit::Degree, "degree", L"度"},
	{calc::AngleUnit::Gradian, "gradian", L"百分度"},
};

inline constexpr char kAngleUnitSettingKey[] = "com.candytek.calc.trigonometric.units";

struct CalcHistoryItem {
	std::wstring expr;
	std::wstring result;
};

class CalcPlugin : public IPlugin {
private:
	static constexpr size_t kMaxHistory = 200;

	calc::CalcEngine engine;
	int iconIndex = -1;
	bool expandedSections[kSectionCount] = {};
	// 本次启动以来的计算历史，最新的在前
	std::vector<CalcHistoryItem> history;

	void AddHistory(const std::wstring& expr, const std::wstring& result) {
		if (!isShowHistory || expr.find_first_not_of(L" \t") == std::wstring::npos) return;
		history.erase(std::remove_if(history.begin(), history.end(), [&](const CalcHistoryItem& item) {
			return item.expr == expr && item.result == result;
		}), history.end());
		history.insert(history.begin(), {expr, result});
		if (history.size() > kMaxHistory) history.resize(kMaxHistory);
	}

	std::shared_ptr<CalcAction> MakeAction(std::wstring title, std::wstring subTitle, const CalcAction::Kind kind) const {
		return std::make_shared<CalcAction>(std::move(title), std::move(subTitle), kind, iconIndex);
	}

	std::shared_ptr<CalcAction> MakeCopyAction(std::wstring title, std::wstring subTitle, std::wstring copyText,
												const double value) const {
		auto action = MakeAction(std::move(title), std::move(subTitle), CalcAction::Kind::Copy);
		action->copyText = std::move(copyText);
		action->value = value;
		return action;
	}

	// 整数显示进制，小数显示分数，大数显示千分位、科学计数法
	void AppendExtraFormats(std::vector<std::shared_ptr<BaseAction>>& list, const double v) const {
		if (!isShowExtraFormats || !std::isfinite(v)) return;
		const double absV = std::fabs(v);

		if (int64_t iv; calc::ToInt64(v, iv) && absV >= 2) {
			const std::wstring hex = calc::FormatRadix(iv, 16);
			list.push_back(MakeCopyAction(hex, L"十六进制 · 回车复制", hex, v));
			if (absV < 4294967296.0) {
				const std::wstring bin = calc::FormatRadix(iv, 2);
				list.push_back(MakeCopyAction(bin, L"二进制 · 回车复制", bin, v));
			}
			const std::wstring oct = calc::FormatRadix(iv, 8);
			list.push_back(MakeCopyAction(oct, L"八进制 · 回车复制", oct, v));
		}
		if (int64_t num, den; calc::ToFraction(v, num, den)) {
			const std::wstring frac = std::to_wstring(num) + L"/" + std::to_wstring(den);
			list.push_back(MakeCopyAction(frac, L"分数 · 回车复制", frac, v));
		}
		if (absV >= 10000 && absV < 1e15) {
			list.push_back(MakeCopyAction(calc::FormatThousands(v), L"千分位 · 回车复制", calc::FormatThousands(v), v));
		}
		if (absV >= 1e6 || (absV < 1e-3 && absV > 0)) {
			const std::wstring sci = calc::FormatScientific(v);
			list.push_back(MakeCopyAction(sci, L"科学计数法 · 回车复制", sci, v));
		}
	}

	std::vector<std::shared_ptr<BaseAction>> BuildResultActions(const calc::CalcEngine::Result& r,
																const std::wstring& expr) const {
		using Kind = calc::CalcEngine::Result::Kind;
		std::vector<std::shared_ptr<BaseAction>> list;
		switch (r.kind) {
		case Kind::Value:
			list.push_back(MakeCopyAction(calc::FormatNumber(r.value), L"计算结果 · 回车复制", calc::FormatPlain(r.value), r.value));
			AppendExtraFormats(list, r.value);
			break;
		case Kind::Text:
			list.push_back(MakeCopyAction(r.text, L"计算结果 · 回车复制", r.text, r.value));
			break;
		case Kind::Assignment: {
			auto action = MakeAction(calc::FormatNumber(r.value), L"回车保存变量: " + r.detail, CalcAction::Kind::Commit);
			action->source = expr;
			action->value = r.value;
			list.push_back(action);
			break;
		}
		case Kind::FunctionDef: {
			auto action = MakeAction(r.text, L"回车保存自定义函数", CalcAction::Kind::Commit);
			action->source = expr;
			list.push_back(action);
			break;
		}
		case Kind::Error:
			list.push_back(MakeAction(L"表达式有误", r.text, CalcAction::Kind::None));
			break;
		default:
			break;
		}
		if (isShowAngleUnitAction && r.kind != Kind::Error && calc::CalcEngine::UsesAngleFunction(expr)) {
			list.push_back(MakeAngleUnitAction());
		}
		for (const auto& item : list) {
			const auto action = std::static_pointer_cast<CalcAction>(item);
			if (action->kind == CalcAction::Kind::Copy) action->source = expr;
		}
		return list;
	}

	size_t CurrentAngleUnitIndex() const {
		const calc::AngleUnit unit = engine.GetAngleUnit();
		for (size_t i = 0; i < std::size(angleUnitInfos); ++i) {
			if (angleUnitInfos[i].unit == unit) return i;
		}
		return 0;
	}

	std::shared_ptr<CalcAction> MakeAngleUnitAction() const {
		const size_t index = CurrentAngleUnitIndex();
		const AngleUnitInfo& next = angleUnitInfos[(index + 1) % std::size(angleUnitInfos)];
		return MakeAction(L"三角学单位: " + std::wstring(angleUnitInfos[index].name),
						L"回车切换为" + std::wstring(next.name) + L" · 30° 始终表示角度",
						CalcAction::Kind::SwitchAngleUnit);
	}

	// 切换到下一个角度单位，并保存到用户设置
	void SwitchToNextAngleUnit() {
		const AngleUnitInfo& next = angleUnitInfos[(CurrentAngleUnitIndex() + 1) % std::size(angleUnitInfos)];
		engine.SetAngleUnit(next.unit);
		if (!m_host->SetPluginSettingValue(m_pluginId, kAngleUnitSettingKey, next.settingValue)) {
			Loge(L"CalcPlugin", L"save angle unit setting failed");
		}
	}

	std::shared_ptr<CalcAction> MakeSectionAction(const CalcHelpSection section, const std::wstring& title,
												const std::wstring& summary) const {
		const bool expanded = expandedSections[section];
		auto action = MakeAction((expanded ? L"▼ " : L"▶ ") + title,
								L"回车" + std::wstring(expanded ? L"收起" : L"展开") + L" · " + summary,
								CalcAction::Kind::ToggleHelp);
		action->section = section;
		return action;
	}

	// 只输入前缀时展示：函数用法、计算示例、当前变量三个可展开分组 + 本次启动以来的计算历史
	std::vector<std::shared_ptr<BaseAction>> BuildHelpActions() const {
		std::vector<std::shared_ptr<BaseAction>> list;

		if (isShowFunctionsSection) {
			list.push_back(MakeSectionAction(kSectionFunctions, L"函数用法",
											std::to_wstring(std::size(calcFunctionUsages)) + L" 组常用函数"));
			if (expandedSections[kSectionFunctions]) {
				for (const auto& usage : calcFunctionUsages) {
					auto action = MakeAction(usage.usage, usage.desc, CalcAction::Kind::Fill);
					action->fillText = startStr + usage.insert;
					list.push_back(action);
				}
			}
		}

		if (isShowExamplesSection) {
			list.push_back(MakeSectionAction(kSectionExamples, L"计算示例",
											std::to_wstring(std::size(calcExamples)) + L" 个示例，回车填入"));
			if (expandedSections[kSectionExamples]) {
				for (const auto& example : calcExamples) {
					auto action = MakeAction(example.expr, example.desc, CalcAction::Kind::Fill);
					action->fillText = startStr + example.expr;
					list.push_back(action);
				}
			}
		}

		if (isShowVariablesSection) {
			const size_t variableCount = engine.GetVariables().size();
			const size_t functionCount = engine.GetFunctions().size();
			list.push_back(MakeSectionAction(kSectionVariables, L"当前变量",
											std::to_wstring(variableCount) + L" 个变量、" + std::to_wstring(functionCount) + L" 个自定义函数"));
			if (expandedSections[kSectionVariables]) AppendVariableActions(list);
		}

		if (isShowAngleUnitAction) list.push_back(MakeAngleUnitAction());

		if (!isShowHistory) return list;
		for (const auto& item : history) {
			auto action = MakeAction(item.expr + L" = " + item.result, L"计算历史 · 回车填入 · 可拖拽表达式", CalcAction::Kind::History);
			action->source = item.expr;
			action->fillText = startStr + item.expr;
			list.push_back(action);
		}
		if (!history.empty()) {
			list.push_back(MakeAction(L"清除所有计算历史", L"共 " + std::to_wstring(history.size()) + L" 条，回车清除",
									CalcAction::Kind::ClearHistory));
		}
		return list;
	}

	void AppendVariableActions(std::vector<std::shared_ptr<BaseAction>>& list) const {
		const size_t symbolStart = list.size();
		for (const auto& [name, value] : engine.GetVariables()) {
			auto action = MakeAction(utf8_to_wide(name) + L" = " + calc::FormatNumber(value), L"变量 · 回车插入", CalcAction::Kind::Fill);
			action->fillText = startStr + utf8_to_wide(name);
			list.push_back(action);
		}
		for (const auto& f : engine.GetFunctions()) {
			auto action = MakeAction(f.Signature(), L"自定义函数 · 回车插入", CalcAction::Kind::Fill);
			action->fillText = startStr + utf8_to_wide(f.name) + L"(";
			list.push_back(action);
		}
		if (list.size() - symbolStart > 1) {
			list.push_back(MakeAction(L"清除所有自定义变量和函数", L"ans 会重置为 0", CalcAction::Kind::ClearSymbols));
		}
	}

	// 无前缀模式下，只拦截看起来像算式的输入，避免影响正常搜索
	static bool LooksLikeMath(const std::wstring& input) {
		bool hasDigit = false, hasOperator = false;
		for (const wchar_t c : input) {
			if (iswdigit(c)) hasDigit = true;
			else if (wcschr(L"+-*/^%()!×÷√", c)) hasOperator = true;
		}
		return hasDigit && hasOperator;
	}

	// 以 “=” 结尾时用计算结果替换输入，排除 == := <= >= != 等运算符
	static bool EndsWithReplaceMark(const std::wstring& input) {
		if (!EndsWith(input, L"=") || input.size() < 2) return false;
		const wchar_t prev = input[input.size() - 2];
		return !wcschr(L"=:<>!", prev);
	}

	static std::wstring EvaluateLatex(const std::wstring& expr) {
		static ImprovedLaTeXCalculator calculator;
		const std::pair<bool, std::string> result = calculator.evaluate(wide_to_utf8(expr));
		if (!result.first) return L"Error: " + utf8_to_wide(result.second);
		try {
			return calc::FormatNumber(std::stod(result.second));
		} catch (const std::exception& e) {
			return L"Invalid result format: " + utf8_to_wide(e.what());
		}
	}

public:
	CalcPlugin() = default;
	~CalcPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"计算器";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.calc";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.1.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"科学计算器，支持变量、自定义函数、求导积分、方程求根、进制转换等";
	}


	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.calc.start_str",
			"title": "直接激活命令（留空则自动识别输入的算式）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "="
		},
		{
			"key": "com.candytek.calc.autoreplace",
			"title": "如果查询以 “=” 结尾，则替换输入",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.show_formats",
			"title": "显示其他格式的结果（进制、分数、千分位、科学计数法）",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.show_functions_section",
			"title": "只输入前缀时显示 “函数用法” 分组",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.show_examples_section",
			"title": "只输入前缀时显示 “计算示例” 分组",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.show_variables_section",
			"title": "只输入前缀时显示 “当前变量” 分组",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.show_angle_unit_action",
			"title": "显示切换三角学单位的选项",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.show_history",
			"title": "记录并显示计算历史",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.calc.trigonometric.units",
			"title": "三角学单位",
			"type": "list",
			"subPage": "plugin",
			"entries": [
				"弧度",
				"度",
				"百分度"
			],
			"entryValues": [
			    "radian",
			    "degree",
			    "gradian"
			],
			"defValue": "radian"
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		const auto& settings = m_host->GetSettingsMap();
		startStr = utf8_to_wide(settings.at("com.candytek.calc.start_str").stringValue);
		isDetectReplace = settings.at("com.candytek.calc.autoreplace").boolValue;
		const auto readBool = [&settings](const char* key, const bool defValue) {
			const auto it = settings.find(key);
			return it != settings.end() ? it->second.boolValue : defValue;
		};
		isShowExtraFormats = readBool("com.candytek.calc.show_formats", true);
		isShowFunctionsSection = readBool("com.candytek.calc.show_functions_section", true);
		isShowExamplesSection = readBool("com.candytek.calc.show_examples_section", true);
		isShowVariablesSection = readBool("com.candytek.calc.show_variables_section", true);
		isShowAngleUnitAction = readBool("com.candytek.calc.show_angle_unit_action", true);
		isShowHistory = readBool("com.candytek.calc.show_history", true);
		if (!isShowHistory) history.clear();
		calc::AngleUnit unit = calc::AngleUnit::Radian;
		if (const auto it = settings.find(kAngleUnitSettingKey); it != settings.end()) {
			for (const auto& info : angleUnitInfos) {
				if (it->second.stringValue == info.settingValue) unit = info.unit;
			}
		}
		engine.SetAngleUnit(unit);
	}


	void RefreshAllActions() override {
		if (!m_host) return;

		iconIndex = GetSysImageIndex(CalcAction().getIconFilePath());
		allPluginActions.clear();
		auto action = MakeAction(L"计算器", L"输入 " + startStr + L" 开始计算", CalcAction::Kind::Fill);
		action->fillText = startStr;
		allPluginActions.push_back(action);
	}

	void Shutdown() override {
		allPluginActions.clear();
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!m_host || startStr.empty()) return {};
		return allPluginActions;
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (StartsWith(input, L"latex ")) {
			const std::wstring result = EvaluateLatex(input.substr(6));
			return {MakeCopyAction(result, L"LaTeX 计算结果 · 回车复制", result, calc::kNaN)};
		}

		const bool isGlobalMode = startStr.empty();
		if (!isGlobalMode && !StartsWith(input, startStr)) return {};
		if (isGlobalMode && !LooksLikeMath(input)) return {};

		std::wstring expr = input.substr(startStr.size());
		if (!isGlobalMode && expr.find_first_not_of(L" \t") == std::wstring::npos) {
			return BuildHelpActions();
		}

		if (isDetectReplace && EndsWithReplaceMark(expr)) {
			const auto r = engine.Evaluate(expr.substr(0, expr.size() - 1), false);
			if (r.kind == calc::CalcEngine::Result::Kind::Value && !std::isnan(r.value)) {
				engine.SetAns(r.value);
				AddHistory(expr.substr(0, expr.size() - 1), calc::FormatNumber(r.value));
				m_host->ChangeEditTextText(startStr + calc::FormatPlain(r.value));
				return {};
			}
		}

		const auto r = engine.Evaluate(expr, !isGlobalMode);
		if (isGlobalMode) {
			// 自动识别模式下只展示有效的数值结果
			if (r.kind != calc::CalcEngine::Result::Kind::Value || !std::isfinite(r.value)) return {};
			// 纯数字（如 "-5"、"(3)"）无需展示
			if (calc::FormatPlain(r.value) == expr) return {};
		}
		return BuildResultActions(r, expr);
	}

	void OnMainWindowShow(const bool isShow) override {
		if (!isShow) std::fill(std::begin(expandedSections), std::end(expandedSections), false);
	}

	// 拖拽历史记录时提供表达式文本
	bool OnItemBeginDrag(const std::shared_ptr<BaseAction>& action, HWND sourceHwnd, POINT screenPt) override {
		const auto calcAction = std::dynamic_pointer_cast<CalcAction>(action);
		if (!m_host || !calcAction || calcAction->kind != CalcAction::Kind::History || calcAction->source.empty()) {
			return false;
		}
		OleDragDropData data;
		data.text = calcAction->source;
		return m_host->BeginOleDragDropData(data, sourceHwnd);
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		const auto calcAction = std::dynamic_pointer_cast<CalcAction>(action);
		if (!calcAction) return false;

		switch (calcAction->kind) {
		case CalcAction::Kind::Copy:
			if (std::isfinite(calcAction->value)) engine.SetAns(calcAction->value);
			AddHistory(calcAction->source, calcAction->title);
			CopyTextToClipboard(nullptr, calcAction->copyText);
			return true;
		case CalcAction::Kind::Commit: {
			// 重新计算一次，保证保存的是当前输入
			const auto r = engine.Evaluate(calcAction->source);
			engine.CommitPending();
			if (r.kind == calc::CalcEngine::Result::Kind::Assignment) engine.SetAns(r.value);
			AddHistory(calcAction->source, calcAction->title);
			m_host->ChangeEditTextText(startStr);
			return false;
		}
		case CalcAction::Kind::Fill:
		case CalcAction::Kind::History:
			m_host->ChangeEditTextText(calcAction->fillText);
			return false;
		case CalcAction::Kind::ToggleHelp: {
			if (calcAction->section >= 0 && calcAction->section < kSectionCount) {
				expandedSections[calcAction->section] = !expandedSections[calcAction->section];
			}
			auto list = BuildHelpActions();
			m_host->ShowResultsDerectly(list);
			return false;
		}
		case CalcAction::Kind::ClearSymbols:
			engine.ClearUserSymbols();
			m_host->ChangeEditTextText(startStr);
			return false;
		case CalcAction::Kind::ClearHistory: {
			history.clear();
			auto list = BuildHelpActions();
			m_host->ShowResultsDerectly(list);
			return false;
		}
		case CalcAction::Kind::SwitchAngleUnit: {
			SwitchToNextAngleUnit();
			// 用新单位重新计算当前输入，刷新结果和切换项的文字
			auto list = InterceptInputShowResultsDirectly(m_host->GetEditTextText());
			m_host->ShowResultsDerectly(list);
			return false;
		}
		default:
			return false;
		}
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new CalcPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
