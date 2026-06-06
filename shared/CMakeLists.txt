cmake_minimum_required(VERSION 3.20)
# Shared types/utils - header-only, no compiled library needed
# But expose include directory for other targets
add_library(BBRenderShared INTERFACE)
target_include_directories(BBRenderShared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
