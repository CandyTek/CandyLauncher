#pragma once
#pragma execution_character_set("utf-8")

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <memory>
#include <regex>
#include <sstream>
#include <iomanip>
#include <locale>
#include <cctype>
#include <cmath>
#include "UnitConversionData.hpp"
#include "../../util/StringUtil.hpp"

// 格式化转换结果：整数直接输出，其余最多 14 位有效数字并去掉小数部分的尾随零
inline std::wstring FormatNumber(double value) {
	std::wostringstream woss;

	// 检查是否是整数
	if (std::floor(value) == value && std::abs(value) < 1e15) {
		woss << std::fixed << std::setprecision(0) << value;
		return woss.str();
	}

	// 使用科学记数法或固定小数点
	woss << std::setprecision(14) << std::noshowpoint << value;
	std::wstring result = woss.str();

	// 只去掉小数部分的尾随零。科学记数法时零可能在指数里（1.25e+20），指数一个字符都不能动，
	// 否则 1.25e+20 会被改成 1.25e+2，显示和复制的数值都差了 18 个量级
	const size_t exponentPos = result.find_first_of(L"eE");
	std::wstring mantissa = result.substr(0, exponentPos);
	const std::wstring exponent = exponentPos == std::wstring::npos ? L"" : result.substr(exponentPos);
	if (mantissa.find(L'.') != std::wstring::npos) {
		mantissa.erase(mantissa.find_last_not_of(L'0') + 1, std::wstring::npos);
		if (!mantissa.empty() && mantissa.back() == L'.') {
			mantissa.pop_back();
		}
	}
	return mantissa + exponent;
}


// 结构体定义
struct ConvertModel {
public:
	double Value = 0.0;
	std::string FromUnit;
	std::string ToUnit;

	ConvertModel() = default;

	ConvertModel(double value, std::string fromUnit, std::string toUnit) {
		Value = value;
		FromUnit = std::move(fromUnit);
		ToUnit = std::move(toUnit);
	}
};

// 单位转换结果
struct UnitConversionResult {
	double ConvertedValue;
	std::string UnitName;
	std::string QuantityName;

	UnitConversionResult(double value, std::string unit, std::string quantity) : ConvertedValue(value), UnitName(std::move(unit)),
																				QuantityName(std::move(quantity)) {
	}

	std::wstring ToWString() const {
		std::wostringstream woss;
		woss.precision(14);
		woss << ConvertedValue << L" " << StringToWString(UnitName);
		return woss.str();
	}
};

// 输入解释器
class InputInterpreter {
public:
	// 正则分隔器
	[[deprecated(L"regex正则后行断言功能支持不全，易崩溃")]]
	static std::vector<std::string> RegexSplitter(const std::string& input) {
		std::regex pattern(R"((?<=\d)(?![,.\-])(?=\D)|(?<=\D)(?<![,.\-])(?=\d))");
		std::vector<std::string> result;
		std::sregex_token_iterator iter(input.begin(), input.end(), pattern, -1);
		std::sregex_token_iterator end;
		for (; iter != end; ++iter) {
			if (!iter->str().empty()) {
				result.push_back(iter->str());
			}
		}
		return result;
	}

	static std::vector<std::string> SafeRegexSplitter(const std::string& input) {
		std::vector<std::string> result;
		std::string current;

		for (size_t i = 0; i < input.size(); ++i) {
			if (i > 0 &&
				((isdigit(input[i - 1]) && !isdigit(input[i])) ||
				 (!isdigit(input[i - 1]) && isdigit(input[i])))) {
				result.push_back(current);
				current.clear();
				 }
			current += input[i];
		}

		if (!current.empty()) result.push_back(current);
		return result;
	}

	// text 开头完整数字的长度：可选符号、整数或小数（"10"、"1.5"、".5"）、可选指数（"1e3"、"2.5E-2"）；
	// 开头不是数字时返回 0
	static size_t NumberPrefixLength(const std::string& text) {
		const size_t size = text.size();
		size_t pos = 0;
		if (pos < size && (text[pos] == '+' || text[pos] == '-')) ++pos;
		size_t digits = 0;
		while (pos < size && isdigit(static_cast<unsigned char>(text[pos]))) { ++pos; ++digits; }
		if (pos < size && text[pos] == '.') {
			++pos;
			while (pos < size && isdigit(static_cast<unsigned char>(text[pos]))) { ++pos; ++digits; }
		}
		if (digits == 0) return 0;
		if (pos < size && (text[pos] == 'e' || text[pos] == 'E')) {
			size_t exponentEnd = pos + 1;
			if (exponentEnd < size && (text[exponentEnd] == '+' || text[exponentEnd] == '-')) ++exponentEnd;
			size_t exponentDigits = 0;
			while (exponentEnd < size && isdigit(static_cast<unsigned char>(text[exponentEnd]))) { ++exponentEnd; ++exponentDigits; }
			// "1em" 里的 e 是单位的一部分，不是指数
			if (exponentDigits > 0) pos = exponentEnd;
		}
		return pos;
	}

	// 整个 text 必须是一个数字；std::stod 接受 "10junk" 这样的前缀，这里不接受
	static bool ParseNumber(const std::string& text, double& value) {
		if (text.empty() || NumberPrefixLength(text) != text.size()) return false;
		try {
			size_t consumed = 0;
			value = std::stod(text, &consumed);
			return consumed == text.size();
		} catch (...) {
			return false;
		}
	}

	// "10m to cm" 的 "10m" 拆成数字和单位。数字部分保留符号、小数点和指数（"-10m"、".5m"、"1e3m"），
	// 单位取数字后面的全部字符（"°C"、"km/h"、"o.z" 这类不全是字母的单位也能保留）
	static void InputSpaceInserter(std::vector<std::string>& split) {
		if (split.size() != 3) return;

		const size_t numberLength = NumberPrefixLength(split[0]);
		if (numberLength == 0 || numberLength == split[0].size()) return;

		std::vector<std::string> newSplit = {split[0].substr(0, numberLength), split[0].substr(numberLength)};
		// 将原 split 中剩余的元素 ("in", "cm") 追加进来
		newSplit.insert(newSplit.end(), split.begin() + 1, split.end());
		split = newSplit;
	}

	// 处理英尺英寸简写 (e.g., 1', 1'2")
	static void ShorthandFeetInchHandler(std::vector<std::string>& split) {
		if (split.empty() || (split[0].find('\'') == std::string::npos && split[0].find('\"') == std::string::npos)) {
			return;
		}

		if (split.size() == 3) {
			// auto shortsplit = RegexSplitter(split[0]);
			auto shortsplit = SafeRegexSplitter(split[0]);

			if (shortsplit.size() == 2) {
				// 1' or 1"
				if (shortsplit[1] == "\'") {
					split = {shortsplit[0], "foot", split[1], split[2]};
				} else if (shortsplit[1] == "\"") {
					split = {shortsplit[0], "inch", split[1], split[2]};
				}
			} else if (shortsplit.size() >= 3 && shortsplit[1] == "\'") {
				// 1'2 or 1'2"
				bool isNegative = shortsplit[0][0] == '-';
				std::string feetStr = shortsplit[0];
				if (isNegative) {
					feetStr = feetStr.substr(1);
				}

				try {
					double feet = std::stod(feetStr);
					double inches = std::stod(shortsplit[2]);
					double totalFeet = feet + inches / 12.0;
					if (isNegative) totalFeet *= -1;

					split = {std::to_string(totalFeet), "foot", split[1], split[2]};
				} catch (...) {
					// 解析失败，保持原样
				}
			}
		}
	}

	// 添加度数前缀
	static void DegreePrefixer(std::vector<std::string>& split) {
		if (split.size() != 4) return;

		auto toLower = [](std::string s) {
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			return s;
		};

		// 处理 fromUnit
		std::string lower1 = toLower(split[1]);
		if (lower1 == "celsius") split[1] = "DegreeCelsius";
		else if (lower1 == "fahrenheit") split[1] = "DegreeFahrenheit";
		else if (lower1 == "c") split[1] = "°C";
		else if (lower1 == "f") split[1] = "°F";

		// 处理 toUnit
		std::string lower3 = toLower(split[3]);
		if (lower3 == "celsius") split[3] = "DegreeCelsius";
		else if (lower3 == "fahrenheit") split[3] = "DegreeFahrenheit";
		else if (lower3 == "c") split[3] = "°C";
		else if (lower3 == "f") split[3] = "°F";
	}

	// KPH 处理器
	static void KPHHandler(std::vector<std::string>& split) {
		if (split.size() != 4) return;

		auto replace = [](std::string& str, const std::string& from, const std::string& to) {
			size_t pos = 0;
			std::string lower = str;
			std::string lowerFrom = from;
			std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
			std::transform(lowerFrom.begin(), lowerFrom.end(), lowerFrom.begin(), ::tolower);
			if ((pos = lower.find(lowerFrom)) != std::string::npos) {
				str.replace(pos, from.length(), to);
			}
		};

		replace(split[1], "cph", "cm/h");
		replace(split[1], "kmph", "km/h");
		replace(split[1], "kph", "km/h");
		replace(split[1], "cmph", "cm/h");

		replace(split[3], "cph", "cm/h");
		replace(split[3], "kmph", "km/h");
		replace(split[3], "kph", "km/h");
		replace(split[3], "cmph", "cm/h");
	}

	// Metre 转换为 Meter
	static void MetreToMeter(std::vector<std::string>& split) {
		if (split.size() != 4) return;

		auto replace = [](std::string& str) {
			size_t pos = 0;
			std::string lower = str;
			std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
			if ((pos = lower.find("metre")) != std::string::npos) {
				str.replace(pos, 5, "meter");
			}
		};

		replace(split[1]);
		replace(split[3]);
	}

	// 加仑处理器
	static void GallonHandler(std::vector<std::string>& split) {
		if (split.size() != 4) return;

		auto toLower = [](std::string s) {
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			return s;
		};

		// 默认使用美制加仑
		std::string lower1 = toLower(split[1]);
		if (lower1 == "gal" || lower1 == "gallon") {
			split[1] = "UsGallon";
		}

		std::string lower3 = toLower(split[3]);
		if (lower3 == "gal" || lower3 == "gallon") {
			split[3] = "UsGallon";
		}
	}

	// 盎司处理器
	static void OunceHandler(std::vector<std::string>& split) {
		if (split.size() != 4) return;

		auto toLower = [](std::string s) {
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			return s;
		};

		// 默认使用美制盎司
		std::string lower1 = toLower(split[1]);
		if (lower1 == "o.z" || lower1 == "o.z." || lower1 == "oz" || lower1 == "ounce") {
			split[1] = "UsOunce";
		}

		std::string lower3 = toLower(split[3]);
		if (lower3 == "o.z" || lower3 == "o.z." || lower3 == "oz" || lower3 == "ounce") {
			split[3] = "UsOunce";
		}
	}

	// 平方单位处理器
	static void SquareHandler(std::vector<std::string>& split) {
		if (split.size() != 4) return;

		auto replace = [](std::string& str) {
			std::regex pattern(R"(sq(s|μm|mm|cm|dm|m|km|mil|in|ft|yd|mi|nmi))", std::regex::icase);
			str = std::regex_replace(str, pattern, "$1²");
		};

		replace(split[1]);
		replace(split[3]);
	}

	// 解析查询
	static std::unique_ptr<ConvertModel> Parse(const std::wstring& query) {
		std::string queryStr = WStringToUtf8(query);
		std::vector<std::string> split;

		// 分割输入
		std::istringstream iss(queryStr);
		std::string token;
		while (iss >> token) {
			split.push_back(token);
		}

		if (split.empty()) return nullptr;

		ShorthandFeetInchHandler(split);
		InputSpaceInserter(split);

		// 只接受 "10 ft in cm" 或 "10 ft to cm" 格式
		if (split.size() != 4) return nullptr;

		// 检查第三个单词是否为 "in" 或 "to"
		std::string connector = split[2];
		std::transform(connector.begin(), connector.end(), connector.begin(), ::tolower);
		if (connector != "in" && connector != "to") return nullptr;

		DegreePrefixer(split);
		MetreToMeter(split);
		KPHHandler(split);
		GallonHandler(split);
		OunceHandler(split);
		SquareHandler(split);

		// 尝试解析数值；"10junk m to cm" 这种数字后带垃圾字符的输入不接受
		double value = 0.0;
		if (!ParseNumber(split[0], value)) return nullptr;
		return std::make_unique<ConvertModel>(value, split[1], split[3]);
	}
};

// 单位处理器
class UnitHandler {
public:
	// 执行单位转换
	static double ConvertInput(const ConvertModel& model, const QuantityInfo& quantityInfo) {
		auto& db = UnitConversionDatabase::GetInstance();

		const UnitInfo* fromUnit = db.FindUnit(quantityInfo, model.FromUnit);
		const UnitInfo* toUnit = db.FindUnit(quantityInfo, model.ToUnit);

		if (!fromUnit || !toUnit) {
			return std::nan("");
		}

		// 先转换到基础单位，再转换到目标单位
		double baseValue = fromUnit->toBase(model.Value);
		double result = toUnit->fromBase(baseValue);

		return result;
	}

	// 转换并返回所有可能的结果
	static std::vector<UnitConversionResult> Convert(const ConvertModel& model) {
		std::vector<UnitConversionResult> results;
		auto& db = UnitConversionDatabase::GetInstance();

		const QuantityInfo* quantity = db.FindQuantityByUnits(model.FromUnit, model.ToUnit);
		if (quantity) {
			double convertedValue = ConvertInput(model, *quantity);
			if (!std::isnan(convertedValue)) {
				results.emplace_back(convertedValue, model.ToUnit, quantity->name);
			}
		}

		return results;
	}
};
