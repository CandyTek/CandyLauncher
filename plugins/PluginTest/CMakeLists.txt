cmake_minimum_required(VERSION 3.10)

project(PluginTest C CXX)

add_executable(PluginTest
		Main.cpp
#		../BrowserHistory/BrowserHistoryPlugin.cpp
#		../Bookmark/BookmarkPlugin.cpp
#		../CalcExprtk/CalcPlugin.cpp
#		../CherryTree/CherryTreePlugin.cpp
		../Example/ExamplePlugin.cpp
#		../FeatureLaunch/FeatureLaunchPlugin.cpp
#		../Folder/FolderPlugin.cpp
#		../JetbrainsWorkspaces/Main.cpp
#		../OneNote/OneNotePlugin.cpp
#		../Registry/RegistryPlugin.cpp
#		../RunningApp/RunningAppPlugin.cpp
#		../Service/ServicePlugin.cpp
#		../TextFile/TextFilePlugin.cpp
#		../UnitConverter/UnitConverterPlugin.cpp
#		../ValueGenerator/Main.cpp
#		../VSCodeWorkspaces/Main.cpp
#		../WindowsSearch/WindowsSearchPlugin.cpp
#		../EverNote/EverNotePlugin.cpp
)


set_target_properties(PluginTest PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		WIN32_EXECUTABLE OFF         # 明确不是 GUI
)

#add_compile_options(-finput-charset=utf8 -fexec-charset=utf8)

if (MSVC)
	target_link_options(PluginTest PRIVATE /SUBSYSTEM:CONSOLE)
	target_compile_options(PluginTest PRIVATE /utf-8 /bigobj)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_link_options(PluginTest PRIVATE -Wl,-subsystem,console)
endif ()

# 包含头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_SOURCE_DIR}
		SYSTEM ${CMAKE_SOURCE_DIR}/3rdparty/sqlite-amalgamation
)

# 链接 SQLite3 库
target_link_libraries(${PROJECT_NAME} PRIVATE
		SQLite3
		gdiplus
		comctl32
		kernel32
		user32
		gdi32
		winspool
		shell32
		ole32
		oleaut32
		uuid
		comdlg32
		advapi32
		shlwapi
		dwmapi
		d2d1
		dwrite
		msimg32
		pugixml::pugixml
		runtimeobject.lib
		WinToast
		oledb
)



# Copy required DLLs to test directory
add_custom_command(TARGET PluginTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:pugixml-shared>
	$<TARGET_FILE_DIR:PluginTest>
)
add_custom_command(TARGET PluginTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:SQLite3>
	$<TARGET_FILE_DIR:PluginTest>
)

add_test(NAME PluginTest COMMAND PluginTest)

# 通过 DLL 加载书签插件，分别模拟当前主程序和 2.0 主程序
if (TARGET BookmarkPlugin)
	add_dependencies(PluginTest BookmarkPlugin)
	add_test(NAME PluginTestBookmarkDll COMMAND PluginTest $<TARGET_FILE:BookmarkPlugin>)
	add_test(NAME PluginTestBookmarkDllHost20 COMMAND PluginTest $<TARGET_FILE:BookmarkPlugin> --host-minor=0)
	add_test(NAME PluginTestBookmarkDllHost21 COMMAND PluginTest $<TARGET_FILE:BookmarkPlugin> --host-minor=1)
endif ()
# 示例插件在 2.1 主程序上：2.2 的 SDK 封装必须静默退化
add_test(NAME PluginTestHost21 COMMAND PluginTest --host-minor=1)

