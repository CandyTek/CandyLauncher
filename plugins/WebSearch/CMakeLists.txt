cmake_minimum_required(VERSION 3.16)
project(WebSearchPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_SOURCES
		WebSearchPlugin.cpp
		../Plugin.hpp
		WebSearchAction.hpp
		WebSearchPluginData.hpp
		WebSearchUtil.hpp
		WebSearchHotkeyManager.hpp
		SearchManagerWindow.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE comctl32)


