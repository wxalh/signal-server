cmake_minimum_required(VERSION 3.21)

project(signal_server VERSION 1.0.0 LANGUAGES CXX)

if(MSVC)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8 /wd4819 /wd4319 /wd5105)
endif()

set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable automoc, autouic, and autorcc for Qt project
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QT5_DIR}")

# Find Qt5 components
find_package(Qt5 COMPONENTS Core WebSockets Network REQUIRED)

# ---------------------------
# third_party dependencies (source build)
# ---------------------------
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ===== spdlog =====
# 只编译依赖库：关闭安装/示例/测试/benchmark
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
# 使用共享库构建 spdlog（便于运行时动态链接和调试）
set(SPDLOG_BUILD_SHARED ON CACHE BOOL "" FORCE)
# 使用spdlog自带fmt（避免额外引入fmt依赖）
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL_HO OFF CACHE BOOL "" FORCE)
# 强制使用header-only fmt（spdlog内置 third_party/spdlog/include/spdlog/fmt/bundled）
set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/spdlog)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
set(SOURCES
    src/main.cpp
    src/websocketserver.cpp
    src/websocketclient.cpp
    src/usermanager.cpp
    src/messagehandler.cpp
    src/rcsuser.cpp
    src/wsmsg.cpp
    src/logger_manager.cpp
    src/config_util.cpp
)

# Header files
set(HEADERS
    src/websocketserver.h
    src/websocketclient.h
    src/usermanager.h
    src/messagehandler.h
    src/rcsuser.h
    src/wsmsg.h
    src/logger_manager.h
    src/config_util.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link Qt5 libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Network
    Qt5::WebSockets
    spdlog
)

if(TARGET spdlog AND SPDLOG_BUILD_SHARED)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:spdlog>
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()

file(GLOB CONFIG_INIS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ini"
)
foreach(f ${CONFIG_INIS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${f}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endforeach()

# 立即启用 origin 用于构建时 rpath（使 $ORIGIN 在 build_rpath 中起作用）
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_SKIP_RPATH OFF)

# 设置 RPATH，让程序优先从相对路径查找动态库
if(UNIX AND NOT APPLE)
    # Linux: 设置 RPATH 为 $ORIGIN（可执行文件所在目录）
    # 因为库文件会被拷贝到可执行文件目录，所以只需要 $ORIGIN
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# 设置可执行文件输出目录（兼容单/多配置）
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/release"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/release"
    )
else()
    # 单配置生成器（如 Ninja）：根据 CMAKE_BUILD_TYPE 决定输出目录
    if(NOT CMAKE_BUILD_TYPE)
        # 默认到 release
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/debug")
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release")
    endif()
    # 也为 multi-config 生成器提供通用占位（不会影响单配置）
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<CONFIG>")
endif()

# Windows: 拷贝 DLL 到输出目录（如果存在）
if(WIN32)
    # 拷贝 Qt DLL（使用 windeployqt 工具自动部署）
    get_target_property(_qt5_qmake_location Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt5_install_prefix "${_qt5_qmake_location}" DIRECTORY)
    get_filename_component(_qt5_install_prefix "${_qt5_install_prefix}" DIRECTORY)
    set(_qt5_windeployqt "${_qt5_install_prefix}/bin/windeployqt.exe")

    if(EXISTS "${_qt5_windeployqt}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${_qt5_windeployqt}"
            "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "正在使用 windeployqt 部署 Qt 依赖..."
        )
    else()
        message(WARNING "未找到 windeployqt 工具, Qt DLL 需要手动拷贝")
    endif()
endif()
