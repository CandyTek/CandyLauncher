cmake_minimum_required(VERSION 3.16)
project(SmallToolBoxPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
		ToolBoxAction.hpp
		ToolBoxPlugin.cpp
		../Plugin.hpp
		ToolBoxPluginData.hpp	
		qrcodegen.cpp
		qrcodegen.hpp
		ToolBoxUtil.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE gdiplus)
