cmake_minimum_required(VERSION 3.16)
project(CalcPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
		CalcAction.hpp
		CalcPlugin.cpp
		latex.hpp
		../../util/BaseTools.hpp
		../../model/SettingItem.hpp
		exprtk/exprtk.hpp
		../Plugin.hpp
		ExprtkWrapper.hpp
		PluginData.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)
