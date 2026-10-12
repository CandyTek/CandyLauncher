cmake_minimum_required(VERSION 3.16)
project(ExamplePlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
		ExampleAction.hpp
		ExamplePlugin.cpp
		../sdk/CandyPluginSdk.hpp
		ExamplePluginData.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 清单文件放在 DLL 旁边，主程序不加载 DLL 也能读到元数据
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/ExamplePlugin.plugin.json"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/ExamplePlugin.plugin.json"
)
