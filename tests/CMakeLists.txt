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
		"$<TARGET_FILE:cpp-pinyin>"
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

add_executable(AutomationActionModelTest
	AutomationActionModelTest.cpp
	../plugins/Folder/AutomationActionModel.hpp
)
set_target_properties(AutomationActionModelTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_link_libraries(AutomationActionModelTest PRIVATE shell32)
if (MSVC)
	target_compile_options(AutomationActionModelTest PRIVATE /utf-8)
endif ()
add_test(NAME AutomationActionModelTest COMMAND AutomationActionModelTest)

add_executable(UrlUtilTest UrlUtilTest.cpp)
target_link_libraries(UrlUtilTest PRIVATE libcmark-gfm-extensions_static libcmark-gfm_static)
target_compile_definitions(UrlUtilTest PRIVATE CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
target_include_directories(UrlUtilTest PRIVATE
	${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/src
	${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/extensions
	${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/src
	${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/extensions)
if (MSVC)
	target_compile_options(UrlUtilTest PRIVATE /utf-8)
endif ()
add_test(NAME UrlUtilTest COMMAND UrlUtilTest)

#add_executable(ScreenToolsQrTest ScreenToolsQrTest.cpp)
#target_link_libraries(ScreenToolsQrTest PRIVATE screen_tools_qr gdiplus)
#add_test(NAME ScreenToolsQrTest COMMAND ScreenToolsQrTest
#	${CMAKE_SOURCE_DIR}/plugins/ScreenTools2/test.png)

add_executable(ScreenToolsZxingQrTest ScreenToolsZxingQrTest.cpp
	${CMAKE_SOURCE_DIR}/plugins/ScreenTools/QrDecoder.cpp)
target_link_libraries(ScreenToolsZxingQrTest PRIVATE screen_tools_zxing_qr gdiplus)
add_test(NAME ScreenToolsZxingQrTest COMMAND ScreenToolsZxingQrTest
	${CMAKE_SOURCE_DIR}/plugins/ScreenTools2/test.png)

add_executable(CalcEngineTest CalcEngineTest.cpp ../plugins/CalcExprtk/CalcEngine.hpp)
set_target_properties(CalcEngineTest PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
target_include_directories(CalcEngineTest PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/ExprTk)
if (MSVC)
	target_compile_options(CalcEngineTest PRIVATE /utf-8 /bigobj)
endif ()
add_test(NAME CalcEngineTest COMMAND CalcEngineTest)
