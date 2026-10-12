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

# 引用公共 CMake 模块（也会把 ExamplePlugin.plugin.json 清单复制到 DLL 旁边）
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)
