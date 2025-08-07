cmake_minimum_required(VERSION 3.16)
project(signal_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt5 components
find_package(Qt5 COMPONENTS Core WebSockets Network REQUIRED)

# Find spdlog
find_package(spdlog REQUIRED)

# Set up Qt5
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    add_compile_options(/utf-8)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/wd4819>")
endif()

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
    spdlog::spdlog
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
