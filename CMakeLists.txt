cmake_minimum_required(VERSION 3.20)
project(BBRender VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BBRENDER_BUILD_FRONTEND   "Build Qt GUI frontend"      ON)
option(BBRENDER_BUILD_BACKEND    "Build C++ backend daemon"   ON)
option(BBRENDER_BUILD_WORKER     "Build render worker agent"  ON)
option(BBRENDER_BUILD_TESTS      "Build unit tests"           OFF)
option(BBRENDER_USE_OPENSSL      "Enable TLS for networking"  ON)

# Platform detection
if(WIN32)
    add_compile_definitions(BB_PLATFORM_WINDOWS)
    set(PLATFORM_LIBS ws2_32 mswsock)
elseif(APPLE)
    add_compile_definitions(BB_PLATFORM_MACOS)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    set(PLATFORM_LIBS ${CORE_FOUNDATION})
else()
    add_compile_definitions(BB_PLATFORM_LINUX)
    set(PLATFORM_LIBS pthread dl)
endif()

# Find packages
find_package(Threads REQUIRED)

if(BBRENDER_USE_OPENSSL)
    find_package(OpenSSL REQUIRED)
    add_compile_definitions(BB_USE_OPENSSL)
endif()

if(BBRENDER_BUILD_FRONTEND)
    find_package(Qt6 REQUIRED COMPONENTS
        Core Widgets Network Charts Concurrent
    )
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()

# SQLite (bundled)
add_library(sqlite3 STATIC
    ${CMAKE_SOURCE_DIR}/shared/utils/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC ${CMAKE_SOURCE_DIR}/shared/utils)

# Shared library
add_subdirectory(shared)

# Backend daemon
if(BBRENDER_BUILD_BACKEND)
    add_subdirectory(backend)
endif()

# Frontend GUI
if(BBRENDER_BUILD_FRONTEND)
    add_subdirectory(frontend)
endif()

# Installation
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/ DESTINATION bin
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

message(STATUS "=== BB Render Farm v${PROJECT_VERSION} ===")
message(STATUS "Platform:  ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler:  ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build:     ${CMAKE_BUILD_TYPE}")
message(STATUS "Frontend:  ${BBRENDER_BUILD_FRONTEND}")
message(STATUS "Backend:   ${BBRENDER_BUILD_BACKEND}")
message(STATUS "OpenSSL:   ${BBRENDER_USE_OPENSSL}")
