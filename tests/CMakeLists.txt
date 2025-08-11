cmake_minimum_required(VERSION 3.10)

project(tests C CXX)

add_executable(SplitWordsTest
        SplitWordsTest.cpp
        ../PinyinHelper.h
        ../PinyinHelper.cpp
#        ../cli.c
#        ../everything_ipc.h
        
)

set_target_properties(SplitWordsTest PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        WIN32_EXECUTABLE OFF         # 明确不是 GUI
)

#add_compile_options(-finput-charset=utf8 -fexec-charset=utf8)


if (MSVC)
    target_link_options(SplitWordsTest PRIVATE /SUBSYSTEM:CONSOLE)
    target_compile_options(SplitWordsTest PRIVATE /utf-8)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(SplitWordsTest PRIVATE -Wl,-subsystem,console)
endif()

add_test(NAME SplitWordsTest COMMAND SplitWordsTest)
