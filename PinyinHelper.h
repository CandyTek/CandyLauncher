#pragma once
#pragma execution_character_set("utf-8")
#include <string>
#include <vector>
#include <unordered_set>
#include <regex>
#include <algorithm>
#include <codecvt>
#include <locale>

#include "BaseTools.hpp"
#include "MainTools.hpp"


//static bool IsChinese(const std::wstring& str)
//{
//return std::ranges::any_of(str, [](wchar_t ch)
//{
//	return ch >= 0x4E00 && ch <= 0x9FFF;
//});
//	for (wchar_t ch : str)
//	{
//		if (ch >= 0x4E00 && ch <= 0x9FFF)
//			return true;
//	}
//	return false;
//}
extern const std::vector<std::wstring> QuanPinArr;
extern const std::vector<std::wstring> XiaoHePinyinArr;
extern const std::vector<std::wstring> GuoBiaoPyArr;
extern const std::vector<std::wstring> ZiGuangPyArr;
extern const std::vector<std::wstring> PlusPlusPyArr;
extern const std::vector<std::wstring> AbcPyArr;
extern const std::vector<std::wstring> SougouPyArr;
extern const std::vector<std::wstring> ZiRanMaPyArr;

extern const std::vector<std::wstring> HanCharArr;

class PinyinHelper
{
private:
	static inline const std::vector<std::wstring>* pCurrPinyin = &QuanPinArr;

public:
	static void changePinyinType(const std::string& type)
	{
		if (type == "normal")
		{
			pCurrPinyin = &QuanPinArr;
		}
		else if (type == "xiaohe")
		{
			pCurrPinyin = &XiaoHePinyinArr;
		}
		else if (type == "guobiao")
		{
			pCurrPinyin = &GuoBiaoPyArr;
		}
		else if (type == "ziguang")
		{
			pCurrPinyin = &ZiGuangPyArr;
		}
		else if (type == "plusplus")
		{
			pCurrPinyin = &PlusPlusPyArr;
		}
		else if (type == "abc")
		{
			pCurrPinyin = &AbcPyArr;
		}
		else if (type == "sougou")
		{
			pCurrPinyin = &SougouPyArr;
		}
		else if (type == "weiruan")
		{
			pCurrPinyin = &SougouPyArr;
		}
		else if (type == "ziranma")
		{
			pCurrPinyin = &ZiRanMaPyArr;
		}
	}

	static std::wstring ConvertEnWordsToAbbrWords(const std::wstring& input)
	{
		return ConvertEnWordsToFirstChar(input);
	}


	static std::vector<std::wstring> HandlePolyphone(const std::vector<std::wstring>& array)
	{
		std::vector<std::wstring> result;
		std::vector<std::wstring> temp;

		for (const auto& item : array)
		{
			std::vector<std::wstring> splits;
			size_t pos = 0, found;
			while ((found = item.find(L' ', pos)) != std::wstring::npos)
			{
				splits.push_back(item.substr(pos, found - pos));
				pos = found + 1;
			}
			splits.push_back(item.substr(pos));

			temp.clear();
			if (!result.empty())
			{
				for (const auto& res : result)
				{
					for (const auto& split : splits)
					{
						temp.push_back(res + split);
					}
				}
			}
			else
			{
				for (const auto& split : splits)
				{
					temp.push_back(split);
				}
			}
			result = temp;
		}
		return result;
	}

	static std::vector<std::wstring> HandlePolyphone2(const std::vector<std::wstring>& array)
	{
		std::vector<std::wstring> result;
		std::vector<std::wstring> temp;

		for (const auto& item : array)
		{
			std::vector<std::wstring> splits;
			size_t pos = 0, found;
			while ((found = item.find(L' ', pos)) != std::wstring::npos)
			{
				splits.push_back(item.substr(pos, found - pos));
				pos = found + 1;
			}
			splits.push_back(item.substr(pos));

			temp.clear();
			if (!result.empty())
			{
				for (const auto& res : result)
				{
					for (const std::wstring& split : splits)
					{
						// temp.push_back(res + split[0]);
						if (!split.empty())
							temp.push_back(res + split.substr(0, 1));
						else
						{
							temp.push_back(res);
						}
					}
				}
			}
			else
			{
				for (const std::wstring& split : splits)
				{
					if (!split.empty())
					{
						// wchar_t temp2 = split[0];
						temp.push_back(split.substr(0, 1));
					}
					// temp.push_back(split);
				}
			}
			result = temp;
		}
		return result;
	}


	static std::vector<std::wstring> HandlePolyphone3(const std::vector<std::wstring>& array)
	{
		std::vector<std::wstring> result;
		std::vector<std::wstring> temp;

		for (const auto& item : array)
		{
			std::vector<wchar_t> initials;
			size_t pos = 0, found;
			while ((found = item.find(L' ', pos)) != std::wstring::npos)
			{
				if (found > pos)
					initials.push_back(item[pos]);
				pos = found + 1;
			}
			if (pos < item.size())
				initials.push_back(item[pos]);

			temp.clear();
			if (!result.empty())
			{
				for (const auto& res : result)
				{
					for (const wchar_t ch : initials)
					{
						temp.push_back(res + ch);
					}
				}
			}
			else
			{
				for (const wchar_t ch : initials)
				{
					temp.emplace_back(1, ch);
				}
			}
			result = temp;
		}
		return result;
	}


	static std::wstring ConvertEnWordsToFirstChar(const std::wstring& input)
	{
		std::wstring result;
		bool newWord = true;

		for (const wchar_t ch : input)
		{
			if (iswalnum(ch)) // 字母或数字
			{
				if (newWord)
				{
					result += ch;
					newWord = false;
				}
			}
			else
			{
				newWord = true;
			}
		}
		return result;
	}

	static std::wstring ConvertEnWordsToFourChar(const std::wstring& input)
	{
		std::wstring result;
		std::wstring word;

		for (const wchar_t ch : input)
		{
			if (iswalpha(ch)) // 只处理字母
			{
				word += ch;
			}
			else
			{
				if (word.length() >= 5)
				{
					result += word.substr(0, 3);
					result += word[word.length() - 1];
				}
				word.clear();
			}
		}

		// 最后一个单词处理
		if (word.length() >= 5)
		{
			result += word.substr(0, 3);
			result += word[word.length() - 1];
		}

		return result;
	}


	static std::vector<std::wstring> GetPinyin(const std::wstring& str, const bool polyphone)
	{
		std::vector<std::wstring> result;
		for (wchar_t ch : str)
		{
			if (IsChineseChar(ch))
			{
				std::wstring s(1, ch);
				auto pinyins = GetPinyinByOne(s, polyphone);
				if (!pinyins.empty())
				{
					// result.insert(result.end(), pinyins.begin(), pinyins.end());
					std::wstring oss;
					for (size_t i = 0; i < pinyins.size(); ++i)
					{
						if (i > 0) oss += L' '; // 添加空格分隔符
						oss += pinyins[i];
					}
					result.push_back(oss);
				}
				else
				{
					result.push_back(s);
				}
			}
			else if (ch == L' ')
			{
				// 这里有问题，如果是vs2022 编译的，如果把空格也添加到数组里，就会把结果产生一个中文字符，很多很多个
			}
			else
			{
				//result.emplace_back(&ch);
				std::wstring oss;
				oss += ch;
				result.push_back(oss);
			}
		}
		return result;
	}

	static std::vector<std::wstring> GetPinyinFirstEnChar(const std::wstring& str, const bool polyphone)
	{
		std::vector<std::wstring> result;
		bool firstWord = true;
		for (const wchar_t ch : str)
		{
			if (IsChineseChar(ch))
			{
				firstWord = true;
				std::wstring s(1, ch);
				auto pinyins = GetPinyinByOne(s, polyphone);
				if (!pinyins.empty())
				{
					// result.insert(result.end(), pinyins.begin(), pinyins.end());
					std::wstring oss;
					for (size_t i = 0; i < pinyins.size(); ++i)
					{
						if (i > 0) oss += L" "; // 添加空格分隔符
						oss += pinyins[i];
					}
					result.push_back(oss);
				}
				else
				{
					result.push_back(s);
				}
			}
			else if (ch == L' ')
			{
				firstWord = true;
				//result.emplace_back(&ch);
				//std::wstring oss;
				//oss += ch;
				//result.push_back(oss);
			}
			else
			{
				if (firstWord)
				{
					//result.emplace_back(&ch);
					std::wstring oss;
					oss += ch;
					result.push_back(oss);

					firstWord = false;
				}
			}
		}
		return result;
	}


	static std::vector<std::wstring> GetPinyinByOne(const std::wstring& ch, const bool polyphone)
	{
		std::vector<std::wstring> result;
		for (size_t i = 0; i < HanCharArr.size(); ++i)
		{
			const std::wstring& hanGroup = HanCharArr[i];
			if (hanGroup.find(ch) != std::wstring::npos)
			{
				result.push_back((*pCurrPinyin)[i]);
				if (!polyphone)
					return result;
			}
		}
		return result;
	}

	// static bool IsChinese(const std::wstring& str)
	// {
	// 	static const std::wregex reg(LR"([\x4e00-\x9fff])");
	// 	return std::regex_match(str, reg);
	// }
	// static bool IsChinese(const std::wstring& str)
	// {
	// 	static const std::wregex reg(LR"([\x4e00-\x9fff])");
	// 	return std::regex_search(str, reg); // ✅ 匹配字符串中是否包含汉字
	// }
	// static bool IsChinese(const std::wstring& str)
	// {
	// 	if (str.empty()) return false;
	// 	for (wchar_t ch : str)
	// 	{
	// 		if (ch >= 0x4E00 && ch <= 0x9FFF)
	// 			return true;
	// 	}
	// 	return false;
	// }

	static std::wstring GetPinyinLongStr(const std::wstring& str)
	{
		// 拆分中英文
		std::wstring enStr, chStr;
		for (wchar_t ch : str)
		{
			if (IsChineseChar(ch))
				chStr += ch;
			else
				enStr += ch;
		}

		// std::vector<std::wstring> mainArr = GetPinyinAll(str, true);
		// std::vector<std::wstring> chArr = GetPinyinJustCh(chStr, true);

		// 拼音组合部分
		// std::unordered_set<std::wstring> resultSet;
		// for (const auto& item : HandlePolyphone(mainArr)) {
		// 	resultSet.insert(ToLower(item));
		// }
		// for (const auto& item : HandlePolyphone2(chArr)) {
		// 	resultSet.insert(ToLower(item));
		// }

		// GetPinyin

		// std::wstring result = enStr;
		std::wstring result;
		// for (const auto& item : resultSet) {
		// 	result += item + L" ";
		// }
		const auto strPinyins = GetPinyin(str, true);
		const auto chstrPinyins = GetPinyinFirstEnChar(str, true);

		std::unordered_set<std::wstring> resultSet;
		for (const auto& item : HandlePolyphone(strPinyins))
		{
			resultSet.insert(item);
		}
		if (chstrPinyins.size() > 1)
		{
			const std::vector<std::wstring> basic_strings = HandlePolyphone2(chstrPinyins);
			for (const auto& item : basic_strings)
			{
				resultSet.insert(item);
			}
		}

		for (const auto& item : resultSet)
		{
			result += item + L" ";
		}


		// result.insert(result.end(), temp.begin(), temp.end());
		// result.insert(result.end(), temp2.begin(), temp2.end());

		// for (const auto& pinyin : temp)
		// {
		// 	result += pinyin;
		// }
		// for (const auto& pinyin : temp2)
		// {
		// 	result += pinyin;
		// }


		const std::wstring convert_en_words_to_abbr_words = ConvertEnWordsToAbbrWords(enStr);
		// 添加英文部分的缩写形式
		if (convert_en_words_to_abbr_words.length() > 1)
			result += convert_en_words_to_abbr_words + L" ";

		const std::wstring input = ConvertEnWordsToFourChar(enStr);
		if (input.length() > 1)
			result += input;
		// std::wstring outputdebug;
		// outputdebug += str;
		// outputdebug += L" ";
		// outputdebug += MyTrim(result);
		// std::wcout << outputdebug << std::endl;

		// 去除多余空格
		return str + L" "+ MyTrim(result);
	}
};
