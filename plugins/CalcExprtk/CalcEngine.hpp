#pragma once

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <limits>
#include <numeric>
#include <random>
#include <regex>
#include <string>
#include <vector>

#include "exprtk.hpp"
#include "util/StringUtil.hpp"

// 基于 exprtk 的计算引擎：
// - 变量 (x := 5 / x = 5，回车后保存)、ans 上次结果
// - 自定义函数 (f(x) := x^2 + 1)
// - 微积分: deriv / deriv2 / integ / solve / sigma / prod
// - 数论/组合: fact gcd lcm ncr npr isprime nextprime fib
// - 统计: mean median variance stddev
// - 位运算: band bor bxor bnot shl shr
// - 0x / 0b / 0o 字面量、阶乘后缀 n!、全角符号及 × ÷ π √ ° 等输入
// - exprtk 原生语句 (var / for / while / if / return [...]) 与 vecops 向量函数
namespace calc {
	using symbol_table_t = exprtk::symbol_table<double>;
	using expression_t = exprtk::expression<double>;
	using parser_t = exprtk::parser<double>;
	using compositor_t = exprtk::function_compositor<double>;

	constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
	constexpr double kInf = std::numeric_limits<double>::infinity();
	constexpr double kPi = 3.141592653589793238462643383279;
	constexpr double kMaxExactInt = 9007199254740992.0; // 2^53

	enum class AngleUnit { Radian, Degree, Gradian };

	// 当前角度单位 -> 弧度 的换算系数
	inline double g_angleFactor = 1.0;
	// 当前角度单位下的直角大小，弧度模式为 0
	inline double g_quarterTurn = 0.0;
	// 1° 在当前角度单位下的值，用于 30° 这种写法
	inline double g_degreeUnit = 1.0;

	inline bool IsInteger(const double v) {
		return std::isfinite(v) && std::floor(v) == v;
	}

	inline bool ToInt64(const double v, int64_t& out) {
		if (!IsInteger(v) || std::fabs(v) > kMaxExactInt) return false;
		out = static_cast<int64_t>(v);
		return true;
	}

	namespace fn {
		// 角度为直角整数倍时给出精确值，避免 sin(180) = 1.2e-16 这类误差
		inline bool QuarterTurn(const double x, int& q) {
			if (g_quarterTurn == 0.0) return false;
			const double k = x / g_quarterTurn;
			if (!IsInteger(k) || std::fabs(k) > 1e15) return false;
			q = static_cast<int>(std::fmod(k, 4.0));
			if (q < 0) q += 4;
			return true;
		}

		inline double SinU(const double x) {
			static const double table[] = {0, 1, 0, -1};
			if (int q; QuarterTurn(x, q)) return table[q];
			return std::sin(x * g_angleFactor);
		}

		inline double CosU(const double x) {
			static const double table[] = {1, 0, -1, 0};
			if (int q; QuarterTurn(x, q)) return table[q];
			return std::cos(x * g_angleFactor);
		}

		inline double TanU(const double x) {
			if (int q; QuarterTurn(x, q)) return q % 2 == 0 ? 0.0 : kNaN;
			return std::tan(x * g_angleFactor);
		}

		inline double CotU(const double x) {
			if (int q; QuarterTurn(x, q)) return q % 2 == 1 ? 0.0 : kNaN;
			return 1.0 / std::tan(x * g_angleFactor);
		}

		inline double SecU(const double x) {
			const double c = CosU(x);
			return c == 0 ? kNaN : 1.0 / c;
		}

		inline double CscU(const double x) {
			const double s = SinU(x);
			return s == 0 ? kNaN : 1.0 / s;
		}

		inline double AsinU(const double x) { return std::asin(x) / g_angleFactor; }
		inline double AcosU(const double x) { return std::acos(x) / g_angleFactor; }
		inline double AtanU(const double x) { return std::atan(x) / g_angleFactor; }
		inline double Atan2U(const double y, const double x) { return std::atan2(y, x) / g_angleFactor; }

		inline double Cbrt(const double x) { return std::cbrt(x); }
		inline double Gamma(const double x) { return std::tgamma(x); }

		inline double Fact(const double n) {
			if (std::isnan(n) || (n < 0 && IsInteger(n))) return kNaN;
			if (IsInteger(n) && n <= 170) {
				double r = 1;
				for (int i = 2; i <= static_cast<int>(n); ++i) r *= i;
				return r;
			}
			return std::tgamma(n + 1);
		}

		inline double LogChoose(const double n, const double r) {
			return std::lgamma(n + 1) - std::lgamma(r + 1) - std::lgamma(n - r + 1);
		}

		inline double Ncr(const double n, const double r) {
			int64_t ni, ri;
			if (!ToInt64(n, ni) || !ToInt64(r, ri) || ni < 0 || ri < 0) return kNaN;
			if (ri > ni) return 0;
			ri = (std::min)(ri, ni - ri);
			if (ri > 1000000) return std::round(std::exp(LogChoose(n, static_cast<double>(ri))));
			double res = 1;
			for (int64_t i = 1; i <= ri; ++i) {
				res = res * static_cast<double>(ni - ri + i) / static_cast<double>(i);
				if (!std::isfinite(res)) return kInf;
			}
			return std::round(res);
		}

		inline double Npr(const double n, const double r) {
			int64_t ni, ri;
			if (!ToInt64(n, ni) || !ToInt64(r, ri) || ni < 0 || ri < 0) return kNaN;
			if (ri > ni) return 0;
			if (ri > 1000000) return std::round(std::exp(std::lgamma(n + 1) - std::lgamma(n - r + 1)));
			double res = 1;
			for (int64_t i = 0; i < ri; ++i) {
				res *= static_cast<double>(ni - i);
				if (!std::isfinite(res)) return kInf;
			}
			return res;
		}

		inline uint64_t MulMod(uint64_t a, uint64_t b, const uint64_t m) {
			uint64_t r = 0;
			a %= m;
			while (b) {
				if (b & 1) r = (r >= m - a) ? r - (m - a) : r + a;
				a = (a >= m - a) ? a - (m - a) : a + a;
				b >>= 1;
			}
			return r;
		}

		inline uint64_t PowMod(uint64_t b, uint64_t e, const uint64_t m) {
			uint64_t r = 1 % m;
			b %= m;
			while (e) {
				if (e & 1) r = MulMod(r, b, m);
				b = MulMod(b, b, m);
				e >>= 1;
			}
			return r;
		}

		// 确定性 Miller-Rabin，对 64 位整数均正确
		inline bool IsPrimeU64(const uint64_t n) {
			if (n < 2) return false;
			static const uint64_t bases[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
			for (const uint64_t p : bases) {
				if (n % p == 0) return n == p;
			}
			uint64_t d = n - 1;
			int s = 0;
			while ((d & 1) == 0) {
				d >>= 1;
				++s;
			}
			for (const uint64_t a : bases) {
				uint64_t x = PowMod(a, d, n);
				if (x == 1 || x == n - 1) continue;
				bool composite = true;
				for (int i = 1; i < s; ++i) {
					x = MulMod(x, x, n);
					if (x == n - 1) {
						composite = false;
						break;
					}
				}
				if (composite) return false;
			}
			return true;
		}

		inline double IsPrime(const double n) {
			int64_t v;
			if (!ToInt64(n, v) || v < 2) return 0;
			return IsPrimeU64(static_cast<uint64_t>(v)) ? 1 : 0;
		}

		inline double NextPrime(const double n) {
			int64_t v;
			if (!ToInt64(std::floor(n), v)) return kNaN;
			v = (std::max)(v + 1, static_cast<int64_t>(2));
			while (!IsPrimeU64(static_cast<uint64_t>(v))) {
				if (static_cast<double>(++v) > kMaxExactInt) return kNaN;
			}
			return static_cast<double>(v);
		}

		inline double Fib(const double n) {
			int64_t v;
			if (!ToInt64(n, v) || v < 0) return kNaN;
			if (v > 1476) return kInf;
			double a = 0, b = 1;
			for (int64_t i = 0; i < v; ++i) {
				const double t = a + b;
				a = b;
				b = t;
			}
			return a;
		}

		inline double RandUniform() {
			static std::mt19937_64 engine{std::random_device{}()};
			static std::uniform_real_distribution<double> dist(0.0, 1.0);
			return dist(engine);
		}

		inline double RandInt(const double a, const double b) {
			int64_t lo, hi;
			if (!ToInt64(std::ceil((std::min)(a, b)), lo) || !ToInt64(std::floor((std::max)(a, b)), hi) || lo > hi) return kNaN;
			return static_cast<double>(lo + static_cast<int64_t>(RandUniform() * static_cast<double>(hi - lo + 1)));
		}

		// 兼容旧版本: ranint() 返回 0~100 的整数
		inline double RanInt() { return RandInt(0, 100); }

		inline double BitAnd(const double a, const double b) {
			int64_t x, y;
			if (!ToInt64(a, x) || !ToInt64(b, y)) return kNaN;
			return static_cast<double>(x & y);
		}

		inline double BitOr(const double a, const double b) {
			int64_t x, y;
			if (!ToInt64(a, x) || !ToInt64(b, y)) return kNaN;
			return static_cast<double>(x | y);
		}

		inline double BitXor(const double a, const double b) {
			int64_t x, y;
			if (!ToInt64(a, x) || !ToInt64(b, y)) return kNaN;
			return static_cast<double>(x ^ y);
		}

		inline double BitNot(const double a) {
			int64_t x;
			if (!ToInt64(a, x)) return kNaN;
			return static_cast<double>(~x);
		}

		inline double Shl(const double a, const double n) {
			int64_t x, s;
			if (!ToInt64(a, x) || !ToInt64(n, s) || s < 0 || s > 63) return kNaN;
			return static_cast<double>(static_cast<int64_t>(static_cast<uint64_t>(x) << s));
		}

		inline double Shr(const double a, const double n) {
			int64_t x, s;
			if (!ToInt64(a, x) || !ToInt64(n, s) || s < 0 || s > 63) return kNaN;
			return static_cast<double>(x >> s);
		}

		inline double Gcd(const std::vector<double>& args) {
			int64_t g = 0;
			for (const double v : args) {
				int64_t x;
				if (!ToInt64(v, x)) return kNaN;
				g = std::gcd(g, x);
			}
			return static_cast<double>(g);
		}

		inline double Lcm(const std::vector<double>& args) {
			double l = 1;
			for (const double v : args) {
				int64_t x;
				if (!ToInt64(v, x)) return kNaN;
				if (x == 0) return 0;
				int64_t cur;
				if (!ToInt64(l, cur)) return kNaN;
				l = static_cast<double>(cur / std::gcd(cur, x)) * static_cast<double>(x < 0 ? -x : x);
			}
			return l;
		}

		inline double Mean(const std::vector<double>& args) {
			if (args.empty()) return kNaN;
			return std::accumulate(args.begin(), args.end(), 0.0) / static_cast<double>(args.size());
		}

		inline double Median(const std::vector<double>& args) {
			if (args.empty()) return kNaN;
			std::vector<double> v = args;
			std::sort(v.begin(), v.end());
			const size_t n = v.size();
			return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
		}

		// 样本方差 (n - 1)
		inline double Variance(const std::vector<double>& args) {
			if (args.size() < 2) return kNaN;
			const double mean = Mean(args);
			double sum = 0;
			for (const double v : args) sum += (v - mean) * (v - mean);
			return sum / static_cast<double>(args.size() - 1);
		}

		inline double Stddev(const std::vector<double>& args) {
			return std::sqrt(Variance(args));
		}
	}

	struct VarargFunction final : exprtk::ivararg_function<double> {
		using Impl = double (*)(const std::vector<double>&);

		explicit VarargFunction(const Impl impl) : impl_(impl) {
		}

		double operator()(const std::vector<double>& args) override {
			return impl_(args);
		}

	private:
		Impl impl_;
	};

	// ---------------------------------------------------------------------
	// 格式化
	// ---------------------------------------------------------------------

	// 用于显示
	inline std::wstring FormatNumber(const double v) {
		if (std::isnan(v)) return L"NaN";
		if (std::isinf(v)) return v > 0 ? L"∞" : L"-∞";
		if (v == 0) return L"0";
		wchar_t buf[64];
		swprintf(buf, 64, L"%.15g", v);
		return buf;
	}

	// 用于复制或回填输入框，保证能再次被解析
	inline std::wstring FormatPlain(const double v) {
		if (std::isinf(v)) return v > 0 ? L"inf" : L"-inf";
		return FormatNumber(v);
	}

	inline std::wstring FormatRadix(const int64_t value, const int base) {
		const wchar_t* prefix = base == 16 ? L"0x" : base == 8 ? L"0o" : L"0b";
		uint64_t v = value < 0 ? static_cast<uint64_t>(0) - static_cast<uint64_t>(value) : static_cast<uint64_t>(value);
		std::wstring digits;
		do {
			digits.push_back(L"0123456789ABCDEF"[v % base]);
			v /= base;
		} while (v);
		std::reverse(digits.begin(), digits.end());
		return (value < 0 ? L"-" : L"") + std::wstring(prefix) + digits;
	}

	inline std::wstring FormatThousands(const double v) {
		std::wstring s = FormatNumber(v);
		if (!std::isfinite(v) || s.find(L'e') != std::wstring::npos) return s;
		const size_t start = s[0] == L'-' ? 1 : 0;
		size_t end = s.find(L'.');
		if (end == std::wstring::npos) end = s.size();
		for (size_t i = end; i > start + 3; i -= 3) s.insert(i - 3, 1, L',');
		return s;
	}

	// 1.2345e7 形式
	inline std::wstring FormatScientific(const double v) {
		if (!std::isfinite(v)) return FormatNumber(v);
		wchar_t buf[64];
		swprintf(buf, 64, L"%.14e", v);
		std::wstring s = buf;
		const size_t ePos = s.find(L'e');
		std::wstring mantissa = s.substr(0, ePos);
		if (mantissa.find(L'.') != std::wstring::npos) {
			mantissa.erase(mantissa.find_last_not_of(L'0') + 1);
			if (mantissa.back() == L'.') mantissa.pop_back();
		}
		const int exponent = std::stoi(s.substr(ePos + 1));
		return mantissa + L"e" + std::to_wstring(exponent);
	}

	// 连分数逼近，分母不超过 maxDen 且误差极小时返回 true
	inline bool ToFraction(const double v, int64_t& num, int64_t& den, const int64_t maxDen = 1000000) {
		if (!std::isfinite(v) || IsInteger(v) || std::fabs(v) > 1e12) return false;
		int64_t h0 = 0, h1 = 1, k0 = 1, k1 = 0;
		double x = v;
		for (int i = 0; i < 64; ++i) {
			const double a = std::floor(x);
			if (std::fabs(a) > 1e15) break;
			const int64_t ai = static_cast<int64_t>(a);
			const int64_t h2 = ai * h1 + h0;
			const int64_t k2 = ai * k1 + k0;
			if (k2 > maxDen) break;
			h0 = h1;
			h1 = h2;
			k0 = k1;
			k1 = k2;
			if (std::fabs(v - static_cast<double>(h1) / static_cast<double>(k1)) <= 8 * DBL_EPSILON * std::fabs(v)) {
				num = h1;
				den = k1;
				return den > 1;
			}
			const double frac = x - a;
			if (frac == 0) break;
			x = 1.0 / frac;
		}
		return false;
	}

	// ---------------------------------------------------------------------
	// 计算引擎
	// ---------------------------------------------------------------------
	class CalcEngine {
	public:
		struct Result {
			enum class Kind { Empty, Value, Text, Assignment, FunctionDef, Error };

			Kind kind = Kind::Empty;
			double value = kNaN;
			// Text: 结果文本；FunctionDef: 函数签名；Error: 错误信息
			std::wstring text;
			// Assignment: 本次将被保存的变量，如 "x = 5"
			std::wstring detail;
		};

		struct UserFunction {
			std::string name;
			std::vector<std::string> params;
			// 已预处理，用于编译
			std::string body;
			// 用户原始输入，用于显示
			std::string displayBody;

			std::wstring Signature() const {
				std::string s = name + "(";
				for (size_t i = 0; i < params.size(); ++i) s += (i ? ", " : "") + params[i];
				return utf8_to_wide(s + ") = " + displayBody);
			}
		};

		CalcEngine() {
			SetAngleUnit(AngleUnit::Radian);
			Setup();
		}

		CalcEngine(const CalcEngine&) = delete;
		CalcEngine& operator=(const CalcEngine&) = delete;

		~CalcEngine() {
			expression_.release();
			compositor_.clear();
		}

		void SetAngleUnit(const AngleUnit unit) {
			angleUnit_ = unit;
			switch (unit) {
			case AngleUnit::Degree:
				g_angleFactor = kPi / 180.0;
				g_quarterTurn = 90.0;
				break;
			case AngleUnit::Gradian:
				g_angleFactor = kPi / 200.0;
				g_quarterTurn = 100.0;
				break;
			default:
				g_angleFactor = 1.0;
				g_quarterTurn = 0.0;
				break;
			}
			g_degreeUnit = (kPi / 180.0) / g_angleFactor;
			degreeUnit_ = g_degreeUnit;
		}

		AngleUnit GetAngleUnit() const {
			return angleUnit_;
		}

		// 表达式中是否调用了受角度单位影响的三角函数
		static bool UsesAngleFunction(const std::wstring& expr) {
			const size_t n = expr.size();
			size_t i = 0;
			while (i < n) {
				if (expr[i] < 128 && IsIdentStart(static_cast<char>(expr[i]))) {
					size_t j = i + 1;
					while (j < n && expr[j] < 128 && IsIdentChar(static_cast<char>(expr[j]))) ++j;
					std::string ident;
					for (size_t c = i; c < j; ++c) ident += static_cast<char>(expr[c]);
					size_t k = j;
					while (k < n && expr[k] == L' ') ++k;
					if (k < n && expr[k] == L'(' && IsAngleFunction(ident)) return true;
					i = j;
				} else {
					++i;
				}
			}
			return false;
		}

		// 计算表达式，不会修改已保存的变量；赋值和函数定义需调用 CommitPending 保存。
		// allowDefine 为 false 时禁止赋值、自定义函数（用于无前缀的自动识别模式）
		Result Evaluate(const std::wstring& input, const bool allowDefine = true) {
			pendingVars_.clear();
			pendingFunction_ = {};
			hasPendingFunction_ = false;

			std::string src = NormalizeToAscii(input);
			Trim(src);
			if (src.empty()) return {};

			if (allowDefine) {
				Result r;
				if (TryParseFunctionDefinition(src, r)) return r;
				src = ConvertSimpleAssignment(src);
			}
			return EvaluateExpression(Preprocess(src), allowDefine);
		}

		// 保存最近一次 Evaluate 中的变量赋值、函数定义
		bool CommitPending() {
			bool changed = false;
			if (hasPendingFunction_) {
				const auto it = std::find_if(functions_.begin(), functions_.end(), [&](const UserFunction& f) {
					return EqualsIgnoreCase(f.name, pendingFunction_.name);
				});
				if (it != functions_.end()) *it = pendingFunction_;
				else functions_.push_back(pendingFunction_);
				RebuildFunctions();
				changed = true;
			}
			for (const auto& [name, value] : pendingVars_) {
				SetVariable(name, value);
				changed = true;
			}
			pendingVars_.clear();
			hasPendingFunction_ = false;
			return changed;
		}

		void SetAns(const double value) {
			SetVariable("ans", value);
		}

		void SetVariable(const std::string& name, const double value) {
			if (auto* var = userTable_.get_variable(name)) var->ref() = value;
			else userTable_.create_variable(name, value);
		}

		void ClearUserSymbols() {
			expression_.release();
			functions_.clear();
			RebuildFunctions();
			std::vector<std::string> names;
			userTable_.get_variable_list(names);
			for (const auto& name : names) {
				if (!EqualsIgnoreCase(name, "ans")) userTable_.remove_variable(name);
			}
			SetAns(0);
		}

		std::vector<std::pair<std::string, double>> GetVariables() const {
			std::vector<std::pair<std::string, double>> vars;
			userTable_.get_variable_list(vars);
			return vars;
		}

		const std::vector<UserFunction>& GetFunctions() const {
			return functions_;
		}

		// 全角符号、× ÷ π √ ° 等转换为 exprtk 可识别的 ASCII 表达式（UTF-8）
		static std::string NormalizeToAscii(const std::wstring& input) {
			std::wstring out;
			out.reserve(input.size() + 8);
			for (size_t i = 0; i < input.size(); ++i) {
				wchar_t c = input[i];
				if (c >= 0xFF01 && c <= 0xFF5E) c = static_cast<wchar_t>(c - 0xFEE0);
				switch (c) {
				case 0x3000: out += L' ';
					break;
				case 0x00D7: // ×
				case 0x00B7: // ·
				case 0x22C5: // ⋅
				case 0x2219: // ∙
					out += L'*';
					break;
				case 0x00F7: out += L'/';
					break;
				case 0x2212: // −
				case 0x2013: // –
					out += L'-';
					break;
				case 0x3002: out += L'.';
					break;
				case 0x3001: out += L',';
					break;
				case L'"':
				case 0x2018:
				case 0x2019:
				case 0x201C:
				case 0x201D:
					out += L'\'';
					break;
				case 0x03C0: out += L"pi";
					break;
				case 0x00B2: out += L"^2";
					break;
				case 0x00B3: out += L"^3";
					break;
				case 0x2264: out += L"<=";
					break;
				case 0x2265: out += L">=";
					break;
				case 0x2260: out += L"!=";
					break;
				case 0x00B0: out += L"*degunit";
					break;
				case 0x221A: {
					// √2 -> sqrt(2)，√(...) -> sqrt(...)
					size_t j = i + 1;
					while (j < input.size() && (iswdigit(input[j]) || input[j] == L'.')) ++j;
					if (j > i + 1) {
						out += L"sqrt(" + input.substr(i + 1, j - i - 1) + L")";
						i = j - 1;
					} else {
						out += L"sqrt";
					}
					break;
				}
				default: out += c;
					break;
				}
			}
			return wide_to_utf8(out);
		}

		// exprtk 不支持的语法转换：0x/0b/0o 字面量、** 幂、n! 阶乘、角度单位三角函数
		std::string Preprocess(const std::string& s) const {
			std::string out;
			out.reserve(s.size() + 16);
			const size_t n = s.size();
			size_t i = 0;
			while (i < n) {
				const char c = s[i];
				if (c == '\'') {
					// 字符串字面量原样保留
					size_t j = i + 1;
					while (j < n && s[j] != '\'') j += (s[j] == '\\' && j + 1 < n) ? 2 : 1;
					j = (std::min)(j + 1, n);
					out.append(s, i, j - i);
					i = j;
				} else if (IsIdentStart(c)) {
					size_t j = i + 1;
					while (j < n && IsIdentChar(s[j])) ++j;
					std::string ident = s.substr(i, j - i);
					if (angleUnit_ != AngleUnit::Radian && IsAngleFunction(ident) && NextNonSpace(s, j) == '(') {
						ident += "_u";
					}
					out += ident;
					i = j;
				} else if (IsDigit(c) || (c == '.' && i + 1 < n && IsDigit(s[i + 1]))) {
					size_t j = i;
					if (c == '0' && i + 2 < n) {
						const char p = static_cast<char>(tolower(static_cast<unsigned char>(s[i + 1])));
						const int base = p == 'x' ? 16 : p == 'b' ? 2 : p == 'o' ? 8 : 0;
						if (base && DigitValue(s[i + 2]) >= 0 && DigitValue(s[i + 2]) < base) {
							uint64_t value = 0;
							j = i + 2;
							for (; j < n; ++j) {
								if (s[j] == '_') continue;
								const int d = DigitValue(s[j]);
								if (d < 0 || d >= base) break;
								value = value * base + d;
							}
							out += std::to_string(value);
							i = j;
							continue;
						}
					}
					while (j < n && (IsDigit(s[j]) || s[j] == '.')) ++j;
					if (j < n && (s[j] == 'e' || s[j] == 'E')) {
						size_t k = j + 1;
						if (k < n && (s[k] == '+' || s[k] == '-')) ++k;
						if (k < n && IsDigit(s[k])) {
							j = k;
							while (j < n && IsDigit(s[j])) ++j;
						}
					}
					out.append(s, i, j - i);
					i = j;
				} else if (c == '*' && i + 1 < n && s[i + 1] == '*') {
					out += '^';
					i += 2;
				} else if (c == '!' && (i + 1 >= n || s[i + 1] != '=') && WrapFactorial(out)) {
					++i;
				} else {
					out += c;
					++i;
				}
			}
			return out;
		}

	private:
		struct Resolver final : parser_t::unknown_symbol_resolver {
			std::vector<std::string> created;

			bool process(const std::string& name, usr_symbol_type& st, double& defaultValue,
						std::string& errorMessage) override {
				st = e_usr_variable_type;
				defaultValue = 0;
				errorMessage.clear();
				created.push_back(name);
				return true;
			}
		};

		// 子表达式（deriv('x^2', 1) 等字符串参数）编译结果，x 为自变量
		struct InnerFunction {
			double x = 0;
			symbol_table_t local;
			expression_t expr;

			double operator()(const double v) {
				x = v;
				return expr.value();
			}
		};

		class CalculusFunction final : public exprtk::igeneric_function<double> {
		public:
			enum class Op { Deriv, Deriv2, Integ, Solve, Sigma, Prod };

			CalculusFunction(CalcEngine& engine, const Op op, const std::string& sequence)
				: exprtk::igeneric_function<double>(sequence), engine_(engine), op_(op) {
			}

			double operator()(parameter_list_t params) override {
				return Run(0, params);
			}

			double operator()(const std::size_t& ps, parameter_list_t params) override {
				return Run(ps, params);
			}

		private:
			double Run(const std::size_t ps, parameter_list_t& params) {
				using string_t = generic_type::string_view;
				using scalar_t = generic_type::scalar_view;
				const auto arg = [&](const size_t i) { return scalar_t(params[i])(); };

				InnerFunction f;
				const bool isSeries = op_ == Op::Sigma || op_ == Op::Prod;
				if (!engine_.CompileInner(exprtk::to_str(string_t(params[0])), f, op_ == Op::Solve, isSeries)) {
					return kNaN;
				}

				switch (op_) {
				case Op::Deriv:
					f.x = arg(1);
					return exprtk::derivative(f.expr, f.x);
				case Op::Deriv2:
					f.x = arg(1);
					return exprtk::second_derivative(f.expr, f.x);
				case Op::Integ: {
					double a = arg(1), b = arg(2);
					if (a == b) return 0;
					const bool negative = a > b;
					if (negative) std::swap(a, b);
					const double area = exprtk::integrate(f.expr, f.x, a, b, 20000);
					return negative ? -area : area;
				}
				case Op::Solve:
					return ps == 0 ? SolveInRange(f, arg(1), arg(2)) : Newton(f, arg(1));
				case Op::Sigma:
				case Op::Prod: {
					int64_t a, b;
					if (!ToInt64(arg(1), a) || !ToInt64(arg(2), b) || b - a > 1000000) return kNaN;
					double acc = op_ == Op::Sigma ? 0 : 1;
					for (int64_t k = a; k <= b; ++k) {
						const double v = f(static_cast<double>(k));
						acc = op_ == Op::Sigma ? acc + v : acc * v;
					}
					return acc;
				}
				}
				return kNaN;
			}

			static double Newton(InnerFunction& f, const double x0) {
				double x = x0;
				for (int i = 0; i < 100; ++i) {
					const double fx = f(x);
					if (!std::isfinite(fx)) return kNaN;
					if (fx == 0) return x;
					const double h = 1e-7 * (std::max)(1.0, std::fabs(x));
					const double d = (f(x + h) - f(x - h)) / (2 * h);
					if (d == 0 || !std::isfinite(d)) break;
					const double next = x - fx / d;
					const bool converged = std::fabs(next - x) <= 1e-14 * (std::max)(1.0, std::fabs(next));
					x = next;
					if (converged) break;
				}
				const double fx = f(x);
				return std::isfinite(fx) && std::fabs(fx) < 1e-9 ? x : kNaN;
			}

			static double SolveInRange(InnerFunction& f, double a, double b) {
				if (a > b) std::swap(a, b);
				double fa = f(a);
				const double fb = f(b);
				if (fa == 0) return a;
				if (fb == 0) return b;
				if (!std::isfinite(fa) || !std::isfinite(fb) || (fa < 0) == (fb < 0)) {
					// 区间两端同号，退回牛顿迭代
					return Newton(f, (a + b) / 2);
				}
				for (int i = 0; i < 200; ++i) {
					const double m = (a + b) / 2;
					const double fm = f(m);
					if (fm == 0 || (b - a) / 2 <= 1e-15 * (std::max)(1.0, std::fabs(m))) return m;
					if ((fm < 0) == (fa < 0)) {
						a = m;
						fa = fm;
					} else {
						b = m;
					}
				}
				return (a + b) / 2;
			}

			CalcEngine& engine_;
			Op op_;
		};

		void Setup() {
			constTable_.add_constants();
			constTable_.add_constant("e", 2.718281828459045235360287471352);
			constTable_.add_constant("tau", 6.283185307179586476925286766559);
			constTable_.add_constant("phi", 1.618033988749894848204586834365);
			constTable_.add_variable("degunit", degreeUnit_);

			funcTable_.add_function("sin_u", fn::SinU);
			funcTable_.add_function("cos_u", fn::CosU);
			funcTable_.add_function("tan_u", fn::TanU);
			funcTable_.add_function("cot_u", fn::CotU);
			funcTable_.add_function("sec_u", fn::SecU);
			funcTable_.add_function("csc_u", fn::CscU);
			funcTable_.add_function("asin_u", fn::AsinU);
			funcTable_.add_function("acos_u", fn::AcosU);
			funcTable_.add_function("atan_u", fn::AtanU);
			funcTable_.add_function("atan2_u", fn::Atan2U);

			funcTable_.add_function("cbrt", fn::Cbrt);
			funcTable_.add_function("gamma", fn::Gamma);
			funcTable_.add_function("fact", fn::Fact);
			funcTable_.add_function("ncr", fn::Ncr);
			funcTable_.add_function("npr", fn::Npr);
			funcTable_.add_function("isprime", fn::IsPrime);
			funcTable_.add_function("nextprime", fn::NextPrime);
			funcTable_.add_function("fib", fn::Fib);
			funcTable_.add_function("rand", fn::RandUniform);
			funcTable_.add_function("randint", fn::RandInt);
			funcTable_.add_function("ranint", fn::RanInt);
			funcTable_.add_function("band", fn::BitAnd);
			funcTable_.add_function("bor", fn::BitOr);
			funcTable_.add_function("bxor", fn::BitXor);
			funcTable_.add_function("bnot", fn::BitNot);
			funcTable_.add_function("shl", fn::Shl);
			funcTable_.add_function("shr", fn::Shr);

			funcTable_.add_function("gcd", gcd_);
			funcTable_.add_function("lcm", lcm_);
			funcTable_.add_function("mean", mean_);
			funcTable_.add_function("median", median_);
			funcTable_.add_function("variance", variance_);
			funcTable_.add_function("stddev", stddev_);

			funcTable_.add_function("deriv", deriv_);
			funcTable_.add_function("deriv2", deriv2_);
			funcTable_.add_function("integ", integ_);
			funcTable_.add_function("solve", solve_);
			funcTable_.add_function("sigma", sigma_);
			funcTable_.add_function("prod", prod_);

			vecops_.register_package(funcTable_);

			userTable_.create_variable("ans", 0);

			loopCheck_.loop_set = exprtk::loop_runtime_check::e_all_loops;
			loopCheck_.max_loop_iterations = 1000000;
			parser_.register_loop_runtime_check(loopCheck_);
			parser_.enable_unknown_symbol_resolver(&resolver_);

			compositor_.add_auxiliary_symtab(userTable_);
			compositor_.add_auxiliary_symtab(funcTable_);
			compositor_.add_auxiliary_symtab(constTable_);
			compositor_.register_loop_runtime_check(loopCheck_);

			// 第一个注册的符号表用于存放未知符号创建的变量
			expression_.register_symbol_table(userTable_);
			expression_.register_symbol_table(compositor_.symbol_table());
			expression_.register_symbol_table(funcTable_);
			expression_.register_symbol_table(constTable_);
		}

		void RegisterSharedTables(expression_t& expr) {
			expr.register_symbol_table(userTable_);
			expr.register_symbol_table(compositor_.symbol_table());
			expr.register_symbol_table(funcTable_);
			expr.register_symbol_table(constTable_);
		}

		bool CompileInner(const std::string& body, InnerFunction& f, const bool asEquation, const bool isSeries) {
			f.local.add_variable("x", f.x);
			if (isSeries) {
				f.local.add_variable("n", f.x);
				f.local.add_variable("k", f.x);
				f.local.add_variable("i", f.x);
			}
			f.expr.register_symbol_table(f.local);
			RegisterSharedTables(f.expr);
			parser_t parser;
			parser.register_loop_runtime_check(loopCheck_);
			const std::string src = asEquation ? EquationToZero(body) : body;
			return parser.compile(Preprocess(src), f.expr);
		}

		Result EvaluateExpression(const std::string& expr, const bool allowDefine) {
			std::vector<std::pair<std::string, double>> before;
			userTable_.get_variable_list(before);
			resolver_.created.clear();

			if (!parser_.compile(expr, expression_)) {
				std::string error = ParserError(parser_);
				DiscardCreatedVariables();
				return MakeError(error);
			}

			// 只允许 name := ... 形式隐式创建新变量，其他未知符号视为错误
			for (const auto& name : resolver_.created) {
				if (!allowDefine || !IsAssignmentTarget(expr, name)) {
					DiscardCreatedVariables();
					return MakeError("未定义的符号: " + name);
				}
			}

			Result r;
			try {
				r.value = expression_.value();
			} catch (const std::exception& ex) {
				DiscardCreatedVariables();
				RestoreVariables(before);
				return MakeError(std::string("运行错误: ") + ex.what());
			}

			// 没有 return 语句时 return_invoked() 会解引用空指针，这里用 results().count() 判断
			if (expression_.results().count() > 0) ReadReturnResults(r);
			else r.kind = Result::Kind::Value;

			std::vector<std::pair<std::string, double>> after;
			userTable_.get_variable_list(after);
			for (const auto& [name, value] : after) {
				const auto it = std::find_if(before.begin(), before.end(), [&](const auto& p) { return p.first == name; });
				if (it == before.end() || !SameValue(it->second, value)) pendingVars_.emplace_back(name, value);
			}
			DiscardCreatedVariables();
			RestoreVariables(before);

			if (!pendingVars_.empty()) {
				if (!allowDefine) return MakeError("不允许赋值");
				for (const auto& [name, value] : pendingVars_) {
					if (!r.detail.empty()) r.detail += L", ";
					r.detail += utf8_to_wide(name) + L" = " + FormatNumber(value);
				}
				if (r.kind == Result::Kind::Value) r.kind = Result::Kind::Assignment;
			}
			return r;
		}

		void ReadReturnResults(Result& r) const {
			using results_t = exprtk::results_context<double>;
			using type_store_t = results_t::type_store_t;
			const results_t& results = expression_.results();
			bool hasScalar = false;
			std::wstring text;
			for (size_t i = 0; i < results.count(); ++i) {
				auto& ts = const_cast<type_store_t&>(results[i]);
				if (i) text += L", ";
				switch (ts.type) {
				case type_store_t::e_scalar: {
					const double v = results_t::scalar_t(ts)();
					if (!hasScalar) r.value = v;
					hasScalar = true;
					text += FormatNumber(v);
					break;
				}
				case type_store_t::e_vector: {
					const results_t::vector_t vec(ts);
					text += L"[";
					for (size_t k = 0; k < vec.size(); ++k) text += (k ? L", " : L"") + FormatNumber(vec[k]);
					text += L"]";
					break;
				}
				case type_store_t::e_string:
					text += utf8_to_wide(exprtk::to_str(results_t::string_t(ts)));
					break;
				default:
					break;
				}
			}
			if (results.count() == 1 && hasScalar) {
				r.kind = Result::Kind::Value;
			} else {
				r.kind = Result::Kind::Text;
				r.text = text;
			}
		}

		bool TryParseFunctionDefinition(const std::string& src, Result& r) {
			static const std::regex re(
				R"(^([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*([A-Za-z_][A-Za-z0-9_]*(?:\s*,\s*[A-Za-z_][A-Za-z0-9_]*)*)\s*\)\s*:?=(?!=)\s*(.*)$)");
			std::smatch m;
			if (!std::regex_match(src, m, re)) return false;

			UserFunction f;
			f.name = m[1].str();
			if (IsKnownSymbol(f.name)) return false;

			static const std::regex paramRe(R"([A-Za-z_][A-Za-z0-9_]*)");
			const std::string params = m[2].str();
			for (auto it = std::sregex_iterator(params.begin(), params.end(), paramRe); it != std::sregex_iterator(); ++it) {
				f.params.push_back(it->str());
			}
			f.displayBody = m[3].str();
			Trim(f.displayBody);

			if (f.params.size() > 6) {
				r = MakeError("自定义函数最多支持 6 个参数");
				return true;
			}
			if (f.displayBody.empty()) {
				r = MakeError("请输入函数体，例如 f(x) := x^2 + 1");
				return true;
			}
			for (size_t i = 0; i < f.params.size(); ++i) {
				for (size_t j = 0; j < i; ++j) {
					if (EqualsIgnoreCase(f.params[i], f.params[j])) {
						r = MakeError("参数重复: " + f.params[i]);
						return true;
					}
				}
				if (exprtk::details::is_reserved_symbol(f.params[i])) {
					r = MakeError("参数名不可用: " + f.params[i]);
					return true;
				}
			}
			f.body = Preprocess(RenameParams(f.displayBody, f.params));

			// 校验函数体
			std::vector<double> dummy(f.params.size(), 0.0);
			symbol_table_t local;
			for (size_t i = 0; i < f.params.size(); ++i) local.add_variable(ParamName(i), dummy[i]);
			expression_t expr;
			expr.register_symbol_table(local);
			RegisterSharedTables(expr);
			parser_t parser;
			if (!parser.compile(f.body, expr)) {
				r = MakeError(ParserError(parser));
				return true;
			}

			pendingFunction_ = f;
			hasPendingFunction_ = true;
			r.kind = Result::Kind::FunctionDef;
			r.text = f.Signature();
			return true;
		}

		// 编译失败的函数（例如依赖的函数被重新定义为不同参数个数）会被移除
		void RebuildFunctions() {
			expression_.release();
			compositor_.clear();
			compositor_.register_loop_runtime_check(loopCheck_);
			std::vector<UserFunction> kept;
			for (const auto& f : functions_) {
				compositor_t::function def(f.name);
				def.expression(f.body);
				for (size_t i = 0; i < f.params.size(); ++i) def.var(ParamName(i));
				if (compositor_.add(def, true)) kept.push_back(f);
			}
			functions_ = std::move(kept);
		}

		// 参数在内部使用独立的名字，避免与同名的已保存变量冲突
		static std::string ParamName(const size_t index) {
			return "fnarg" + std::to_string(index);
		}

		static std::string RenameParams(const std::string& body, const std::vector<std::string>& params) {
			std::string out;
			size_t i = 0;
			while (i < body.size()) {
				if (body[i] == '\'') {
					size_t j = i + 1;
					while (j < body.size() && body[j] != '\'') j += (body[j] == '\\' && j + 1 < body.size()) ? 2 : 1;
					j = (std::min)(j + 1, body.size());
					out.append(body, i, j - i);
					i = j;
				} else if (IsIdentStart(body[i])) {
					size_t j = i + 1;
					while (j < body.size() && IsIdentChar(body[j])) ++j;
					const std::string ident = body.substr(i, j - i);
					const auto it = std::find_if(params.begin(), params.end(), [&](const std::string& p) {
						return EqualsIgnoreCase(p, ident);
					});
					out += it == params.end() ? ident : ParamName(static_cast<size_t>(it - params.begin()));
					i = j;
				} else {
					out += body[i++];
				}
			}
			return out;
		}

		void DiscardCreatedVariables() {
			if (resolver_.created.empty()) return;
			// 先释放表达式，避免其引用被删除的变量节点
			expression_.release();
			for (const auto& name : resolver_.created) userTable_.remove_variable(name);
			resolver_.created.clear();
		}

		void RestoreVariables(const std::vector<std::pair<std::string, double>>& values) {
			for (const auto& [name, value] : values) {
				if (auto* var = userTable_.get_variable(name)) var->ref() = value;
			}
		}

		bool IsKnownSymbol(const std::string& name) const {
			return exprtk::details::is_reserved_symbol(name)
				|| funcTable_.symbol_exists(name)
				|| constTable_.symbol_exists(name)
				|| userTable_.symbol_exists(name);
		}

		std::string ConvertSimpleAssignment(const std::string& src) const {
			// x = 5  ->  x := 5
			static const std::regex re(R"(^([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=))");
			std::smatch m;
			if (!std::regex_search(src, m, re)) return src;
			const std::string name = m[1].str();
			if (exprtk::details::is_reserved_symbol(name) || funcTable_.symbol_exists(name) || constTable_.symbol_exists(name)) {
				return src;
			}
			return name + " :=" + src.substr(m.length(0));
		}

		// 'x^2 = 2' -> '(x^2)-(2)'
		static std::string EquationToZero(const std::string& s) {
			size_t pos = std::string::npos;
			for (size_t i = 0; i < s.size(); ++i) {
				if (s[i] != '=') continue;
				const char prev = i ? s[i - 1] : '\0';
				const char next = i + 1 < s.size() ? s[i + 1] : '\0';
				if (prev == ':' || prev == '<' || prev == '>' || prev == '!' || prev == '=' || next == '=') continue;
				if (pos != std::string::npos) return s;
				pos = i;
			}
			if (pos == std::string::npos) return s;
			return "(" + s.substr(0, pos) + ")-(" + s.substr(pos + 1) + ")";
		}

		static bool IsAssignmentTarget(const std::string& expr, const std::string& name) {
			const std::regex re("(^|[^A-Za-z0-9_])" + name + "\\s*:=", std::regex::icase);
			return std::regex_search(expr, re);
		}

		// n! -> fact(n)，(a+b)! -> fact((a+b))，sqrt(4)! -> fact(sqrt(4))
		static bool WrapFactorial(std::string& out) {
			size_t end = out.size();
			while (end > 0 && out[end - 1] == ' ') --end;
			if (end == 0) return false;
			size_t start = end;
			if (out[end - 1] == ')') {
				int depth = 0;
				size_t i = end;
				while (i > 0) {
					--i;
					if (out[i] == ')') ++depth;
					else if (out[i] == '(' && --depth == 0) break;
				}
				if (depth != 0) return false;
				start = i;
				while (start > 0 && IsIdentChar(out[start - 1])) --start;
			} else if (IsIdentChar(out[end - 1]) || out[end - 1] == '.') {
				while (start > 0 && (IsIdentChar(out[start - 1]) || out[start - 1] == '.')) --start;
			} else {
				return false;
			}
			out = out.substr(0, start) + "fact(" + out.substr(start, end - start) + ")";
			return true;
		}

		static std::string ParserError(const parser_t& parser) {
			if (parser.error_count() == 0) return "表达式无效";
			std::string diag = parser.get_error(0).diagnostic;
			// 去掉 "ERR123 - " 前缀
			static const std::regex prefix(R"(^ERR\d+\s*-\s*)");
			diag = std::regex_replace(diag, prefix, "");
			return diag;
		}

		static Result MakeError(const std::string& message) {
			Result r;
			r.kind = Result::Kind::Error;
			r.text = utf8_to_wide(message);
			return r;
		}

		static bool SameValue(const double a, const double b) {
			return a == b || (std::isnan(a) && std::isnan(b));
		}

		static bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const char x, const char y) {
				return tolower(static_cast<unsigned char>(x)) == tolower(static_cast<unsigned char>(y));
			});
		}

		static bool IsAngleFunction(const std::string& ident) {
			static const char* names[] = {"sin", "cos", "tan", "cot", "sec", "csc", "asin", "acos", "atan", "atan2"};
			for (const char* name : names) {
				if (EqualsIgnoreCase(ident, name)) return true;
			}
			return false;
		}

		static char NextNonSpace(const std::string& s, size_t i) {
			while (i < s.size() && s[i] == ' ') ++i;
			return i < s.size() ? s[i] : '\0';
		}

		static bool IsDigit(const char c) { return c >= '0' && c <= '9'; }
		static bool IsIdentStart(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
		static bool IsIdentChar(const char c) { return IsIdentStart(c) || IsDigit(c); }

		static int DigitValue(const char c) {
			if (IsDigit(c)) return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}

		static void Trim(std::string& s) {
			const size_t start = s.find_first_not_of(" \t\r\n");
			if (start == std::string::npos) {
				s.clear();
				return;
			}
			s = s.substr(start, s.find_last_not_of(" \t\r\n") - start + 1);
		}

		AngleUnit angleUnit_ = AngleUnit::Radian;
		double degreeUnit_ = 1.0;

		// 成员按声明逆序析构：函数对象声明在符号表之前，保证符号表析构时它们仍然有效
		VarargFunction gcd_{fn::Gcd};
		VarargFunction lcm_{fn::Lcm};
		VarargFunction mean_{fn::Mean};
		VarargFunction median_{fn::Median};
		VarargFunction variance_{fn::Variance};
		VarargFunction stddev_{fn::Stddev};
		CalculusFunction deriv_{*this, CalculusFunction::Op::Deriv, "ST"};
		CalculusFunction deriv2_{*this, CalculusFunction::Op::Deriv2, "ST"};
		CalculusFunction integ_{*this, CalculusFunction::Op::Integ, "STT"};
		CalculusFunction solve_{*this, CalculusFunction::Op::Solve, "STT|ST"};
		CalculusFunction sigma_{*this, CalculusFunction::Op::Sigma, "STT"};
		CalculusFunction prod_{*this, CalculusFunction::Op::Prod, "STT"};
		exprtk::rtl::vecops::package<double> vecops_;
		exprtk::loop_runtime_check loopCheck_;

		symbol_table_t constTable_;
		symbol_table_t funcTable_;
		symbol_table_t userTable_;
		compositor_t compositor_;
		Resolver resolver_;
		parser_t parser_;
		expression_t expression_;

		std::vector<UserFunction> functions_;
		std::vector<std::pair<std::string, double>> pendingVars_;
		UserFunction pendingFunction_;
		bool hasPendingFunction_ = false;
	};
}
