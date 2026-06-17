# GTA V — RexGlue project with pre-built SDK
cmake_minimum_required(VERSION 3.25)
project(gta5_rexglue LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REXGLUE_SDK "${CMAKE_SOURCE_DIR}/../tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64")

# Find pre-built SDK via cmake package
find_package(rexglue 0.8.1 QUIET CONFIG
    PATHS "${REXGLUE_SDK}" NO_DEFAULT_PATH)
if(NOT rexglue_FOUND)
    message(FATAL_ERROR "rexglue SDK not found at ${REXGLUE_SDK}")
endif()

# Generated PPC code (197 files)
file(GLOB REXGLUE_SRCS "generated/default/*.cpp")

# GTA V app with ReXApp + platform entry from SDK
add_executable(gta5_rexglue WIN32
    main.cpp
    polyfills.cpp
    "${REXGLUE_SDK}/share/rexglue/windowed_app_main_win.cpp"
    ../SanRecomp/gta5_app.h
    ${REXGLUE_SRCS}
)
target_include_directories(gta5_rexglue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/generated/default
    ${CMAKE_CURRENT_SOURCE_DIR}/../SanRecomp
)
target_compile_options(gta5_rexglue PRIVATE /W3 /bigobj /EHa)
target_compile_definitions(gta5_rexglue PRIVATE SPDLOG_FMT_EXTERNAL)
target_link_libraries(gta5_rexglue PRIVATE rex::runtime)
