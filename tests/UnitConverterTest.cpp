#include "plugins/UnitConverter/UnitConverterUtil.hpp"
#include <cmath>
#include <iostream>
#include <string>

static int failures = 0;

static void Expect(const bool ok, const char* what) {
	if (ok) return;
	++failures;
	std::cout << "FAIL: " << what << "\n";
}

static bool Near(const double a, const double b) {
	return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b));
}

int main() {
	// 科学记数法：指数里的 0 不能被当成尾随零去掉
	Expect(FormatNumber(1.25e20) == L"1.25e+20", "1.25e20 keeps exponent");
	Expect(FormatNumber(1.25e-10) == L"1.25e-10", "1.25e-10 keeps exponent");
	Expect(FormatNumber(1.0e20) == L"1e+20", "1e20 trims mantissa only");
	// 普通小数和整数照旧
	Expect(FormatNumber(2.5) == L"2.5", "2.5 unchanged");
	Expect(FormatNumber(2.50000000001) == L"2.50000000001", "no false trimming");
	Expect(FormatNumber(100) == L"100", "integer");
	Expect(FormatNumber(0) == L"0", "zero");
	Expect(FormatNumber(-3.75) == L"-3.75", "negative decimal");

	// 紧凑写法：符号、小数点、指数都要保留
	auto model = InputInterpreter::Parse(L"-10m to cm");
	Expect(model && Near(model->Value, -10) && model->FromUnit == "m" && model->ToUnit == "cm", "-10m keeps sign");
	model = InputInterpreter::Parse(L"+2.5km to m");
	Expect(model && Near(model->Value, 2.5) && model->FromUnit == "km", "+2.5km keeps plus and decimal");
	model = InputInterpreter::Parse(L"1e3m to cm");
	Expect(model && Near(model->Value, 1000) && model->FromUnit == "m", "1e3m parses exponent");
	model = InputInterpreter::Parse(L".5m to cm");
	Expect(model && Near(model->Value, 0.5) && model->FromUnit == "m", ".5m leading decimal");
	model = InputInterpreter::Parse(L"10km/h to m/s");
	Expect(model && Near(model->Value, 10) && model->FromUnit == "km/h", "10km/h keeps slash unit");
	model = InputInterpreter::Parse(L"10°C to F");
	Expect(model && Near(model->Value, 10) && model->FromUnit == "°C" && model->ToUnit == "°F", "10°C compact degree");
	// 分开写法与简写不受影响
	model = InputInterpreter::Parse(L"10 m in cm");
	Expect(model && Near(model->Value, 10) && model->FromUnit == "m" && model->ToUnit == "cm", "spaced input");
	model = InputInterpreter::Parse(L"1'6\" to cm");
	Expect(model && Near(model->Value, 1.5) && model->FromUnit == "foot", "feet/inch shorthand");
	// 数字后带垃圾字符不能被当成数字前缀接受
	Expect(InputInterpreter::Parse(L"10junk m to cm") == nullptr, "10junk rejected");
	Expect(InputInterpreter::Parse(L"abc m to cm") == nullptr, "non-number rejected");
	Expect(InputInterpreter::Parse(L"10m cm") == nullptr, "missing connector rejected");

	// 端到端：大数值转换后显示的量级要正确
	model = InputInterpreter::Parse(L"1.25e20 m to m");
	Expect(model != nullptr, "1.25e20 m to m parses");
	if (model) {
		const auto results = UnitHandler::Convert(*model);
		Expect(!results.empty() && FormatNumber(results[0].ConvertedValue) == L"1.25e+20", "converted value formatted with full exponent");
	}

	if (failures) {
		std::cout << failures << " UnitConverterTest check(s) failed\n";
		return 1;
	}
	std::cout << "UnitConverterTest passed\n";
	return 0;
}
