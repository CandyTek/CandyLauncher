cmake_minimum_required(VERSION 3.10)
# 应用版本号唯一来源：改这里即可，会同步到 AppVersion.h / VERSIONINFO 资源 / 关于页 / 更新检查
# GitHub Release 的 tag 使用 v<MAJOR>.<MINOR>.<PATCH>（例如 v1.2.0）
project(CandyLauncher VERSION 1.0.0)
#project(CandyLauncher C CXX)

# 用到17的许多特性，filesystem快速搜索文件的功能，方便的内联变量
set(CMAKE_CXX_STANDARD 17)
include_directories(.)

# 由 project(VERSION) 生成 common/AppVersion.h，供 C++ 与 .rc 共用
configure_file(common/AppVersion.h.in "${CMAKE_BINARY_DIR}/generated/common/AppVersion.h" @ONLY)
include_directories("${CMAKE_BINARY_DIR}/generated")

add_executable(CandyLauncher WIN32
		common/pch.h
		plugins/BaseAction.hpp
		plugins/PluginActionProxy.hpp
		plugins/PluginHostApi.hpp
		plugins/sdk/CandyPluginApi.h
		plugins/sdk/CandyPluginSdk.hpp
		common/framework.h
		util/json.hpp
		common/Resource.h
		common/I18n.hpp
		common/I18nResource.h
		common/targetver.h
		CandyLauncher.cpp
		util/PinyinHelper.hpp
		manager/ListViewManager.hpp
		util/MainTools.hpp
		view/GlassHelper.hpp
		manager/TrayMenuManager.hpp
		common/GlobalState.hpp
		manager/EditManager.hpp
		window/SettingsManager.hpp
		window/SettingsWindow.hpp
		window/SkinEditorWindow.hpp
		web/skin_editor.html
		common/Constants.hpp
		util/BaseTools.hpp
		CandyLauncher.rc
		icons/CandyLauncher.ico
		icons/close.ico
		icons/small.ico
		icons/refresh.ico
		icons/tab_plugin.ico
		icons/tab_normal.ico
		icons/tab_feature.ico
		icons/tab_skin.ico
		icons/tab_other.ico
		icons/tab_hotkey.ico
		icons/tab_about.ico
		common/settings.json
		view/ScrollViewController.hpp
		util/ShortcutUtil.hpp
		common/AppController.hpp
		common/GlobalState.cpp
		util/PinyinHelper.cpp
		common/app.manifest
		util/ThreadPool.hpp
		manager/SkinHelper.hpp
		model/TraverseOptions.hpp
		view/CustomButtonHelper.hpp
		view/SwitchView.hpp
		view/HotkeyEditView.hpp
		view/SwitchViewPainter.hpp
		view/EditScrollSynchronizer.hpp
		window/RenameDialog.hpp
		model/FileInfo.hpp
		view/ToolTipHelper.hpp
		view/SwitchState.hpp
		view/CustomBgColorView.hpp
		util/NinePatchImage.hpp
		manager/EditControlPainter.hpp
		window/Shell32IconViewer.hpp
		view/CustomComboBox.hpp
		util/BitmapUtil.hpp
		util/FileSystemTraverser.hpp
		model/SettingItem.hpp
		util/RunningWindowsTraverser.hpp
		util/RegistryAppsTraverser.hpp
		util/UWPAppsTraverser.hpp
		plugins/Bookmark/BookmarkUtil.hpp
		util/StringUtil.hpp
		util/UrlUtil.hpp
		util/LogUtil.hpp
		util/FileUtil.hpp
		util/MathUtil.hpp
		util/ColorUtil.hpp
		util/ClipboardUtil.hpp
		util/MyJsonUtil.hpp
		manager/TextMatchHelper.hpp
		manager/FuzzyScorer.hpp
		manager/MatchTextBuilder.hpp
		manager/UsageHistory.hpp
		model/ScoredAction.hpp
		util/MyToastUtil.hpp
		util/TaskWorker.hpp
		view/CustomEdit.hpp
		util/FullScreenDetectUtil.hpp
		util/ShortCutDetectUtil.hpp
		util/HotkeyUtils.hpp
		util/SvgUtil.hpp
		manager/ProcessManager.hpp
		manager/SkinFileUtil.hpp
		common/GuaranteedSkinJson.hpp
)

# 语言配置
set(LANG_RC_DEPS "${CMAKE_SOURCE_DIR}/common/I18nResource.h")

set(SUPPORTED_LANGUAGES
		"zh-CN"
		"en-US"
)

set(LANGUAGE_TARGETS)
foreach(lang ${SUPPORTED_LANGUAGES})
	string(REPLACE "-" "_" lang_target_suffix ${lang})
	set(target_name "CandyLauncherLang_${lang_target_suffix}")
	set(rc_file "lang/${lang}.rc")

	set_source_files_properties(${rc_file} PROPERTIES
			OBJECT_DEPENDS "${LANG_RC_DEPS}"
	)

	add_library(${target_name} MODULE ${rc_file} lang/ResourceDllEntry.cpp)

	set_target_properties(${target_name} PROPERTIES
			OUTPUT_NAME "${lang}"
			PREFIX ""
			RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:CandyLauncher>/lang"
			LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:CandyLauncher>/lang"
	)
	list(APPEND LANGUAGE_TARGETS ${target_name})

	target_compile_definitions(${target_name} PRIVATE UNICODE _UNICODE)
endforeach()
add_dependencies(CandyLauncher ${LANGUAGE_TARGETS})

target_precompile_headers(CandyLauncher
		PRIVATE
		common/pch.h
)


#set_source_files_properties(cli.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
#set_source_files_properties(cli.h PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
target_compile_definitions(CandyLauncher PRIVATE UNICODE _UNICODE
		CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE
		$<$<CONFIG:Debug>:DEBUG=1>
		$<$<CONFIG:Release>:NDEBUG=1>
		$<$<CONFIG:RelWithDebInfo>:REL_WITH_DEB_INFO_DEBUG=1>
)

# Add /bigobj flag for MSVC to handle large object files (ExprTk)
if (MSVC)
#	target_compile_options(CandyLauncher PRIVATE /bigobj /WX)
	target_compile_options(CandyLauncher PRIVATE /bigobj)
	# 屏蔽所有第三方库警告
	add_compile_options(/external:anglebrackets /external:W0)
	# 禁用特定的第三方库警告
	#    target_compile_options(CandyLauncher PRIVATE /wd4251 /wd4819)
#	target_compile_options(CandyLauncher PRIVATE /wd4251 /wd4996)
endif ()


# 仅在 Windows 平台上设置 GUI 子系统和相关链接选项
if (WIN32)
	# 设置 GUI 子系统（只有当你定义了 WinMain 时才这样设置）
	#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
	#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
	# 检查是否是 Visual Studio 2022
	if (MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio 17")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
		# 屏蔽外部库警告
#		add_compile_options(/W4 /WX /bigobj /external:anglebrackets /external:W0)
		add_compile_options(/bigobj /external:anglebrackets /external:W0)
		add_definitions(-DUSE_VS_TOOLCHAIN=1)
		set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# 编译资源文件（推荐使用外部命令或者手动添加）
		#        set(RESOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/CandyLauncher.res)
		#        add_custom_command(
		#                OUTPUT ${RESOURCE_FILE}
		#                COMMAND windres ${CMAKE_SOURCE_DIR}/CandyLauncher.rc -O coff -o ${RESOURCE_FILE}
		#                DEPENDS ${CMAKE_SOURCE_DIR}/CandyLauncher.rc
		#                COMMENT "Compiling resource file"
		#        )
		#        add_custom_target(resource_target ALL DEPENDS ${RESOURCE_FILE})
		#        add_dependencies(CandyLauncher resource_target)
		#        target_sources(CandyLauncher PRIVATE ${RESOURCE_FILE})
		# 关闭优化
		if (CMAKE_BUILD_TYPE STREQUAL "Debug")
			if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
				set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
			endif ()
		endif ()

		# 检查是否是 MinGW
		if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
			target_link_options(CandyLauncher PRIVATE -municode -mwindows)
			#            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
		endif ()
	endif ()
endif ()

#set(CMAKE_GENERATOR_PLATFORM x64)  # 或者 x86，依据需要设置
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
#add_compile_definitions(_WIN32_WINNT=0x0600)
#add_compile_definitions(NTDDI_VERSION=0x06010000)
add_compile_definitions(NTDDI_VERSION=0x0A000003)

# 内嵌的皮肤编辑器页面改动后需要重新编译资源
# VERSIONINFO reads the generated AppVersion.h; without this dependency the .res is not recompiled after a version bump
set_source_files_properties(CandyLauncher.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/web/skin_editor.html;${CMAKE_BINARY_DIR}/generated/common/AppVersion.h")

# 独立的皮肤编辑器程序 SkinEditor.exe，与 CandyLauncher.exe 输出到同一目录
add_executable(SkinEditor WIN32
		skin_editor/SkinEditorApp.cpp
		skin_editor/SkinEditor.rc
		common/app.manifest
		common/GlobalState.cpp
		window/SkinEditorWindow.hpp
		web/skin_editor.html
)
set_source_files_properties(skin_editor/SkinEditor.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/web/skin_editor.html;${CMAKE_BINARY_DIR}/generated/common/AppVersion.h")
target_precompile_headers(SkinEditor PRIVATE common/pch.h)
target_compile_definitions(SkinEditor PRIVATE UNICODE _UNICODE
		$<$<CONFIG:Debug>:DEBUG=1>
		$<$<CONFIG:Release>:NDEBUG=1>
		$<$<CONFIG:RelWithDebInfo>:REL_WITH_DEB_INFO_DEBUG=1>
)
if (MSVC)
	target_compile_options(SkinEditor PRIVATE /bigobj)
endif ()
set_target_properties(SkinEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:CandyLauncher>")
target_link_libraries(SkinEditor PRIVATE gdiplus comctl32 user32 gdi32 shell32 ole32 oleaut32 uuid comdlg32 advapi32 shlwapi dwmapi)
add_dependencies(CandyLauncher SkinEditor)

# 独立的 Shell 枚举进程 CandyUwpHelper.exe，与 CandyLauncher.exe 输出到同一目录。
# Folder 插件通过它枚举 shell:AppsFolder（含图标）、控制面板（含图标）和 "设置" 页面名称
# （util/UwpHelperClient.hpp），WinRT / StateRepository / MRT 和控制面板 applet 等几十个 DLL
# 只在这个短命进程里加载，不占主程序内存
add_executable(CandyUwpHelper WIN32
		uwp_helper/UwpHelperMain.cpp
		util/SystemSettingsEnumerator.hpp
		util/UwpAppsEnumerator.hpp
		util/UwpHelperProtocol.hpp
)
target_compile_definitions(CandyUwpHelper PRIVATE UNICODE _UNICODE)
set_target_properties(CandyUwpHelper PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:CandyLauncher>")
target_link_libraries(CandyUwpHelper PRIVATE ole32 shell32 shlwapi uuid gdi32 user32)
add_dependencies(CandyLauncher CandyUwpHelper)

if (CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
	set(EVERYTHING_LIB ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/lib/Everything64.lib)
else () # 32-bit
	set(EVERYTHING_LIB ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/lib/Everything32.lib)
endif ()

# 链接 GDI+ 和 Windows 控件库
# 链接 Windows 所需库
target_link_libraries(CandyLauncher PRIVATE
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
		dbghelp
		d2d1
		dwrite
		msimg32
		rapidfuzz::rapidfuzz
		cpp-pinyin::cpp-pinyin
		pugixml::pugixml
		${EVERYTHING_LIB}
		runtimeobject.lib
		WinToast
		oledb
		autoupdater::autoupdater
		lunasvg::lunasvg
		libcmark-gfm-extensions_static
		libcmark-gfm_static
)
if (MSVC)
	target_link_libraries(CandyLauncher PRIVATE oledb WinToast)
else ()
	target_link_libraries(CandyLauncher PRIVATE oledb32)
endif ()


#include_directories("E:/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/atlmfc/include")

target_include_directories(CandyLauncher PRIVATE
		${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/src
		${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/extensions
		${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/src
		${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/extensions
		SYSTEM ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/include
		SYSTEM ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/ipc
		SYSTEM ${CMAKE_SOURCE_DIR}/3rdparty/cpp-pinyin/include
		SYSTEM ${CMAKE_SOURCE_DIR}/3rdparty/sqlite-amalgamation
		SYSTEM ${CMAKE_SOURCE_DIR}/3rdparty/WinToast/src
)

add_custom_command(TARGET CandyLauncher POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/dll/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,Everything64.dll,Everything32.dll>
		$<TARGET_FILE_DIR:CandyLauncher>
)
add_custom_command(TARGET CandyLauncher POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:cpp-pinyin>
		$<TARGET_FILE_DIR:CandyLauncher>)

# 复制多音字拼音库字典
add_custom_command(TARGET CandyLauncher POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-Dsrc=${CMAKE_SOURCE_DIR}/3rdparty/cpp-pinyin/res/dict
		-Ddst=$<TARGET_FILE_DIR:CandyLauncher>/dict
		-P "${CMAKE_SOURCE_DIR}/cmakefile/copy_if_missing.cmake"
		VERBATIM
)
# 复制皮肤
add_custom_command(TARGET CandyLauncher POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		-Dsrc=${CMAKE_SOURCE_DIR}/skins
		-Ddst=$<TARGET_FILE_DIR:CandyLauncher>/skins
		-P "${CMAKE_SOURCE_DIR}/cmakefile/copy_if_missing.cmake"
		VERBATIM
)


enable_testing()
add_subdirectory(tests)
add_subdirectory(3rdparty/RapidFuzz)
add_subdirectory(3rdparty/cpp-pinyin)
# SQLite 以共享库 sqlite3.dll 形式构建，供 BrowserHistory / CherryTree / EverNote 等插件共用，避免每个插件各自静态链接一份
add_library(SQLite3 SHARED 3rdparty/sqlite-amalgamation/sqlite3.c)
set_target_properties(SQLite3 PROPERTIES OUTPUT_NAME sqlite3 DEBUG_POSTFIX d)
target_include_directories(SQLite3 PUBLIC ${CMAKE_SOURCE_DIR}/3rdparty/sqlite-amalgamation)
target_compile_definitions(SQLite3 PRIVATE
		"SQLITE_API=__declspec(dllexport)"
		SQLITE_DEFAULT_MEMSTATUS=0
		SQLITE_DQS=0
		SQLITE_LIKE_DOESNT_MATCH_BLOBS
		SQLITE_MAX_EXPR_DEPTH=0
		SQLITE_OMIT_DECLTYPE
		SQLITE_OMIT_DEPRECATED
		SQLITE_OMIT_LOAD_EXTENSION
		SQLITE_OMIT_PROGRESS_CALLBACK
		SQLITE_OMIT_SHARED_CACHE
		SQLITE_USE_ALLOCA
)
if (MSVC)
	target_compile_options(SQLite3 PRIVATE /w)
endif ()
add_custom_command(TARGET CandyLauncher POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:SQLite3>
		$<TARGET_FILE_DIR:CandyLauncher>)
add_dependencies(CandyLauncher SQLite3)
add_subdirectory(3rdparty/pugixml)
set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/cmark-gfm)
if (MSVC)
	target_compile_options(libcmark-gfm_static PRIVATE /utf-8)
	target_compile_options(libcmark-gfm-extensions_static PRIVATE /utf-8)
endif ()
if (MSVC)
add_subdirectory(3rdparty/WinToast)
endif ()
add_subdirectory(3rdparty/github-releases-autoupdater)
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PLUTOVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# Force static build for lunasvg and plutovg
set(BUILD_SHARED_LIBS_OLD ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(3rdparty/lunasvg)
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_OLD})


#plugins
add_subdirectory(plugins/RunningApp)
add_subdirectory(plugins/CalcExprtk)
add_subdirectory(plugins/Example)
add_subdirectory(plugins/Folder)
add_subdirectory(plugins/FeatureLaunch)
add_subdirectory(plugins/PluginBrowser)
add_subdirectory(plugins/Bookmark)
add_subdirectory(plugins/CherryTree)
add_subdirectory(plugins/UnitConverter)
add_subdirectory(plugins/JetbrainsWorkspaces)
add_subdirectory(plugins/VSCodeWorkspaces)
add_subdirectory(plugins/ValueGenerator)
add_subdirectory(plugins/BrowserHistory)
add_subdirectory(plugins/Registry)
add_subdirectory(plugins/Service)
add_subdirectory(plugins/WindowsSearch)
if (MSVC)
	include(CheckCXXSourceCompiles)
	unset(HAVE_ONENOTE_TYPELIB CACHE)# Recheck when CMake runs so installing or removing OneNote updates the target list.
	check_cxx_source_compiles([=[
#import "libid:0EA692EE-BB50-4E3C-AEF0-356D91732725" rename_namespace("OneNoteLib") raw_interfaces_only exclude("tagPOINT")
int main() { return 0; }
]=] HAVE_ONENOTE_TYPELIB)
	if (HAVE_ONENOTE_TYPELIB)
		add_subdirectory(plugins/OneNote)
	else ()
		message(STATUS "OneNote type library unavailable; skipping OneNotePlugin")
	endif ()
endif ()
add_subdirectory(plugins/TextFile)
add_subdirectory(plugins/EverNote)
add_subdirectory(plugins/VisualStudio)
add_subdirectory(plugins/SmallToolBox)
add_subdirectory(plugins/WebSearch)
add_subdirectory(plugins/NodeJsPlugin)
add_subdirectory(plugins/AirPods)
add_subdirectory(plugins/ScreenTools)

#add_subdirectory(plugins/ScreenTools2)#DEPRECATED


add_subdirectory(plugins/PluginTest)
# Plugins that are rarely changed can be commented to speed up compilation.
set(PLUGIN_DEPENDENCIES)

#list(APPEND PLUGIN_DEPENDENCIES ScreenToolsPlugin2)#DEPRECATED

list(APPEND PLUGIN_DEPENDENCIES SmallToolBoxPlugin)
list(APPEND PLUGIN_DEPENDENCIES FeatureLaunchPlugin)
list(APPEND PLUGIN_DEPENDENCIES PluginBrowserPlugin)
list(APPEND PLUGIN_DEPENDENCIES WebSearchPlugin)
list(APPEND PLUGIN_DEPENDENCIES NodeJsPlugin)
list(APPEND PLUGIN_DEPENDENCIES AirPodsBatteryPlugin)
list(APPEND PLUGIN_DEPENDENCIES ScreenToolsPlugin)
list(APPEND PLUGIN_DEPENDENCIES EverNotePlugin)
list(APPEND PLUGIN_DEPENDENCIES VisualStudioPlugin)
list(APPEND PLUGIN_DEPENDENCIES BrowserHistoryPlugin)
if (TARGET OneNotePlugin)
	list(APPEND PLUGIN_DEPENDENCIES OneNotePlugin)
endif ()
list(APPEND PLUGIN_DEPENDENCIES TextFilePlugin)
list(APPEND PLUGIN_DEPENDENCIES ExamplePlugin)
list(APPEND PLUGIN_DEPENDENCIES VSCodeWorkspacesPlugin)
list(APPEND PLUGIN_DEPENDENCIES JetbrainsWorkspacesPlugin)
list(APPEND PLUGIN_DEPENDENCIES FolderPlugin)
list(APPEND PLUGIN_DEPENDENCIES RunningAppPlugin)
list(APPEND PLUGIN_DEPENDENCIES CherryTreePlugin)
list(APPEND PLUGIN_DEPENDENCIES UnitConverterPlugin)
list(APPEND PLUGIN_DEPENDENCIES CalcPlugin)
list(APPEND PLUGIN_DEPENDENCIES ValueGeneratorPlugin)
list(APPEND PLUGIN_DEPENDENCIES BookmarkPlugin)
list(APPEND PLUGIN_DEPENDENCIES RegistryPlugin)
list(APPEND PLUGIN_DEPENDENCIES ServicePlugin)
list(APPEND PLUGIN_DEPENDENCIES WindowsSearchPlugin)


if (MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
	#	message(WARNING "come here")
	 set(PLUGIN_DEPENDENCIES
	 		FeatureLaunchPlugin
	 		PluginBrowserPlugin
	 		EverNotePlugin
	 		VisualStudioPlugin
	 		BrowserHistoryPlugin
	 		TextFilePlugin
	 		ExamplePlugin
	 		VSCodeWorkspacesPlugin
	 		JetbrainsWorkspacesPlugin
	 		FolderPlugin
	 		RunningAppPlugin
	 		CherryTreePlugin
	 		UnitConverterPlugin
	 		CalcPlugin
	 		ValueGeneratorPlugin
	 		BookmarkPlugin
	 		RegistryPlugin
	 		ServicePlugin
	 		WindowsSearchPlugin
			NodeJsPlugin
			AirPodsBatteryPlugin
	 )
	if (TARGET OneNotePlugin)
		list(APPEND PLUGIN_DEPENDENCIES OneNotePlugin)
	endif ()

	set(PLUGIN_OUTPUT_DIR $<TARGET_FILE_DIR:CandyLauncher>/plugins)

	if (PLUGIN_DEPENDENCIES)
		add_dependencies(CandyLauncher ${PLUGIN_DEPENDENCIES})

		foreach(plugin_target IN LISTS PLUGIN_DEPENDENCIES)
			add_custom_command(TARGET CandyLauncher POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E make_directory
					${PLUGIN_OUTPUT_DIR}
					COMMAND ${CMAKE_COMMAND} -E copy_if_different
					$<TARGET_FILE:${plugin_target}>
					${PLUGIN_OUTPUT_DIR}
					VERBATIM
			)
		endforeach()
	endif ()
else ()
	if (PLUGIN_DEPENDENCIES)
		add_dependencies(CandyLauncher ${PLUGIN_DEPENDENCIES})
	endif ()
endif ()

message(STATUS "CMAKE_GENERATOR = ${CMAKE_GENERATOR}")
message(STATUS "CMAKE_CXX_COMPILER_ID = ${CMAKE_CXX_COMPILER_ID}")

set(_third_party_targets plutovg lunasvg cpp-pinyin)

if(MSVC)
	set(_suppress_flags 
			/wd4244 /wd4996 /wd4018 /wd5287
			/wd4267 /wd4554 /wd4146 /wd4311
			INTERFACE /wd4251
	)
else()
	set(_suppress_flags -Wno-parentheses -Wno-conversion)
endif()

foreach(_tgt IN LISTS _third_party_targets)
	if(TARGET ${_tgt})
		target_compile_options(${_tgt} PRIVATE ${_suppress_flags})
	endif()
endforeach()

#if (MSVC)
#	set_target_properties(CandyLauncher PROPERTIES CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
#	add_compile_options(
#			$<$<CONFIG:Release>:/O2>
#			$<$<CONFIG:Release>:/Ob3>
#			$<$<CONFIG:Release>:/Oi>
#			$<$<CONFIG:Release>:/fp:fast>
#			$<$<CONFIG:Release>:/arch:AVX2>
#
#			$<$<CONFIG:Release>:/Gy>
#			$<$<CONFIG:Release>:/Gw>
#	)
#	add_link_options(
#			$<$<CONFIG:Release>:/OPT:REF>
#			$<$<CONFIG:Release>:/OPT:ICF>
#	)
#endif ()

# ---- Heap profiling build (memory attribution), never on by default ----
# cmake -DCL_HEAP_PROFILE=ON ... compiles util/HeapProfiler.cpp into the host, every plugin DLL and cpp-pinyin.dll:
# per module operator new / delete tracking with call stacks, dumped on the named event "CandyLauncherHeapDump".
option(CL_HEAP_PROFILE "Build with the in-process heap profiler (util/HeapProfiler.cpp)" OFF)
if (CL_HEAP_PROFILE AND MSVC)
	cmake_policy(SET CMP0079 NEW)
	function(cl_collect_targets out dir)
		get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
		get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
		foreach (_sub IN LISTS _subdirs)
			cl_collect_targets(_sub_targets "${_sub}")
			list(APPEND _targets ${_sub_targets})
		endforeach ()
		set(${out} ${_targets} PARENT_SCOPE)
	endfunction()
	cl_collect_targets(_cl_all_targets "${CMAKE_SOURCE_DIR}")
	set_source_files_properties("${CMAKE_SOURCE_DIR}/util/HeapProfiler.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
	foreach (_tgt IN LISTS _cl_all_targets)
		get_target_property(_type ${_tgt} TYPE)
		get_target_property(_srcdir ${_tgt} SOURCE_DIR)
		if (NOT _type MATCHES "^(EXECUTABLE|SHARED_LIBRARY)$")
			continue()
		endif ()
		if (NOT (_tgt STREQUAL "CandyLauncher" OR _srcdir MATCHES "/plugins/" OR _tgt MATCHES "pinyin"))
			continue()
		endif ()
		message(STATUS "Heap profiler attached to ${_tgt}")
		target_sources(${_tgt} PRIVATE "${CMAKE_SOURCE_DIR}/util/HeapProfiler.cpp")
		target_compile_definitions(${_tgt} PRIVATE CL_HEAP_PROFILE=1)
		target_link_libraries(${_tgt} PRIVATE dbghelp psapi)
	endforeach ()
endif ()
