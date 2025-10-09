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
#include <locale>
#include <cctype>
#include <cmath>
#include "UnitConversionData.hpp"
#include "../../util/StringUtil.hpp"


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

	// 改为查找并提取数字和文本 Token
	static std::vector<std::string> RegexTokenizer(const std::string& input) {
		std::vector<std::string> result;
		try {
			// 这个正则表达式会匹配两种模式：
			// 1. \d+(\.\d+)?  -> 匹配一个或多个数字，后面可选地跟着一个小数点和更多数字 (例如 "1", "12.5")
			// 2. [a-zA-Z]+     -> 匹配一个或多个英文字母 (例如 "ft", "cm")
			std::regex pattern(R"(\d+(\.\d+)?|[a-zA-Z]+)");

			auto words_begin = std::sregex_iterator(input.begin(), input.end(), pattern);
			auto words_end = std::sregex_iterator();

			for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
				result.push_back(i->str());
			}
		} catch (const std::regex_error& e) {
			// 即使是合法的正则，也最好加上异常处理，这是一种好的编程习惯
			std::wcerr << L"Regex error in RegexTokenizer: " << e.what() << std::endl;
		}
		return result;
	}

	static void InputSpaceInserter(std::vector<std::string>& split) {
		if (split.size() != 3) return;

		// 使用新的 Tokenizer 函数
		auto parsed = RegexTokenizer(split[0]);
		// 如果成功分割成 "1" 和 "ft" 这样的两部分
		if (parsed.size() > 1) {
			std::vector<std::string> newSplit = {parsed[0], parsed[1]};
			// 将原 split 中剩余的元素 ("in", "cm") 追加进来
			newSplit.insert(newSplit.end(), split.begin() + 1, split.end());
			split = newSplit;
		}
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

		// 尝试解析数值
		try {
			double value = std::stod(split[0]);
			return std::make_unique<ConvertModel>(value, split[1], split[3]);
		} catch (...) {
			return nullptr;
		}
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
