# GTA V RexGlue — manual link (pre-built SDK v0.8.1.32)
cmake_minimum_required(VERSION 3.25)
project(gta5_rexglue LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REXGLUE "${CMAKE_SOURCE_DIR}/../tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64")
include_directories("${REXGLUE}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/generated/default")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../SanRecomp")
link_directories("${REXGLUE}/lib")

file(GLOB REXGLUE_SRCS "generated/default/*.cpp")

add_executable(gta5_rexglue WIN32
    main.cpp
    polyfills.cpp
    "${REXGLUE}/share/rexglue/windowed_app_main_win.cpp"
    ${REXGLUE_SRCS}
)
target_compile_options(gta5_rexglue PRIVATE /W3 /bigobj /EHa)
target_compile_definitions(gta5_rexglue PRIVATE SPDLOG_FMT_EXTERNAL)
target_link_libraries(gta5_rexglue
    rexruntimerd SDL3-staticrd
    libavcodecrd libavutilrd
    fmtrd spdlogrd snappyrd xxhashrd o1heaprd aes128rd disasmrd mspackrd
    TracyClientrd
    setupapi user32 gdi32 winmm imm32 ole32 oleaut32 shell32 version uuid comctl32
    msvcrt
)
