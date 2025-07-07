cmake_minimum_required(VERSION 3.10)
project(WindowsProject1)

#set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 17)  # 或者选择其他 C++ 标准，如 14、20
include_directories(.)

add_executable(WindowsProject1
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
        
)

add_definitions(-DUNICODE -D_UNICODE)

# 仅在 Windows 平台上设置 GUI 子系统和相关链接选项
if (WIN32)
    # 设置 GUI 子系统（只有当你定义了 WinMain 时才这样设置）
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    # 检查是否是 Visual Studio 2022
    if (MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio 17")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
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

        # 检查是否是 MinGW
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
        endif()
    endif()
endif()

# 关闭优化
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

set(CMAKE_GENERATOR_PLATFORM x64)  # 或者 x86，依据需要设置
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

# 链接 GDI+ 和 Windows 控件库
# 链接 Windows 所需库
target_link_libraries(WindowsProject1
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
)

#include_directories("E:/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/atlmfc/include")

enable_testing()
add_subdirectory(tests)
add_subdirectory(3rdparty/RapidFuzz)

