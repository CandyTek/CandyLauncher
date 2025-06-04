cmake_minimum_required(VERSION 3.10)

add_executable(SplitWordsTest SplitWordsTest.cpp)
set_target_properties(SplitWordsTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

add_test(NAME SplitWordsTest COMMAND SplitWordsTest)
