cmake_minimum_required(VERSION 3.16)
project(VisualStudioPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
		VisualStudioPlugin.cpp
		../Plugin.hpp
		VisualStudioAction.hpp
		VisualStudioPluginData.hpp
		VisualStudioUtil.hpp
		${CMAKE_SOURCE_DIR}/3rdparty/pugixml/src/pugixml.cpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 添加 pugixml 头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/pugixml/src)

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)
