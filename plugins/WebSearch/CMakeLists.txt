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
		SearchManagerWindow.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE comctl32
	libcmark-gfm-extensions_static libcmark-gfm_static)
target_compile_definitions(${PROJECT_NAME} PRIVATE CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/src
	${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/extensions
	${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/src
	${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/extensions)


