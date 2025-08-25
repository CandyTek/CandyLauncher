cmake_minimum_required(VERSION 3.10)
project(WindowsProject1)
#project(WindowsProject1 C CXX)

#set(CMAKE_CXX_STANDARD 20)
# 因为要用到17的新特性，快速搜索文件的功能
set(CMAKE_CXX_STANDARD 17)
include_directories(.)

add_executable(WindowsProject1 WIN32
        pch.h
        
        ActionBase.hpp
        framework.h
        json.hpp
        ListedRunnerPlugin.cpp
        ListedRunnerPlugin.h
        Resource.h
        RunCommandAction.hpp
        targetver.h
        WindowsProject1.cpp
        WindowsProject1.h
        PinyinHelper.h
        ListViewManager.cpp
        ListViewManager.h
        MainTools.hpp
        GlassHelper.hpp
        TrayMenuManager.h
        TrayMenuManager.cpp
        actions/ActionNormal.hpp
        DataKeeper.hpp
        EditHelper.hpp
        SettingsHelper.hpp
        SettingWindow.hpp
        Constants.hpp
        BaseTools.hpp
        windowsproject1.rc
        WindowsProject1.ico
        small.ico
        settings.json
        ScrollViewHelper.hpp
        ShortCutHelper.hpp
        AppTools.hpp
        DataKeeper.cpp
        PinyinHelper.cpp
        app.manifest
        ThreadPool.h
        IndexedManager.hpp
        SkinHelper.h
        TraverseFilesHelper.hpp
        TraverseOptions.h
        CustomButtonHelper.hpp
        SwitchView.h
        HotkeyEditView.h
        SwitchViewStyle.h
        EditScrollSyncHelper.hpp
        RenameDialog.hpp
        ShortcutFixWindow.hpp
        FileInfo.hpp
        ToolTipHelper.hpp
)

target_precompile_headers(WindowsProject1
        PRIVATE
        pch.h
)


#set_source_files_properties(cli.c PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
#set_source_files_properties(cli.h PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
target_compile_definitions(WindowsProject1 PRIVATE UNICODE _UNICODE
        $<$<CONFIG:Debug>:DEBUG=1>
        $<$<CONFIG:Release>:NDEBUG=1>
)


# 仅在 Windows 平台上设置 GUI 子系统和相关链接选项
if (WIN32)
    # 设置 GUI 子系统（只有当你定义了 WinMain 时才这样设置）
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    # 检查是否是 Visual Studio 2022
    if (MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio 17")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
        add_compile_options(/W4 /WX)
        add_definitions(-DUSE_VS_TOOLCHAIN=1)
        set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # 编译资源文件（推荐使用外部命令或者手动添加）
#        set(RESOURCE_FILE ${CMAKE_CURRENT_BINARY_DIR}/windowsproject1.res)
#        add_custom_command(
#                OUTPUT ${RESOURCE_FILE}
#                COMMAND windres ${CMAKE_SOURCE_DIR}/windowsproject1.rc -O coff -o ${RESOURCE_FILE}
#                DEPENDS ${CMAKE_SOURCE_DIR}/windowsproject1.rc
#                COMMENT "Compiling resource file"
#        )
#        add_custom_target(resource_target ALL DEPENDS ${RESOURCE_FILE})
#        add_dependencies(WindowsProject1 resource_target)
#        target_sources(WindowsProject1 PRIVATE ${RESOURCE_FILE})
        # 关闭优化
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
            endif()
        endif()

        # 检查是否是 MinGW
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
            target_link_options(WindowsProject1 PRIVATE -municode -mwindows)
#            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
        endif()
    endif()
endif()

#set(CMAKE_GENERATOR_PLATFORM x64)  # 或者 x86，依据需要设置
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
#add_compile_definitions(_WIN32_WINNT=0x0600)
add_compile_definitions(NTDDI_VERSION=0x06010000)

if (CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
    set(EVERYTHING_LIB ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/lib/Everything64.lib)
else() # 32-bit
    set(EVERYTHING_LIB ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/lib/Everything32.lib)
endif()

# 链接 GDI+ 和 Windows 控件库
# 链接 Windows 所需库
target_link_libraries(WindowsProject1 PRIVATE
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
        rapidfuzz::rapidfuzz
        ${EVERYTHING_LIB}
        
)

#include_directories("E:/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/atlmfc/include")

include_directories(
        ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/include
        ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/ipc
)

add_custom_command(TARGET WindowsProject1 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/3rdparty/EverythingSDK/dll/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,Everything64.dll,Everything32.dll>
        $<TARGET_FILE_DIR:WindowsProject1>
)


enable_testing()
add_subdirectory(tests)
add_subdirectory(3rdparty/RapidFuzz)

