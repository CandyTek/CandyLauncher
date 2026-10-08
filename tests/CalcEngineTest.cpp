#include "../plugins/CalcExprtk/CalcEngine.hpp"

#include <cstdio>
#include <cstdlib>

using calc::CalcEngine;
using Kind = CalcEngine::Result::Kind;

static int failures = 0;
static const bool verbose = std::getenv("CALC_TEST_VERBOSE") != nullptr;

static void Trace(const std::wstring& input) {
	if (verbose) fwprintf(stderr, L"> %ls\n", input.c_str());
}

static void Fail(const std::wstring& input, const std::wstring& message) {
	++failures;
	wprintf(L"FAIL: %ls -> %ls\n", input.c_str(), message.c_str());
}

static void ExpectValue(CalcEngine& engine, const std::wstring& input, const double expected, const double tol = 1e-9) {
	Trace(input);
	const auto r = engine.Evaluate(input);
	if (r.kind != Kind::Value && r.kind != Kind::Assignment) {
		Fail(input, L"kind=" + std::to_wstring(static_cast<int>(r.kind)) + L" " + r.text);
	} else if (!(std::fabs(r.value - expected) <= tol * (std::max)(1.0, std::fabs(expected)))) {
		Fail(input, L"got " + calc::FormatNumber(r.value) + L", expected " + calc::FormatNumber(expected));
	}
}

static void ExpectKind(CalcEngine& engine, const std::wstring& input, const Kind kind, const bool allowDefine = true) {
	Trace(input);
	const auto r = engine.Evaluate(input, allowDefine);
	if (r.kind != kind) Fail(input, L"kind=" + std::to_wstring(static_cast<int>(r.kind)) + L" " + r.text);
}

static void ExpectText(const std::wstring& input, const std::wstring& actual, const std::wstring& expected) {
	if (actual != expected) Fail(input, L"got " + actual + L", expected " + expected);
}

int main() {
	CalcEngine engine;
	Trace(L"engine created");

	// 基础运算与预处理
	ExpectValue(engine, L"1+2*3", 7);
	ExpectValue(engine, L"2^10 + sqrt(16) * 5!", 1024 + 4 * 120);
	ExpectValue(engine, L"2**3", 8);
	ExpectValue(engine, L"(2+1)!", 6);
	ExpectValue(engine, L"0xFF + 0b1010 + 0o17", 255 + 10 + 15);
	ExpectValue(engine, L"３×４÷２", 6);
	ExpectValue(engine, L"（1＋2）×3", 9);
	ExpectValue(engine, L"√16 + 2π - 2*pi", 4);
	ExpectKind(engine, L"1 +", Kind::Error);
	ExpectKind(engine, L"foo + 1", Kind::Error);
	ExpectValue(engine, L"5 != 3", 1);

	// 角度单位
	ExpectValue(engine, L"sin(pi/2)", 1);
	ExpectValue(engine, L"sin(30°)", 0.5);
	engine.SetAngleUnit(calc::AngleUnit::Degree);
	ExpectValue(engine, L"sin(30) + cos(60)", 1);
	ExpectValue(engine, L"sin(180)", 0, 0);
	ExpectValue(engine, L"asin(1)", 90);
	ExpectValue(engine, L"sinh(0)", 0);
	engine.SetAngleUnit(calc::AngleUnit::Radian);

	// 变量
	ExpectKind(engine, L"x := 5", Kind::Assignment);
	ExpectKind(engine, L"x + 1", Kind::Error); // 未保存前不可用
	engine.Evaluate(L"x = 5");
	engine.CommitPending();
	ExpectValue(engine, L"x * 2", 10);
	ExpectValue(engine, L"2x", 10);
	ExpectKind(engine, L"x := x + 1", Kind::Assignment);
	ExpectValue(engine, L"x", 5); // 预览不会修改变量
	ExpectValue(engine, L"x == 5", 1);
	engine.SetAns(42);
	ExpectValue(engine, L"ans / 2", 21);
	ExpectValue(engine, L"r := 3; pi * r^2", calc::kPi * 9);
	ExpectKind(engine, L"y := 1", Kind::Error, false);

	// 自定义函数
	ExpectKind(engine, L"f(x) := x^2 + 2x + 1", Kind::FunctionDef);
	engine.CommitPending();
	ExpectValue(engine, L"f(3)", 16);
	engine.Evaluate(L"g(a, b) = f(a) + b");
	engine.CommitPending();
	ExpectValue(engine, L"g(1, 1)", 5);
	engine.Evaluate(L"f(x) = x");
	engine.CommitPending();
	ExpectValue(engine, L"g(1, 1)", 2);
	ExpectKind(engine, L"h(x) = x + unknown", Kind::Error);

	// 微积分
	ExpectValue(engine, L"deriv('x^3', 2)", 12, 1e-6);
	ExpectValue(engine, L"deriv2('x^3', 2)", 12, 1e-4);
	ExpectValue(engine, L"integ('x^2', 0, 3)", 9, 1e-9);
	ExpectValue(engine, L"integ('x^2', 3, 0)", -9, 1e-9);
	ExpectValue(engine, L"solve('x^2 = 2', 0, 5)", std::sqrt(2.0), 1e-12);
	ExpectValue(engine, L"solve('cos(x) - x', 1)", 0.7390851332151607, 1e-12);
	ExpectValue(engine, L"sigma('n', 1, 100)", 5050);
	ExpectValue(engine, L"prod('k', 1, 5)", 120);

	// 数论 / 组合 / 统计 / 位运算
	ExpectValue(engine, L"gcd(12, 18, 24)", 6);
	ExpectValue(engine, L"lcm(4, 6)", 12);
	ExpectValue(engine, L"ncr(5, 2) + npr(5, 2)", 30);
	ExpectValue(engine, L"isprime(1000000007) + isprime(91)", 1);
	ExpectValue(engine, L"nextprime(100)", 101);
	ExpectValue(engine, L"fib(10)", 55);
	ExpectValue(engine, L"median(3, 1, 4, 1, 5)", 3);
	ExpectValue(engine, L"stddev(2, 4, 4, 4, 5, 5, 7, 9)", std::sqrt(32.0 / 7));
	ExpectValue(engine, L"band(0xF0, 0x3C) + shl(1, 8)", 0x30 + 256);
	ExpectValue(engine, L"bnot(0)", -1);

	// 帮助中列出的 exprtk 内置函数
	ExpectValue(engine, L"roundn(pi, 3)", 3.142);
	ExpectValue(engine, L"clamp(0, 5, 3) + inrange(0, 2, 3)", 4);
	ExpectValue(engine, L"if(1 > 2, 10, 20) + (1 ? 2 : 3)", 22);
	ExpectValue(engine, L"ncdf(0) + erf(0) + hypot(3, 4)", 5.5);
	ExpectValue(engine, L"logn(8, 2) + root(27, 3) + cbrt(8)", 8);
	ExpectValue(engine, L"deg2rad(180) - pi", 0);

	// 语句与向量
	ExpectValue(engine, L"var s := 0; for (var i := 1; i <= 100; i += 1) { s += i; }; s", 5050);
	ExpectValue(engine, L"var v[5] := {5, 3, 9, 1, 7}; sort(v); v[0]", 1);
	ExpectKind(engine, L"while (true) { 1 }", Kind::Error);
	{
		const auto r = engine.Evaluate(L"return [1 + 1, 'ab' + 'c']");
		if (r.kind != Kind::Text) Fail(L"return", L"kind");
		ExpectText(L"return", r.text, L"2, abc");
	}

	// 格式化
	ExpectText(L"radix", calc::FormatRadix(42, 16), L"0x2A");
	ExpectText(L"radix", calc::FormatRadix(-5, 2), L"-0b101");
	ExpectText(L"thousands", calc::FormatThousands(1234567.5), L"1,234,567.5");
	ExpectText(L"thousands", calc::FormatThousands(-123456), L"-123,456");
	ExpectText(L"sci", calc::FormatScientific(12345000), L"1.2345e7");
	ExpectText(L"sci", calc::FormatScientific(0.00025), L"2.5e-4");
	ExpectText(L"number", calc::FormatNumber(0.1 + 0.2), L"0.3");
	{
		int64_t num = 0, den = 0;
		if (!calc::ToFraction(0.75, num, den) || num != 3 || den != 4) Fail(L"fraction", L"0.75");
		if (calc::ToFraction(calc::kPi, num, den)) Fail(L"fraction", L"pi");
	}

	// 清除
	engine.ClearUserSymbols();
	ExpectKind(engine, L"x", Kind::Error);
	ExpectKind(engine, L"f(1)", Kind::Error);

	if (failures) {
		wprintf(L"%d failure(s)\n", failures);
		return 1;
	}
	wprintf(L"CalcEngineTest passed\n");
	return 0;
}
