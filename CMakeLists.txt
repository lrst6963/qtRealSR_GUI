cmake_minimum_required(VERSION 3.5)

project(qtRealSR_GUI VERSION 1.0 LANGUAGES CXX)

# 自动处理 UIC、MOC 和 RCC
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# set(QT_STATIC_PATH "/opt/qt-static")  # 静态库路径
# set(CMAKE_PREFIX_PATH "${QT_STATIC_PATH}")


# 设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# 查找 Qt 库（核心、GUI 和 Widgets 模块）
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# 如果没有找到 Qt6，则尝试 Qt5
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Core Gui Widgets REQUIRED)
endif()

# 源文件列表
set(SOURCES
    main.cpp
    mainwindow.cpp
    ImageProcessor.cpp
    VideoProcessor.cpp
)

# 头文件列表
set(HEADERS
    mainwindow.h
    ImageProcessor.h
    VideoProcessor.h
)

# UI 文件
set(UI_FILES
    mainwindow.ui
)

# 资源文件
set(RESOURCES
    res.qrc
)

# 翻译文件
set(TS_FILES
    untitled_zh_CN.ts
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCES}
)

# 链接 Qt 库.
if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )
    message(STATUS "使用 Qt6...")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
    )
    message(STATUS "使用 Qt5...")
endif()

set(QT_STATIC_PATH "J:/qt-static")
set(CMAKE_PREFIX_PATH "${QT_STATIC_PATH}")
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        winmm
        ws2_32
        iphlpapi
        user32
        gdi32
        advapi32
        shell32
    )
endif()

# macOS 平台专用设置
if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${MACOSX_BUNDLE_INFO_PLIST}"
    )
endif()

# 安装目标（可选）
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION . 
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
