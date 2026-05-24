cmake_minimum_required(VERSION 3.16)
project(PluginBrowserPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_SOURCES
		PluginBrowserPlugin.cpp
		../Plugin.hpp
		PluginBrowserAction.hpp
		PluginBrowserPluginData.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)
