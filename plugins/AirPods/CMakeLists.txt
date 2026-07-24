cmake_minimum_required(VERSION 3.16)
project(AirPodsBatteryPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PLUGIN_SOURCES
	AirPodsPlugin.cpp
	AirPodsBatteryService.cpp
	AirPodsBatteryService.hpp
	AirPodsAction.hpp
	AirPodsPluginData.hpp
	../Plugin.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE windowsapp)
