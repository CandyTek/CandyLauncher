cmake_minimum_required(VERSION 3.10)

project(tests C CXX)

add_executable(SplitWordsTest
		SplitWordsTest.cpp
		../util/PinyinHelper.hpp
		../util/PinyinHelper.cpp

)

set_target_properties(SplitWordsTest PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		WIN32_EXECUTABLE OFF         # 明确不是 GUI
)

#add_compile_options(-finput-charset=utf8 -fexec-charset=utf8)
target_link_libraries(SplitWordsTest PRIVATE cpp-pinyin::cpp-pinyin pugixml::pugixml)

# 复制多音字拼音库字典
add_custom_command(TARGET SplitWordsTest POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-Dsrc=${CMAKE_SOURCE_DIR}/3rdparty/cpp-pinyin/res/dict
		-Ddst=$<TARGET_FILE_DIR:SplitWordsTest>/dict
		-P "${CMAKE_SOURCE_DIR}/cmakefile/copy_if_missing.cmake"
		VERBATIM
)

add_custom_command(TARGET SplitWordsTest POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_BINARY_DIR}/bin/cpp-pinyind.dll"
		"${CMAKE_BINARY_DIR}/tests/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_BINARY_DIR}/3rdparty/pugixml/pugixml.dll"
		"${CMAKE_BINARY_DIR}/tests/"
)


#add_custom_command(TARGET SplitWordsTest POST_BUILD
#        COMMAND "${CMAKE_COMMAND}" -E copy_directory
#        "${CMAKE_SOURCE_DIR}/3rdparty/cpp-pinyin/res/dict"
#        "$<TARGET_FILE_DIR:SplitWordsTest>/dict"
#)

if (MSVC)
	target_link_options(SplitWordsTest PRIVATE /SUBSYSTEM:CONSOLE)
	target_compile_options(SplitWordsTest PRIVATE /utf-8)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_options(SplitWordsTest PRIVATE -Wl,-subsystem,console)
endif ()


add_test(NAME SplitWordsTest COMMAND SplitWordsTest)
