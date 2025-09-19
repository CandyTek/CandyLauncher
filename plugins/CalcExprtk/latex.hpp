#pragma once

#include <cctype>// for std::isspace
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>

// 包含 ExprTk 头文件
#include <mutex>

#include "exprtk/exprtk.hpp"

#define M_PI		3.14159265358979323846
#define M_E		2.7182818284590452354
#define tau  6.283185307179586476925286766559
#define phi  1.618033988749894848204586834365

class ImprovedLaTeXCalculator
{
private:
	std::map<std::string, double> variables_;
	exprtk::expression<double> expression_;
	exprtk::symbol_table<double> symbol_table_;

public:
	ImprovedLaTeXCalculator()
	{
		setupSymbolTable();
	}

	static double getRandomNumber(double min, double max)
	{
		// Initialize random seed
		static bool initialized = false;
		if (!initialized)
		{
			std::srand(static_cast<unsigned int>(std::time(0))); // Seed the random number generator with current time
			initialized = true;
		}

		// Generate random number in range [min, max]
		return min + (std::rand() / (static_cast<double>(RAND_MAX) + 1)) * (max - min);
	}

	static double random()
	{
		return getRandomNumber(0, 1);
	}
	
	static double randomInt()
	{
		return static_cast<int>(getRandomNumber(0, 100.99999999999999999));
	}

	void setupSymbolTable()
	{
		symbol_table_.add_constant("pi", M_PI);
		symbol_table_.add_constant("e", M_E);
		symbol_table_.add_constant("tau", tau);
		symbol_table_.add_constant("phi", phi);
		symbol_table_.add_function("rand", random);
		symbol_table_.add_function("ranint", randomInt);
		// ExprTk 默认包含大量函数，如 sin, cos, tan, log, ln, exp, sqrt, pow, abs, etc.
		expression_.register_symbol_table(symbol_table_);
	}

	std::mutex mtx_;  // Mutex to protect shared resources
	
	void setVariable(const std::string& name, double value)
	{
		// Lock the mutex to ensure thread-safety
		std::lock_guard<std::mutex> lock(mtx_);

		variables_[name] = value;
		symbol_table_.remove_variable(name);
		symbol_table_.add_variable(name, value);
		expression_.register_symbol_table(symbol_table_);
	}

	void addVariable(const std::string& name, double value)
	{
		// Lock the mutex to ensure thread-safety
		std::lock_guard<std::mutex> lock(mtx_);

		variables_[name] = value;
		symbol_table_.add_variable(name, variables_[name]);
		expression_.register_symbol_table(symbol_table_);
	}

	void removeVariable(const std::string& name)
	{
		// Lock the mutex to ensure thread-safety
		std::lock_guard<std::mutex> lock(mtx_);
		symbol_table_.remove_variable(name);
		expression_.register_symbol_table(symbol_table_);
	}

	// TODO! 待封装
	void setVariables(const std::map<std::string, double>& vars)
	{
		std::lock_guard<std::mutex> lock(mtx_);  // Add mutex lock here
		for (const auto& pair : vars)
		{
			variables_[pair.first] = pair.second;
			symbol_table_.remove_variable(pair.first);
			symbol_table_.add_variable(pair.first, variables_[pair.first]);
		}
		expression_.register_symbol_table(symbol_table_);
	}

	// --- 改进的预处理函数 ---

	// 辅助函数：查找匹配的右括号
	static size_t findMatchingBrace(const std::string& str, size_t start)
	{
		if (str[start] != '{') return -1;
		size_t level = 1;
		for (size_t i = start + 1; i < str.length(); ++i)
		{
			if (str[i] == '{')
			{
				level++;
			}
			else if (str[i] == '}')
			{
				level--;
				if (level == 0)
				{
					return i;
				}
			}
		}
		return -1; // Not found
	}

	// 递归处理 \frac
	std::string processFractions(const std::string& input)
	{
		std::string result;
		size_t i = 0;
		while (i < input.length())
		{
			if (input.substr(i, 5) == "\\frac")
			{
				i += 5;
				// 跳过可能的空格
				while (i < input.length() && std::isspace(input[i])) i++;

				if (i < input.length() && input[i] == '{')
				{
					size_t end_num = findMatchingBrace(input, i);
					if (end_num != -1)
					{
						std::string numerator = input.substr(i + 1, end_num - i - 1);
						i = end_num + 1;
						// 跳过可能的空格
						while (i < input.length() && std::isspace(input[i])) i++;

						if (i < input.length() && input[i] == '{')
						{
							size_t end_den = findMatchingBrace(input, i);
							if (end_den != -1)
							{
								std::string denominator = input.substr(i + 1, end_den - i - 1);
								// 递归处理分子和分母内部可能存在的 \frac
								numerator = processFractions(numerator);
								denominator = processFractions(denominator);
								result += "((" + numerator + ")/(" + denominator + "))";
								i = end_den + 1;
								continue;
							}
						}
					}
				}
				// 如果 \frac 格式不正确，保留原样或报错
				result += "\\frac"; // 或者抛出异常
			}
			else
			{
				result += input[i];
				i++;
			}
		}
		return result;
	}

	// 处理上标 ^
	std::string processPowers(const std::string& input)
	{
		std::string result;
		size_t i = 0;
		while (i < input.length())
		{
			// 查找 ^
			if (input[i] == '^')
			{
				i++; // 跳过 ^
				std::string base; // 我们需要知道底数是什么，这里假设它在 ^ 之前
				// 为了简化，我们只处理变量或数字后直接跟 ^{...} 或 ^...
				// 更复杂的逻辑需要重写整个解析器

				// 检查前面是否是函数名，用于处理 sin^2(x)
				if (result.length() >= 3)
				{
					std::string last_chars = result.substr(result.length() - 3);
					if (last_chars == "sin" || last_chars == "cos" || last_chars == "tan")
					{
						// 处理 sin^2(x) -> (sin(x))^2
						// 这需要更复杂的逻辑，这里提供一个简化版
						// 我们假设函数名是2或3个字符
						size_t func_start = result.length() - 3;
						std::string func_name = result.substr(func_start, 3);
						result.erase(func_start); // 移除函数名

						// 现在 i 指向 ^ 后面
						if (i < input.length() && input[i] == '{')
						{
							size_t end_exp = findMatchingBrace(input, i);
							if (end_exp != -1)
							{
								std::string exponent = input.substr(i + 1, end_exp - i - 1);
								// 现在需要找到函数的参数 (sin^2(x) 中的 x)
								// 这非常复杂，需要完整的解析器
								// 这里我们做一个非常简化的假设：参数紧跟在后面
								// 例如，输入是 "...sin^2(x)..."
								// 当前 result="...", input="^2(x)..."
								// i=1, end_exp=3, exponent="2"
								// 我们需要从 input 中提取 "(x)"
								size_t arg_start = end_exp + 1;
								if (arg_start < input.length() && input[arg_start] == '(')
								{
									size_t end_arg = findMatchingBrace(input, arg_start);
									if (end_arg != -1)
									{
										std::string argument = input.substr(arg_start + 1, end_arg - arg_start - 1);
										result += "(pow(" + func_name + "(" + argument + ")," + exponent + "))";
										i = end_arg + 1;
										continue;
									}
								}
								// 如果参数不匹配，回退
								result += func_name;
								// Fall through to normal power handling
							}
						}
						// 如果不是 {exp}(arg) 格式，也回退
						result += "^";
						continue;
					}
				}

				// 处理普通的 x^{...} 或 x^...
				if (i < input.length() && input[i] == '{')
				{
					size_t end_exp = findMatchingBrace(input, i);
					if (end_exp != -1)
					{
						std::string exponent = input.substr(i + 1, end_exp - i - 1);
						// 在 result 中，^ 前面的字符就是底数的一部分
						// 我们需要取出底数 (这很棘手，因为底数可能是多字符)
						// 简化：假设底数是前一个非空格字符 (对于 x^2) 或被 () 包围的部分
						// 更好的方法是重写整个解析逻辑，但我们在此基础上改进
						if (!result.empty())
						{
							char last_char = result.back();
							if (std::isalnum(last_char) || last_char == ')')
							{
								std::string base_str;
								if (last_char == ')')
								{
									// 寻找匹配的 (
									size_t paren_level = 1;
									size_t j = result.length() - 2;
									for (; j >= 0; j--)
									{
										if (result[j] == ')') paren_level++;
										else if (result[j] == '(')
											paren_level--;
										if (paren_level == 0) break;
									}
									if (j >= 0)
									{
										base_str = result.substr(j);
										result.erase(j);
									}
									else
									{
										// 不匹配的括号，回退
										result += '^';
										continue;
									}
								}
								else
								{
									// 简单的变量或数字
									size_t j = result.length() - 1;
									while (j >= 0 && (std::isalnum(result[j]) || result[j] == '_'))
									{
										j--;
									}
									base_str = result.substr(j + 1);
									result.erase(j + 1);
								}
								// 递归处理指数内部
								exponent = processPowers(exponent);
								result += "pow(" + base_str + "," + exponent + ")";
								i = end_exp + 1;
								continue;
							}
						}
					}
				}
				else if (i < input.length())
				{
					// 处理 x^2 这样的简单形式
					if (std::isalnum(input[i]) || input[i] == '(')
					{
						std::string exponent(1, input[i]);
						if (!result.empty())
						{
							char last_char = result.back();
							if (std::isalnum(last_char) || last_char == ')')
							{
								std::string base_str;
								if (last_char == ')')
								{
									size_t paren_level = 1;
									size_t j = result.length() - 2;
									for (; j >= 0; j--)
									{
										if (result[j] == ')') paren_level++;
										else if (result[j] == '(')
											paren_level--;
										if (paren_level == 0) break;
									}
									if (j >= 0)
									{
										base_str = result.substr(j);
										result.erase(j);
									}
								}
								else
								{
									size_t j = result.length() - 1;
									while (j >= 0 && (std::isalnum(result[j]) || result[j] == '_'))
									{
										j--;
									}
									base_str = result.substr(j + 1);
									result.erase(j + 1);
								}
								result += "pow(" + base_str + "," + exponent + ")";
								i++;
								continue;
							}
						}
					}
				}
				// 如果无法处理，保留 ^
				result += '^';
			}
			else
			{
				result += input[i];
				i++;
			}
		}
		return result;
	}


	// 主预处理函数
	std::string preprocessLaTeX(const std::string& latex)
	{
		std::string result = latex;

		// 1. 移除行内数学模式分隔符 $ 和 $$
		if (!result.empty() && result.front() == '$')
		{
			result.erase(0, 1);
			if (!result.empty() && result.back() == '$')
			{
				result.pop_back();
				if (!result.empty() && result.front() == '$')
				{
					result.erase(0, 1);
				}
			}
		}

		// 2. 移除 LaTeX 空格命令
		std::regex space_regex(R"(\\,|\\:|\\;|\\!|~|\\ |\\quad|\\qquad)");
		result = std::regex_replace(result, space_regex, "");

		// 3. 处理 LaTeX 分数 (支持嵌套)
		result = processFractions(result);

		// 4. 处理 LaTeX 上标 (支持 ^{...} 和 ^...)
		result = processPowers(result);

		// 5. 替换 LaTeX 乘法
		std::regex cdot_regex(R"(\\cdot)");
		result = std::regex_replace(result, cdot_regex, "*");
		std::regex times_regex(R"(\\times)");
		result = std::regex_replace(result, times_regex, "*");


		// 6. 将 LaTeX 函数名映射为 ExprTk 函数名 (ExprTk 通常直接支持)
		// 为了确保兼容性，我们显式映射一次
		std::map<std::string, std::string> func_map = {
			{R"(\\sin)", "sin"},
			{R"(\\cos)", "cos"},
			{R"(\\tan)", "tan"},
			{R"(\\arcsin)", "asin"},
			{R"(\\arccos)", "acos"},
			{R"(\\arctan)", "atan"},
			{R"(\\ln)", "log"},
			{R"(\\log)", "log10"},
			{R"(\\exp)", "exp"},
			{R"(\\sqrt)", "sqrt"},
			{R"(\\abs)", "abs"}
		};
		for (const auto& pair : func_map)
		{
			std::regex func_regex(pair.first);
			result = std::regex_replace(result, func_regex, pair.second);
		}

		// 7. 处理 LaTeX 点 (中间点) 作为乘法
		std::regex cdot_simple(R"(\\cdot)");
		result = std::regex_replace(result, cdot_simple, "*");

		return result;
	}

	std::pair<bool, std::string> evaluate(const std::string& latex_expr)
	{
		std::lock_guard<std::mutex> lock(mtx_);  // Add mutex lock here
		try
		{
			std::string processed_expr = preprocessLaTeX(latex_expr);
			// std::cout << "Debug: Processed expr: " << processed_expr << std::endl; // 调试输出

			exprtk::parser<double> parser;
			if (!parser.compile(processed_expr, expression_))
			{
				return {false, "Compilation error: " + parser.error()};
			}

			double result = expression_.value();

			// 格式化结果
			std::ostringstream oss;
			if (result == std::floor(result))
			{
				// 如果是整数，不显示小数点
				oss << static_cast<long long>(result);
			}
			else
			{
				oss << std::fixed << std::setprecision(8) << result;
				// 移除尾随的0 (简单方法)
				std::string res_str = oss.str();
				res_str.erase(res_str.find_last_not_of('0') + 1, std::string::npos);
				res_str.erase(res_str.find_last_not_of('.') + 1, std::string::npos);
				return {true, res_str};
			}
			return {true, oss.str()};
		}
		catch (const std::exception& e)
		{
			return {false, std::string("Evaluation error: ") + e.what()};
		}
	}
};
