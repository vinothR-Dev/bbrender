cmake_minimum_required(VERSION 3.20)

set(BBRENDER_FRONTEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.cpp
)

set(BBRENDER_FRONTEND_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/job_queue_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/worker_monitor_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/farm_stats_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/log_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets/job_detail_widget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs/job_submit_dialog.h
)

if(WIN32)
    set(BBRENDER_WIN_RC ${CMAKE_CURRENT_SOURCE_DIR}/../resources/bbrender.rc)
    if(EXISTS ${BBRENDER_WIN_RC})
        list(APPEND BBRENDER_FRONTEND_SOURCES ${BBRENDER_WIN_RC})
    endif()
    set(WIN32_FLAG WIN32)
endif()

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE bbrender.icns)
    set(MACOS_FLAG MACOSX_BUNDLE)
endif()

add_executable(BBRenderFarm
    ${WIN32_FLAG}
    ${MACOS_FLAG}
    ${BBRENDER_FRONTEND_SOURCES}
    ${BBRENDER_FRONTEND_HEADERS}
)

target_include_directories(BBRenderFarm PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BBRenderFarm PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
    Qt6::Concurrent
    Threads::Threads
    ${PLATFORM_LIBS}
)

if(BBRENDER_USE_OPENSSL)
    target_link_libraries(BBRenderFarm PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# Compiler optimizations
target_compile_options(BBRenderFarm PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -O2 -march=native -ffast-math
        -Wall -Wextra -Wpedantic
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /O2 /arch:AVX2 /W4 /WX-
        /wd4068 /wd4244 /wd4267
    >
)

# Qt deployment (Windows)
if(WIN32)
    add_custom_command(TARGET BBRenderFarm POST_BUILD
        COMMAND ${Qt6_DIR}/../../../bin/windeployqt
            --no-translations --no-system-d3d-compiler
            $<TARGET_FILE:BBRenderFarm>
        COMMENT "Running windeployqt..."
    )
endif()

# macOS bundle
if(APPLE)
    set_target_properties(BBRenderFarm PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "BB Render Farm"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.bbrender.farm"
    )
    add_custom_command(TARGET BBRenderFarm POST_BUILD
        COMMAND macdeployqt $<TARGET_FILE_DIR:BBRenderFarm>/../..
        COMMENT "Running macdeployqt..."
    )
endif()
