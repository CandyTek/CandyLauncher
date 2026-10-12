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
	../sdk/CandyPluginSdk.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 列表图标：AirPodsPlugin.cpp 从 <exe 目录>\plugins\icon\ 读取这些 .ico，必须随 DLL 一起复制过去
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icon"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/ic_airpods_normal.ico"
		"${CMAKE_CURRENT_SOURCE_DIR}/ic_airpods_charge.ico"
		"${CMAKE_CURRENT_SOURCE_DIR}/ic_airpods_charge_case.ico"
		"${CMAKE_CURRENT_SOURCE_DIR}/ic_airpods_disconnected.ico"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/icon"
		VERBATIM
)

include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE windowsapp)
