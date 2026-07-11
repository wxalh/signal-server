cmake_minimum_required(VERSION 3.21)

project(signal_server VERSION 1.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are disabled. Use a preset or configure with -B out/build/<name>.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_SKIP_RPATH OFF)

if(MSVC)
    add_compile_options(/utf-8 /wd4819 /wd4319 /wd5105)
endif()

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/spdlog)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(${THIRD_PARTY_DIR}/json)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${THIRD_PARTY_DIR}/asio/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE _WEBSOCKETPP_CPP11_STL_)

add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${THIRD_PARTY_DIR}/websocketpp)
target_link_libraries(websocketpp INTERFACE asio)

find_package(Threads REQUIRED)

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

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_compile_definitions(${PROJECT_NAME} PRIVATE SIGNAL_SERVER_VERSION="${PROJECT_VERSION}")
target_link_libraries(${PROJECT_NAME} PRIVATE
    websocketpp
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)

if(SIGNAL_SERVER_PORTABLE_GLIBC)
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN/lib"
    )
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
file(GLOB CONFIG_INIS "${CMAKE_CURRENT_SOURCE_DIR}/*.ini")
install(FILES ${CONFIG_INIS} DESTINATION .)

foreach(config ${CONFIG_INIS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${config}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endforeach()
