cmake_minimum_required(VERSION 3.21)
project(FairyKekkaiWorkshopCpp LANGUAGES CXX)

# exe 图标资源（.rc）需要 RC 语言
if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Svg Network)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Network)
if(WIN32 AND NOT OpenCV_DIR AND EXISTS "D:/CODE/opencv-4.12.0/build/OpenCVConfig.cmake")
    set(OpenCV_DIR "D:/CODE/opencv-4.12.0/build" CACHE PATH "OpenCV package directory")
endif()
find_package(OpenCV REQUIRED COMPONENTS core videoio)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/Qt-Fluent-Widgets/qtfluentwidgets
    ${CMAKE_CURRENT_BINARY_DIR}/qtfluentwidgets-build
)

file(GLOB_RECURSE FKW_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

# exe 图标：把 Python 端同一份 logo.ico 写进 exe 的文件资源，
# 这样资源管理器、任务栏、快捷方式都能取到图标（窗口图标在 main.cpp 里单独设置）。
# 用 configure_file 生成 .rc，避免 rc.exe 对相对路径的解析差异。
set(FKW_APP_RC "")
if(WIN32)
    get_filename_component(FKW_APP_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/../app/resource/images/logo.ico" ABSOLUTE)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
        @ONLY
    )
    set(FKW_APP_RC "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

add_executable(Fairy-Kekkai-Workshop-Cpp WIN32
    ${FKW_SOURCES}
    resources/app_assets.qrc
    ${FKW_APP_RC}
)
# 产物名与 Python 版保持一致（Fairy-Kekkai-Workshop.exe）：两代共用同一个 AppId 与安装目录，
# 名字统一后安装包/快捷方式/关联项都能直接沿用。
# target 名保留 Fairy-Kekkai-Workshop-Cpp，脚本里的 --target 与 $<TARGET_FILE_DIR:...> 都不用改。
set_target_properties(Fairy-Kekkai-Workshop-Cpp PROPERTIES OUTPUT_NAME "Fairy-Kekkai-Workshop")
target_include_directories(Fairy-Kekkai-Workshop-Cpp PRIVATE src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(Fairy-Kekkai-Workshop-Cpp PRIVATE qtfluentwidgets Qt6::Widgets Qt6::Svg Qt6::Network ${OpenCV_LIBS})
target_compile_definitions(Fairy-Kekkai-Workshop-Cpp PRIVATE
    FKW_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
if(WIN32)
    target_link_libraries(Fairy-Kekkai-Workshop-Cpp PRIVATE dwmapi)
    set(FKW_OPENCV_BIN "${OpenCV_DIR}/x64/vc16/bin")
    if(EXISTS "${FKW_OPENCV_BIN}/opencv_world4120.dll")
        add_custom_command(TARGET Fairy-Kekkai-Workshop-Cpp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FKW_OPENCV_BIN}/opencv_videoio_ffmpeg4120_64.dll"
                "$<IF:$<CONFIG:Debug>,${FKW_OPENCV_BIN}/opencv_world4120d.dll,${FKW_OPENCV_BIN}/opencv_world4120.dll>"
                "$<TARGET_FILE_DIR:Fairy-Kekkai-Workshop-Cpp>"
        )
    endif()
endif()
if(MSVC)
    target_compile_options(Fairy-Kekkai-Workshop-Cpp PRIVATE /utf-8)
    # 管理员启动：在 exe 清单里声明 requireAdministrator，启动时由 UAC 提权
    # （双击、命令行、快捷方式都一样）
    target_link_options(Fairy-Kekkai-Workshop-Cpp PRIVATE
        "/MANIFESTUAC:level='requireAdministrator'")
endif()
