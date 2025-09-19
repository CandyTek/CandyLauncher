#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include "../util/PinyinHelper.h"
#include <fcntl.h>
#include <io.h>

//extern "C" {
//#include "../cli.c"   // 或 everything_ipc.h
//#include "../everything_ipc.h"   // 或 everything_ipc.h
//}

class ListedRunnerPlugin {
public:
    static std::vector<std::wstring> SplitWords(const std::wstring& input) {
        std::vector<std::wstring> result;
        size_t start = 0, end = 0;
        while ((end = input.find(L' ', start)) != std::wstring::npos) {
            if (end > start)
                result.push_back(input.substr(start, end - start));
            start = end + 1;
        }
        if (start < input.size())
            result.push_back(input.substr(start));
        return result;
    }
};

int main() {
    _setmode(_fileno(stdout), _O_U8TEXT); // 设置 wcout 输出为 UTF-8
    
    auto words = ListedRunnerPlugin::SplitWords(L"foo bar");
    assert(words.size() == 2);
    assert(words[0] == L"foo");
    assert(words[1] == L"bar");
    std::wcout << "SplitWordsTest passed" << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"Notepad") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"网易云音乐") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"こんにちは世界") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"안녕하세요") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"مرحبا بالعالم") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"Γειά σου Κόσμε") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"测试😀🚀👍") << std::endl;
    std::wcout << PinyinHelper::GetPinyinLongStr(L"IDEA 编辑器テスト😀") << std::endl;

    const char* args[] = {
            "program_name",  // argv[0] 一般是程序名，可以随便写
            "-p",
            "D:\\DengYuhua\\",
            "-r",
            "(.ini)$",
            "-s",
            nullptr           // 以 nullptr 结尾，便于遍历
    };

//    main33(0,args);
    return 0;
}

