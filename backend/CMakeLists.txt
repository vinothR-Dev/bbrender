cmake_minimum_required(VERSION 3.20)

# ─── BB Render Server Daemon ─────────────────────────────────────────────────
add_executable(BBRenderServer
    ${CMAKE_CURRENT_SOURCE_DIR}/server_main.cpp
)

target_include_directories(BBRenderServer PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(BBRenderServer PRIVATE
    Threads::Threads
    ${PLATFORM_LIBS}
)

target_compile_options(BBRenderServer PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>: -O3 -march=native -Wall -Wextra >
    $<$<CXX_COMPILER_ID:MSVC>:      /O2 /W4 /WX- >
)

# ─── BB Render Worker Agent ───────────────────────────────────────────────────
add_executable(BBRenderWorker
    ${CMAKE_CURRENT_SOURCE_DIR}/worker_main.cpp
)

target_include_directories(BBRenderWorker PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(BBRenderWorker PRIVATE
    Threads::Threads
    ${PLATFORM_LIBS}
)

target_compile_options(BBRenderWorker PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>: -O3 -march=native -Wall -Wextra >
    $<$<CXX_COMPILER_ID:MSVC>:      /O2 /W4 /WX- >
)
