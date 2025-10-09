cmake_minimum_required(VERSION 3.16)
project(WindowsSearchPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
	WindowsSearchPlugin.cpp
	../Plugin.hpp
	WindowsSearchAction.hpp
	WindowsSearchPluginData.hpp
	WindowsSearchUtil.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 链接 Windows Search API 和 OLE DB 库
target_link_libraries(${PROJECT_NAME} PRIVATE
	oleaut32
	ole32
	uuid
)
