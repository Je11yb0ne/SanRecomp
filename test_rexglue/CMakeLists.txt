# Minimal test project for rexglue-sdk v0.8.1.32
# Verifies that GTA V generated code compiles and links against pre-built SDK
cmake_minimum_required(VERSION 3.25)
project(gta5_rexglue_test LANGUAGES CXX)

# Require C++23 (rexglue SDK requirement)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to pre-built rexglue SDK
set(REXGLUE_SDK_ROOT "${CMAKE_SOURCE_DIR}/../tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64")

# Compiler/linker set via cmake command line (-DCMAKE_CXX_COMPILER=..., etc.)
# Do NOT set here — causes rc.exe detection issues

# Include rexglue SDK headers
include_directories("${REXGLUE_SDK_ROOT}/include")

# Link directories for pre-built libs
link_directories("${REXGLUE_SDK_ROOT}/lib")

# Generated rexglue source files (197 files from codegen)
file(GLOB REXGLUE_GENERATED_SOURCES
    "generated/default/*.cpp"
)

# Our minimal test entry point
add_executable(gta5_rexglue_test
    main.cpp
    polyfills.cpp
    ${REXGLUE_GENERATED_SOURCES}
)

# Pre-built libraries from rexglue SDK (rd = RelWithDebInfo)
target_link_libraries(gta5_rexglue_test
    # Core runtime (kernel, threading, memory, MMIO)
    rexruntimerd
    # Windowing/input
    SDL3-staticrd
    # Audio
    libavcodecrd
    libavutilrd
    # Utilities
    fmtrd
    spdlogrd
    snappyrd
    xxhashrd
    o1heaprd
    aes128rd
    disasmrd
    mspackrd
    # Profiling
    TracyClientrd
)

# Windows SDK libs needed by SDL3
target_link_libraries(gta5_rexglue_test
    # These are system libs SDL3 needs
    setupapi
    user32
    gdi32
    winmm
    imm32
    ole32
    oleaut32
    shell32
    version
    uuid
    # CRT math — provides roundevenf (C23)
    msvcrt
)

# Compiler flags matching SanRecomp + exceptions for rexglue
target_compile_options(gta5_rexglue_test PRIVATE
    /W3
    /bigobj
    /EHa  # Enable full C++ exception handling (rexglue uses try/throw)
)

# spdlog was built to use external fmt (not bundled)
# The SDK headers need this define to find fmt headers properly
target_compile_definitions(gta5_rexglue_test PRIVATE
    SPDLOG_FMT_EXTERNAL
)
