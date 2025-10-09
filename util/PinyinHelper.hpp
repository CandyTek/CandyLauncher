#pragma once
#pragma execution_character_set("utf-8")
#include <string>
#include <vector>
#include <regex>
#include <cpp-pinyin/ChineseG2p.h>
#include <cpp-pinyin/G2pglobal.h>
#include <cpp-pinyin/ManTone.h>
#include <cpp-pinyin/Pinyin.h>

#include "StringUtil.hpp"


extern const std::unordered_map<std::string, std::wstring> QuanPinArr;
extern const std::unordered_map<std::string, std::wstring> XiaoHePinyinArr;
extern const std::unordered_map<std::string, std::wstring> GuoBiaoPyArr;
extern const std::unordered_map<std::string, std::wstring> ZiGuangPyArr;
extern const std::unordered_map<std::string, std::wstring> PlusPlusPyArr;
extern const std::unordered_map<std::string, std::wstring> AbcPyArr;
extern const std::unordered_map<std::string, std::wstring> SougouPyArr;
extern const std::unordered_map<std::string, std::wstring> ZiRanMaPyArr;

extern const std::unordered_map<std::string, std::wstring>* pCurrPinyin;
inline std::unique_ptr<Pinyin::Pinyin> g2p_man;

class PinyinHelper {
public:
	static void SetActivePinyinScheme(const std::string& type) {
		if (type == "normal") {
			pCurrPinyin = nullptr;
		} else if (type == "xiaohe") {
			pCurrPinyin = &XiaoHePinyinArr;
		} else if (type == "guobiao") {
			pCurrPinyin = &GuoBiaoPyArr;
		} else if (type == "ziguang") {
			pCurrPinyin = &ZiGuangPyArr;
		} else if (type == "plusplus") {
			pCurrPinyin = &PlusPlusPyArr;
		} else if (type == "abc") {
			pCurrPinyin = &AbcPyArr;
		} else if (type == "sougou") {
			pCurrPinyin = &SougouPyArr;
		} else if (type == "weiruan") {
			pCurrPinyin = &SougouPyArr;
		} else if (type == "ziranma") {
			pCurrPinyin = &ZiRanMaPyArr;
		}
	}

	static std::wstring ExtractEnInitials(const std::wstring& input) {
		return ExtractFirstAlphabetFromWords(input);
	}


	static std::wstring ExtractFirstAlphabetFromWords(const std::wstring& input) {
		std::wstring result;
		bool newWord = true;

		for (const wchar_t ch : input) {
			if (iswalnum(ch)) // 字母或数字
			{
				if (newWord) {
					result += ch;
					newWord = false;
				}
			} else {
				newWord = true;
			}
		}
		return result;
	}


	static void initPinyinLib() {
		// 设置字典路径（需要指向编译输出目录中的dict文件夹）
		const auto dictPath = std::filesystem::current_path() / "dict";
		Pinyin::setDictionaryPath(dictPath);
		// 创建Pinyin转换器
		g2p_man = std::make_unique<Pinyin::Pinyin>();
	}

	static std::vector<std::wstring> GetFullPinyinSequence(const std::wstring& str) {
		const std::string utf8Sentence = wide_to_utf8(str);
		// 转换整句为拼音结果向量
		const Pinyin::PinyinResVector res = g2p_man->hanziToPinyin(
			utf8Sentence,
			Pinyin::ManTone::Style::NORMAL,
			Pinyin::Error::Default,
			false
		);

		// 每个汉字对应的拼音字符串数组
		std::vector<std::wstring> perCharPinyin;
		for (const auto& item : res) {
			if (item.error || pCurrPinyin == nullptr) {
				perCharPinyin.push_back(utf8_to_wide(item.pinyin));
				continue;
			}
			perCharPinyin.push_back(pCurrPinyin->at(item.pinyin)); // 每个结果对象都有 .pinyin 字段
		}

		return perCharPinyin;
	}

	static std::string GetFirstChars(const std::vector<std::string>& strPinyins) {
		std::string result;
		result.reserve(strPinyins.size()); // 预留空间以提升性能
		for (const auto& s : strPinyins) {
			if (!s.empty()) {
				result += s[0]; // 取首字符
			}
		}
		return result;
	}

	static std::wstring GetFirstChars(const std::vector<std::wstring>& strPinyins) {
		std::wstring result;
		result.reserve(strPinyins.size()); // 预留空间以提升性能
		for (const auto& s : strPinyins) {
			if (!s.empty()) {
				result += s[0]; // 取首字符
			}
		}
		return result;
	}

	static std::wstring GetPinyinWithVariants(const std::wstring& str) {
		std::wstring result;
		// 拆分中英文
		std::wstring enStr, chStr2;
		for (const wchar_t ch : str) {
			if (IsChineseChar(ch)) chStr2 += ch;
			else enStr += ch;
		}

		const std::vector<std::wstring> strPinyins = GetFullPinyinSequence(str);
		// const std::vector<std::wstring> chstrPinyins = GetPinyinInitialSequence(str);

		std::wstring chResultStr;
		for (const std::wstring& item : strPinyins) {
			chResultStr += item;
		}
		if (chResultStr != str) {
			result += chResultStr + L" " + GetFirstChars(strPinyins);
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


		// 添加英文部分的缩写形式
		if (const std::wstring convert_en_words_to_abbr_words = ExtractEnInitials(enStr); convert_en_words_to_abbr_words.length() >
			1)
			result += convert_en_words_to_abbr_words + L" ";

		// std::wstring outputdebug;
		// outputdebug += str;
		// outputdebug += L" ";
		// outputdebug += MyTrim(result);
		// std::wcout << outputdebug << std::endl;

		// 去除多余空格
		return str + L" " + MyTrim(result);
		// std::wstring temp = str + L" " + MyTrim(result);
		// ConsolePrintln(temp);
		// return temp;
	}
};
