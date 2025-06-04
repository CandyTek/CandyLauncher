#include <vector>
#include <string>
#include <iostream>
#include <cassert>

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
    auto words = ListedRunnerPlugin::SplitWords(L"foo bar");
    assert(words.size() == 2);
    assert(words[0] == L"foo");
    assert(words[1] == L"bar");
    std::cout << "SplitWordsTest passed" << std::endl;
    return 0;
}

