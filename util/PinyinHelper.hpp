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
const std::unordered_map<std::string, std::wstring>& XiaoHePinyinArr();
const std::unordered_map<std::string, std::wstring>& GuoBiaoPyArr();
const std::unordered_map<std::string, std::wstring>& ZiGuangPyArr();
const std::unordered_map<std::string, std::wstring>& PlusPlusPyArr();
const std::unordered_map<std::string, std::wstring>& AbcPyArr();
const std::unordered_map<std::string, std::wstring>& SougouPyArr();
const std::unordered_map<std::string, std::wstring>& ZiRanMaPyArr();

extern const std::unordered_map<std::string, std::wstring>* pCurrPinyin;
// 初始化与方案切换应在索引前完成；初始化后的拼音转换可由多个索引线程并发调用。
inline std::unique_ptr<Pinyin::Pinyin> g2p_man;

class PinyinHelper {
public:
	static void SetActivePinyinScheme(const std::string& type) {
		if (type == "normal") {
			pCurrPinyin = nullptr;
		} else if (type == "xiaohe") {
			pCurrPinyin = &XiaoHePinyinArr();
		} else if (type == "guobiao") {
			pCurrPinyin = &GuoBiaoPyArr();
		} else if (type == "ziguang") {
			pCurrPinyin = &ZiGuangPyArr();
		} else if (type == "plusplus") {
			pCurrPinyin = &PlusPlusPyArr();
		} else if (type == "abc") {
			pCurrPinyin = &AbcPyArr();
		} else if (type == "sougou") {
			pCurrPinyin = &SougouPyArr();
		} else if (type == "weiruan") {
			pCurrPinyin = &SougouPyArr();
		} else if (type == "ziranma") {
			pCurrPinyin = &ZiRanMaPyArr();
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


	static void initPinyinLib(const std::wstring& executableFolder) {
		// 拼音字典始终从程序所在目录加载，不依赖启动快捷方式的“起始位置”。
		const auto dictPath = std::filesystem::path(executableFolder) / L"dict";
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

	// 一次拼音转换得到的全部搜索变体。拼音相关字段在没有中文时为空。
	struct MatchVariants {
		// 整串拼音：中文替换为（当前方案的）拼音，其余字符原样，无分隔；与原文相同时为空
		std::wstring pinyin;
		// 旧格式首字母：每个拼音项的首字符（非中文项也取首字符），只用于拼接 matchText
		std::wstring pinyinInitials;
		// 英文单词首字母（ExtractEnInitials），只用于拼接 matchText
		std::wstring enInitials;
		// 缩写：中文取拼音首字母，英文/数字单词取首字符，忽略分隔符。例如 "微信 wechat" -> "wxw"，
		// "visual studio code" -> "vsc"。供模糊打分的首字母通道使用
		std::wstring acronym;
	};

	// 只调用一次 hanziToPinyin，同时产出 matchText 需要的旧变体和模糊打分需要的新字段
	static MatchVariants BuildMatchVariants(const std::wstring& str) {
		MatchVariants v;
		if (str.empty()) return v;

		bool hasChinese = false;
		std::wstring enStr;
		for (const wchar_t ch : str) {
			if (IsChineseChar(ch)) hasChinese = true;
			else enStr += ch;
		}

		// 无中文时无需拼音库：缩写直接从单词首字符得到
		if (!hasChinese || g2p_man == nullptr) {
			v.enInitials = ExtractEnInitials(enStr);
			v.acronym = ExtractFirstAlphabetFromWords(str);
			return v;
		}

		const Pinyin::PinyinResVector res = g2p_man->hanziToPinyin(
			wide_to_utf8(str),
			Pinyin::ManTone::Style::NORMAL,
			Pinyin::Error::Default,
			false
		);

		std::wstring full;
		bool newWord = true;
		for (const auto& item : res) {
			std::wstring piece;
			if (item.error || pCurrPinyin == nullptr) {
				piece = utf8_to_wide(item.pinyin);
			} else if (const auto it = pCurrPinyin->find(item.pinyin); it != pCurrPinyin->end()) {
				piece = it->second;
			} else {
				piece = utf8_to_wide(item.pinyin);
			}
			full += piece;
			if (!piece.empty()) v.pinyinInitials += piece[0];

			if (!item.error) {
				// 一个汉字就是一个词
				if (!piece.empty()) v.acronym += piece[0];
				newWord = true;
				continue;
			}
			// 非中文片段：按单词取首字符
			for (const wchar_t ch : piece) {
				if (iswalnum(ch)) {
					if (newWord) {
						v.acronym += ch;
						newWord = false;
					}
				} else {
					newWord = true;
				}
			}
		}
		if (full != str) v.pinyin = std::move(full);
		v.enInitials = ExtractEnInitials(enStr);
		return v;
	}

	// 拼接给 matchText 用的旧格式："原文 全拼 拼音首字母英文首字母"（保持与历史版本一致）
	static std::wstring ComposeMatchText(const std::wstring& str, const MatchVariants& v) {
		std::wstring result;
		if (!v.pinyin.empty()) {
			result += v.pinyin + L" " + v.pinyinInitials;
		}
		// 添加英文部分的缩写形式
		if (v.enInitials.length() > 1) {
			result += v.enInitials + L" ";
		}
		// 去除多余空格
		return str + L" " + MyTrim(result);
	}

	static std::wstring GetPinyinWithVariants(const std::wstring& str) {
		return ComposeMatchText(str, BuildMatchVariants(str));
	}
};
