# cpp-pinyin 使用教程

## 简介

cpp-pinyin 是一个轻量级的中文/粤语转拼音库，支持多种拼音风格和多音字处理。本文档介绍如何在 CandyLauncher 项目中使用该库。

## 项目集成

### 1. 添加为子模块

```bash
git submodule add https://github.com/wolfgitpr/cpp-pinyin 3rdparty/cpp-pinyin
```

### 2. CMakeLists.txt 配置

在主项目的 CMakeLists.txt 中添加以下配置：

```cmake
# 添加 cpp-pinyin 子目录
add_subdirectory(../3rdparty/cpp-pinyin)

# 链接库
target_link_libraries(CandyLauncher PRIVATE
        cpp-pinyin::cpp-pinyin
        # ... 其他库
)

# 添加包含目录
include_directories(
        ${CMAKE_SOURCE_DIR}/3rdparty/cpp-pinyin/include
)

# 复制字典文件到输出目录
add_custom_command(TARGET CandyLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/3rdparty/cpp-pinyin/res/dict
        $<TARGET_FILE_DIR:CandyLauncher>/dict
)
```

## 基本使用

### 1. 包含头文件

```cpp
#include <filesystem>
#include <cpp-pinyin/G2pglobal.h>
#include <cpp-pinyin/Pinyin.h>
```

### 2. 初始化拼音转换器

```cpp
// 设置字典路径（指向 dict 文件夹）
const auto dictPath = std::filesystem::current_path() / "dict";
Pinyin::setDictionaryPath(dictPath);

// 创建 Pinyin 转换器实例
auto g2p_man = std::make_unique<Pinyin::Pinyin>();
```

### 3. 基本转换示例

```cpp
// 输入 UTF-8 编码的中文字符串
std::string chineseText = "你好世界";

// 转换为拼音（无声调）
Pinyin::PinyinResVector pinyin = g2p_man->hanziToPinyin(
    chineseText,
    Pinyin::ManTone::Style::NORMAL,
    Pinyin::Error::Default,
    false
);

// 获取拼音字符串
std::string pinyinStr = pinyin.toStdStr(); // "ni hao shi jie"
```

## 拼音风格

cpp-pinyin 支持多种拼音输出风格：

### 1. NORMAL - 无声调

不带声调符号的拼音。

```cpp
// 输入：中国
// 输出：zhong guo
PinyinResVector pinyin = g2p_man->hanziToPinyin(
    "中国",
    Pinyin::ManTone::Style::NORMAL
);
```

### 2. TONE - 标准声调

拼音声调在韵母第一个字母上（默认风格）。

```cpp
// 输入：中国
// 输出：zhōng guó
PinyinResVector pinyin = g2p_man->hanziToPinyin(
    "中国",
    Pinyin::ManTone::Style::TONE
);
```

### 3. TONE2 - 声调在韵母后

拼音声调在各个韵母之后，用数字 [1-4] 表示。

```cpp
// 输入：中国
// 输出：zho1ng guo2
PinyinResVector pinyin = g2p_man->hanziToPinyin(
    "中国",
    Pinyin::ManTone::Style::TONE2
);
```

### 4. TONE3 - 声调在拼音后

拼音声调在各个拼音之后，用数字 [1-4] 表示。

```cpp
// 输入：中国
// 输出：zhong1 guo2
PinyinResVector pinyin = g2p_man->hanziToPinyin(
    "中国",
    Pinyin::ManTone::Style::TONE3
);
```

## 高级功能

### 1. 多音字处理

启用 `candidates` 参数可以获取多音字的所有候选读音。

```cpp
std::string text = "银行";

// 启用候选读音返回
PinyinResVector pinyin = g2p_man->hanziToPinyin(
    text,
    Pinyin::ManTone::Style::TONE3,
    Pinyin::Error::Default,
    true  // 返回所有候选拼音
);

// 遍历每个字的拼音结果
for (const auto& py : pinyin) {
    std::cout << "汉字: " << py.hanzi << std::endl;
    std::cout << "默认拼音: " << py.pinyin << std::endl;

    // 显示所有候选读音
    if (!py.candidates.empty()) {
        std::cout << "候选读音: ";
        for (const auto& candidate : py.candidates) {
            std::cout << candidate << " ";
        }
        std::cout << std::endl;
    }
}
```

### 2. 错误处理

使用 `Error` 参数控制无法转换字符的处理方式。

```cpp
// Default: 保留原始字符
PinyinResVector pinyin1 = g2p_man->hanziToPinyin(
    "hello世界123",
    Pinyin::ManTone::Style::NORMAL,
    Pinyin::Error::Default  // 保留 "hello" 和 "123"
);

// Ignore: 忽略无法转换的字符
PinyinResVector pinyin2 = g2p_man->hanziToPinyin(
    "hello世界123",
    Pinyin::ManTone::Style::NORMAL,
    Pinyin::Error::Ignore  // 只输出 "shi jie"
);
```

### 3. 特殊参数

```cpp
PinyinResVector pinyin = g2p_man->hanziToPinyin(
    text,
    Pinyin::ManTone::Style::TONE,
    Pinyin::Error::Default,
    true,   // candidates: 是否返回候选读音
    true,   // v_to_u: 将 v 转换为 ü
    true    // neutral_tone_with_five: 用 5 表示轻声
);
```

### 4. 获取单个汉字的默认拼音

```cpp
// 获取单个汉字的拼音列表
std::vector<std::string> pinyinList = g2p_man->getDefaultPinyin(
    "了",
    Pinyin::ManTone::Style::TONE3,
    false,  // v_to_u
    false   // neutral_tone_with_five
);

// pinyinList 可能包含: ["le5", "liao3"]
```

## Windows 宽字符串转换

在 Windows 项目中，通常需要在宽字符串（`std::wstring`）和 UTF-8 字符串（`std::string`）之间转换。

### 使用项目提供的工具函数

```cpp
#include "../util/StringUtil.hpp"

// 宽字符串转 UTF-8
std::wstring wideStr = L"你好世界";
std::string utf8Str = wide_to_utf8(wideStr);

// UTF-8 转宽字符串
std::string utf8Result = "ni hao shi jie";
std::wstring wideResult = utf8_to_wide(utf8Result);
```

### 手动实现转换函数

```cpp
// 宽字符串转 UTF-8
std::string WStringToUTF8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0],
                                          (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                        &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// UTF-8 转宽字符串
std::wstring UTF8ToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0],
                                          (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(),
                        &wstrTo[0], size_needed);
    return wstrTo;
}
```

## 完整示例

### 示例 1: 基本转换

```cpp
#include <iostream>
#include <filesystem>
#include <cpp-pinyin/G2pglobal.h>
#include <cpp-pinyin/Pinyin.h>

int main() {
    // 初始化
    const auto dictPath = std::filesystem::current_path() / "dict";
    Pinyin::setDictionaryPath(dictPath);
    auto g2p = std::make_unique<Pinyin::Pinyin>();

    // 转换
    std::string text = "春眠不觉晓";
    auto pinyin = g2p->hanziToPinyin(text, Pinyin::ManTone::Style::TONE3);

    std::cout << "原文: " << text << std::endl;
    std::cout << "拼音: " << pinyin.toStdStr() << std::endl;
    // 输出: chun1 mian2 bu4 jue2 xiao3

    return 0;
}
```

### 示例 2: 批量转换（多种风格）

```cpp
std::vector<std::string> sentences = {
    "你好世界",
    "中华人民共和国",
    "人工智能技术发展迅速"
};

for (const auto& sentence : sentences) {
    // 无声调
    auto normal = g2p->hanziToPinyin(sentence, Pinyin::ManTone::Style::NORMAL);

    // 标准声调
    auto tone = g2p->hanziToPinyin(sentence, Pinyin::ManTone::Style::TONE);

    // 数字声调
    auto tone3 = g2p->hanziToPinyin(sentence, Pinyin::ManTone::Style::TONE3);

    std::cout << "原文: " << sentence << std::endl;
    std::cout << "  NORMAL: " << normal.toStdStr() << std::endl;
    std::cout << "  TONE:   " << tone.toStdStr() << std::endl;
    std::cout << "  TONE3:  " << tone3.toStdStr() << std::endl;
}
```

### 示例 3: 多音字检测与处理

```cpp
struct PolyphonicTest {
    std::string text;
    std::string description;
};

std::vector<PolyphonicTest> tests = {
    {"我还行", "还(hái/huán)"},
    {"银行", "行(háng/xíng)"},
    {"长大", "长(zhǎng/cháng)"},
    {"长度", "长(zhǎng/cháng)"}
};

for (const auto& test : tests) {
    auto pinyin = g2p->hanziToPinyin(
        test.text,
        Pinyin::ManTone::Style::TONE3,
        Pinyin::Error::Default,
        true  // 返回候选读音
    );

    std::cout << "文本: " << test.text
              << " (" << test.description << ")" << std::endl;
    std::cout << "拼音: " << pinyin.toStdStr() << std::endl;

    // 显示每个字的候选读音
    for (const auto& py : pinyin) {
        if (py.candidates.size() > 1) {
            std::cout << "  '" << py.hanzi << "' 的候选: ";
            for (size_t i = 0; i < py.candidates.size(); ++i) {
                std::cout << py.candidates[i];
                if (i < py.candidates.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
}
```

## API 参考

### 核心类

#### `Pinyin::Pinyin`

主要的拼音转换类，继承自 `ChineseG2p`。

**构造函数:**
```cpp
explicit Pinyin();
```

**主要方法:**

```cpp
// 转换字符串为拼音
PinyinResVector hanziToPinyin(
    const std::string& hans,
    ManTone::Style style = ManTone::Style::TONE,
    Error error = Default,
    bool candidates = true,
    bool v_to_u = false,
    bool neutral_tone_with_five = false
) const;

// 转换字符串数组为拼音
PinyinResVector hanziToPinyin(
    const std::vector<std::string>& hans,
    ManTone::Style style = ManTone::Style::TONE,
    Error error = Default,
    bool candidates = true,
    bool v_to_u = false,
    bool neutral_tone_with_five = false
) const;

// 获取单个汉字的默认拼音
std::vector<std::string> getDefaultPinyin(
    const std::string& hanzi,
    ManTone::Style style = ManTone::Style::TONE,
    bool v_to_u = false,
    bool neutral_tone_with_five = false
) const;

// 繁体转简体
std::string tradToSim(const std::string& oneHanzi) const;

// 判断是否为多音字
bool isPolyphonic(const std::string& oneHanzi) const;
```

### 数据结构

#### `PinyinRes`

单个字符的拼音转换结果。

```cpp
struct PinyinRes {
    std::string hanzi;                      // 原始汉字（UTF-8）
    std::string pinyin;                     // 默认拼音（UTF-8）
    std::vector<std::string> candidates;    // 候选拼音（多音字）
    bool error = true;                      // 是否转换失败
};
```

#### `PinyinResVector`

拼音结果向量，提供便捷方法。

```cpp
class PinyinResVector : public std::vector<PinyinRes> {
public:
    // 转换为字符串向量
    std::vector<std::string> toStdVector() const;

    // 转换为字符串（使用分隔符，默认空格）
    std::string toStdStr(const std::string& delimiter = " ") const;
};
```

### 枚举类型

#### `ManTone::Style`

拼音风格枚举。

```cpp
enum Style {
    NORMAL = 0,  // 无声调: zhong guo
    TONE = 1,    // 标准声调: zhōng guó
    TONE2 = 2,   // 声调在韵母后: zho1ng guo2
    TONE3 = 8    // 声调在拼音后: zhong1 guo2
};
```

#### `Error`

错误处理枚举。

```cpp
enum Error {
    Default = 0,  // 保留原始字符
    Ignore = 1    // 忽略无法转换的字符
};
```

### 全局函数

```cpp
namespace Pinyin {
    // 设置字典路径
    void setDictionaryPath(const std::filesystem::path& dir);

    // 获取字典路径
    std::filesystem::path dictionaryPath();

    // 字符类型判断
    bool isHanzi(const char16_t& c);
    bool isLetter(const char16_t& c);
    bool isDigit(const char16_t& c);
    bool isSpace(const char16_t& c);
}
```

## 性能特点

- **速度快**: 在 Intel i3-8100 上约 1,000,000 字/秒
- **准确率高**:
  - 无声调测试（20万字歌词数据集）准确率 99.95%
  - 带声调测试（CPP_Dataset 约7.9万句）准确率 90.3%
- **支持范围**: Unicode 范围 [0x4E00 - 0x9FFF]
- **支持繁简**: 支持繁体和简体中文

## 注意事项

1. **字典路径**: 必须在使用前调用 `setDictionaryPath()` 设置字典路径
2. **UTF-8 编码**: 所有输入输出字符串必须使用 UTF-8 编码
3. **字典文件**: 确保 `dict` 文件夹在程序运行目录下（CMake 会自动复制）
4. **内存管理**: 使用智能指针管理 `Pinyin` 对象生命周期
5. **线程安全**: 可以创建多个 `Pinyin` 实例用于多线程环境

## 测试用例

项目包含完整的测试用例，位于 `tests/SplitWordsTest.cpp`。运行测试：

```bash
./scripts/run_debug_test.cmd
```

测试内容包括：
- 基本拼音转换
- 多种拼音风格
- 多音字处理
- 候选读音获取
- 字符串编码转换

## 常见问题

### Q: 为什么输出乱码？

A: 确保使用 UTF-8 编码，Windows 控制台需要设置：
```cpp
_setmode(_fileno(stdout), _O_U8TEXT);
```

### Q: 字典文件找不到？

A: 检查以下几点：
1. 确认 `dict` 文件夹在可执行文件同目录
2. 检查 CMakeLists.txt 中的复制命令是否正确
3. 手动复制 `3rdparty/cpp-pinyin/res/dict` 到输出目录

### Q: 多音字判断不准确？

A: 启用上下文分析功能（`candidates=true`），并根据实际需求选择合适的候选读音。

### Q: 性能优化建议？

A:
1. 重用 `Pinyin` 对象，避免频繁创建销毁
2. 批量处理时使用向量接口
3. 如果不需要候选读音，设置 `candidates=false`

## 相关链接

- **项目主页**: https://github.com/wolfgitpr/cpp-pinyin
- **字典制作工具**: https://github.com/wolfgitpr/pinyin-makedict
- **Python 版本**: https://github.com/mozillazg/python-pinyin
- **Go 版本**: https://github.com/mozillazg/go-pinyin
- **Rust 版本**: https://github.com/mozillazg/rust-pinyin
- **C# 版本**: https://github.com/wolfgitpr/csharp-pinyin

## 许可证

cpp-pinyin 使用开源许可证，具体请查看项目的 LICENSE 文件。

---

**文档版本**: 1.0
**最后更新**: 2025-10-15
**适用项目**: CandyLauncher
