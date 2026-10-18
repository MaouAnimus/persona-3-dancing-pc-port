cmake_minimum_required(VERSION 3.20)
project(P3DNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(NOMINMAX)

# Source files for shared rhythm engine
set(RHYTHM_SOURCES
    rhythm/rhythm_engine.hpp
    rhythm/rhythm_engine.cpp
    rhythm/mns_parser.hpp
    rhythm/mns_parser.cpp
    platform/timer.hpp
    audio/audio_clock.hpp
)

# 1. Main Windows Executable
add_executable(P3DNative
    WIN32
    app/main.cpp
    platform/window.hpp
    platform/window.cpp
    renderer/renderer.hpp
    renderer/renderer.cpp
    ${RHYTHM_SOURCES}
)

target_link_libraries(P3DNative PRIVATE
    d3d11
    dxgi
    d3dcompiler
    winmm
    xinput
    gdi32
    user32
)

# 2. Automated Test Executable (Console)
add_executable(test_rhythm
    tests/test_rhythm.cpp
    ${RHYTHM_SOURCES}
)
