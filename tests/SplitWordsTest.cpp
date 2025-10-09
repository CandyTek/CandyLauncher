#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <filesystem>
#include "../util/PinyinHelper.hpp"
#include "../util/StringUtil.hpp"
#include <fcntl.h>
#include <io.h>
#include <cpp-pinyin/G2pglobal.h>
#include <cpp-pinyin/Pinyin.h>
#include <pugixml.hpp>

#include "util/json.hpp"


// 测试cpp-pinyin库的拼音转换功能
static void TestCppPinyinConversion2() {
	std::wcout << L"\n=== Testing cpp-pinyin library ===" << std::endl;

	// 设置字典路径（需要指向编译输出目录中的dict文件夹）
	const auto dictPath = std::filesystem::current_path() / "dict";
	Pinyin::setDictionaryPath(dictPath);

	// 创建Pinyin转换器
	auto g2p_man = std::make_unique<Pinyin::Pinyin>();

	// 测试句子列表
	std::vector<std::wstring> testSentences = {
		L"你好世界",
		L"中华人民共和国",
		L"人工智能技术发展迅速",
		L"今天天气真好",
		L"我爱编程",
		L"北京欢迎你",
		L"春眠不觉晓",
		L"窗前明月光"
	};

	std::wcout << L"\nTesting different pinyin styles:\n" << std::endl;

	for (const auto& sentence : testSentences) {
		// 将宽字符串转换为UTF-8
		std::string utf8Sentence = wide_to_utf8(sentence);

		// 使用不同的风格进行转换

		// 1. NORMAL风格（无声调）
		Pinyin::PinyinResVector pinyinNormal = g2p_man->hanziToPinyin(
			utf8Sentence,
			Pinyin::ManTone::Style::NORMAL,
			Pinyin::Error::Default,
			false
		);
		std::string normalStr = pinyinNormal.toStdStr();

		// 2. TONE风格（标准声调）
		Pinyin::PinyinResVector pinyinTone = g2p_man->hanziToPinyin(
			utf8Sentence,
			Pinyin::ManTone::Style::TONE,
			Pinyin::Error::Default,
			false
		);
		std::string toneStr = pinyinTone.toStdStr();

		// 3. TONE3风格（数字声调）
		Pinyin::PinyinResVector pinyinTone3 = g2p_man->hanziToPinyin(
			utf8Sentence,
			Pinyin::ManTone::Style::TONE3,
			Pinyin::Error::Default,
			false
		);
		std::string tone3Str = pinyinTone3.toStdStr();

		// 输出结果
		std::wcout << L"原文: " << sentence << std::endl;
		std::wcout << L"  NORMAL:  " << utf8_to_wide(normalStr) << std::endl;
		std::wcout << L"  TONE:    " << utf8_to_wide(toneStr) << std::endl;
		std::wcout << L"  TONE3:   " << utf8_to_wide(tone3Str) << std::endl;
		std::wcout << std::endl;
	}

	// 测试多音字
	std::wcout << L"\nTesting polyphonic characters (多音字):\n" << std::endl;
	std::vector<std::wstring> polyphonicTests = {
		L"我还行",
		L"银行",
		L"长大",
		L"长度"
	};

	for (const auto& text : polyphonicTests) {
		std::string utf8Text = wide_to_utf8(text);
		Pinyin::PinyinResVector pinyin = g2p_man->hanziToPinyin(
			utf8Text,
			Pinyin::ManTone::Style::TONE3,
			Pinyin::Error::Default,
			true // 返回所有候选拼音
		);

		std::wcout << L"原文: " << text << L" -> " << utf8_to_wide(pinyin.toStdStr()) << std::endl;

		// 显示每个字的候选拼音
		for (const auto& py : pinyin) {
			if (!py.candidates.empty() && py.candidates.size() > 1) {
				std::wcout << L"  字 '" << utf8_to_wide(py.hanzi) << L"' 的候选读音: ";
				for (size_t i = 0; i < py.candidates.size() && i < 3; ++i) {
					std::wcout << utf8_to_wide(py.candidates[i]);
					if (i < py.candidates.size() - 1 && i < 2) std::wcout << L", ";
				}
				std::wcout << std::endl;
			}
		}
		std::wcout << std::endl;
	}

	std::wcout << L"=== cpp-pinyin tests completed ===" << std::endl;
}

// 测试cpp-pinyin库的拼音转换功能
static void TestCppPinyinConversion() {
	std::wcout << L"\n=== Testing cpp-pinyin library ===" << std::endl;

	// 设置字典路径（需要指向编译输出目录中的dict文件夹）
	const auto dictPath = std::filesystem::current_path() / "dict";
	Pinyin::setDictionaryPath(dictPath);

	// 创建Pinyin转换器
	auto g2p_man = std::make_unique<Pinyin::Pinyin>();

	// 测试句子列表
	std::vector<std::wstring> testSentences = {
		L"你好世界",
		L"中华人民共和国",
		L"人工智能技术发展迅速",
		L"今天天气真好",
		L"我爱编程",
		L"北京欢迎你",
		L"春眠不觉晓",
		L"窗前明月光"
	};


	for (const auto& sentence : testSentences) {
		std::string utf8Sentence = wide_to_utf8(sentence);
		Pinyin::PinyinResVector pinyinNormal = g2p_man->hanziToPinyin(
			utf8Sentence,
			Pinyin::ManTone::Style::NORMAL,
			Pinyin::Error::Default,
			false
		);
		std::string normalStr = pinyinNormal.toStdStr();

		std::wcout << sentence << L"\t\t" << utf8_to_wide(normalStr) << std::endl;
	}

	std::wcout << L"=== cpp-pinyin tests completed ===" << std::endl;
}

// 测试pugixml库的XML解析功能
static void TestPugixmlParsing() {
	std::wcout << L"\n=== Testing pugixml library ===" << std::endl;

	// 创建一个简单的XML文档
	pugi::xml_document doc;

	// 从字符串解析XML
	const char* xmlString = R"(
		<?xml version="1.0" encoding="UTF-8"?>
		<bookstore>
			<book id="1">
				<title>C++ Primer</title>
				<author>Stanley Lippman</author>
				<price>89.99</price>
			</book>
			<book id="2">
				<title>Effective C++</title>
				<author>Scott Meyers</author>
				<price>79.99</price>
			</book>
			<book id="3">
				<title>The C++ Programming Language</title>
				<author>Bjarne Stroustrup</author>
				<price>99.99</price>
			</book>
		</bookstore>
	)";

	pugi::xml_parse_result result = doc.load_string(xmlString);

	if (!result) {
		std::wcout << L"XML解析失败: " << utf8_to_wide(result.description()) << std::endl;
		return;
	}

	std::wcout << L"XML解析成功!\n" << std::endl;

	// 遍历所有书籍
	pugi::xml_node bookstore = doc.child("bookstore");
	std::wcout << L"书店中的所有书籍:\n" << std::endl;

	for (pugi::xml_node book : bookstore.children("book")) {
		std::string id = book.attribute("id").as_string();
		std::string title = book.child("title").text().as_string();
		std::string author = book.child("author").text().as_string();
		double price = book.child("price").text().as_double();

		std::wcout << L"书籍ID: " << utf8_to_wide(id) << std::endl;
		std::wcout << L"  书名: " << utf8_to_wide(title) << std::endl;
		std::wcout << L"  作者: " << utf8_to_wide(author) << std::endl;
		std::wcout << L"  价格: $" << price << std::endl;
		std::wcout << std::endl;
	}

	// 使用XPath查询
	std::wcout << L"使用XPath查询价格大于80的书籍:\n" << std::endl;
	pugi::xpath_node_set books = doc.select_nodes("//book[price > 80]");

	for (pugi::xpath_node node : books) {
		pugi::xml_node book = node.node();
		std::string title = book.child("title").text().as_string();
		double price = book.child("price").text().as_double();
		std::wcout << L"  " << utf8_to_wide(title) << L" - $" << price << std::endl;
	}

	std::wcout << L"\n=== pugixml tests completed ===" << std::endl;
}


int main() {
	_setmode(_fileno(stdout), _O_U8TEXT); // 设置 wcout 输出为 UTF-8

	// 调用pugixml测试方法
	TestPugixmlParsing();

	// 调用cpp-pinyin测试方法
	TestCppPinyinConversion();

	PinyinHelper::initPinyinLib();
	std::wcout << PinyinHelper::GetPinyinWithVariants(L"Notepad") << std::endl;
	std::wcout << PinyinHelper::GetPinyinWithVariants(L"网易云音乐") << std::endl;
	// std::wcout << PinyinHelper::GetPinyinWithVariants(L"こんにちは世界") << std::endl;
	// std::wcout << PinyinHelper::GetPinyinWithVariants(L"안녕하세요") << std::endl;
	// std::wcout << PinyinHelper::GetPinyinWithVariants(L"مرحبا بالعالم") << std::endl;
	// std::wcout << PinyinHelper::GetPinyinWithVariants(L"Γειά σου Κόσμε") << std::endl;
	// std::wcout << PinyinHelper::GetPinyinWithVariants(L"测试😀🚀👍") << std::endl;
	// std::wcout << PinyinHelper::GetPinyinWithVariants(L"IDEA 编辑器テスト😀") << std::endl;


	std::string input = R"({"arg":"\"C:\\Users\\Test\\file.txt\"{{}"}  abcabcabc})";

	size_t end = find_json_end(input);
	if (end == std::string::npos) {
		std::wcerr << L"Invalid JSON" << std::endl;
		return 1;
	}

	std::string json_part = input.substr(0, end + 1);
	std::string tail_part = input.substr(end + 1);

	size_t pos = tail_part.find_first_not_of(" \t\r\n");
	if (pos != std::string::npos) tail_part = tail_part.substr(pos);

	nlohmann::json j = nlohmann::json::parse(json_part);
	std::string arg = j["arg"];
	if (arg.size() >= 2 && arg.front() == '"' && arg.back() == '"') arg = arg.substr(1, arg.size() - 2);

	std::wcout << "path = " << utf8_to_wide(arg) << "\n";
	std::wcout << "tail = " << utf8_to_wide(tail_part) << "\n";


	return 0;
}
