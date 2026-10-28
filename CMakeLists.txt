cmake_minimum_required(VERSION 3.24)

project(UpgradeAim
    VERSION 0.1.0
    DESCRIPTION "Upgrade Aim: laboratorio de entrada de controle para Windows"
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STICKYAIM_BUILD_TESTS "Build unit tests (GoogleTest)" ON)
option(STICKYAIM_BUILD_APP   "Build the Qt 6 desktop application" ON)
option(STICKYAIM_BUILD_CLI   "Build the command-line diagnostic tool" ON)
option(STICKYAIM_WITH_VIGEM  "Compile the optional ViGEm virtual-controller output (requires ViGEmBus at runtime)" ON)
option(STICKYAIM_CORE_ONLY   "Build only the portable processing core and its tests" OFF)

if(STICKYAIM_CORE_ONLY OR NOT WIN32)
    set(STICKYAIM_BUILD_APP OFF)
    set(STICKYAIM_BUILD_CLI OFF)
    set(STICKYAIM_WITH_VIGEM OFF)
endif()

# Third-party code first, so it is built with its own warning level rather
# than this project's /W4.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

add_subdirectory(src/core)
if(WIN32 AND NOT STICKYAIM_CORE_ONLY)
    add_subdirectory(src/platform)
    add_subdirectory(src/engine)
endif()

if(STICKYAIM_BUILD_CLI AND WIN32)
    add_subdirectory(src/cli)
endif()

if(STICKYAIM_BUILD_APP AND WIN32)
    add_subdirectory(src/app)
endif()

if(STICKYAIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(DIRECTORY profiles devices DESTINATION share/upgradeaim)
