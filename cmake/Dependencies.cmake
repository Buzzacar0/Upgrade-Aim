# Third-party dependencies. All are fetched at configure time at pinned versions
# so builds are reproducible. See docs/DEPENDENCIES.md for maintenance status.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# nlohmann/json — profile and report serialization (MIT, actively maintained).
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

if(STICKYAIM_BUILD_TESTS)
    # GoogleTest — unit tests (BSD-3, actively maintained).
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if(STICKYAIM_WITH_VIGEM AND WIN32)
    # ViGEmClient — user-mode SDK for the ViGEmBus driver (MIT).
    # NOTE: upstream was archived/retired by its author in Nov 2023. It still
    # builds and the bus driver still works on Windows 10/11, but it receives no
    # fixes. Pinned to the final upstream commit.
    # SOURCE_SUBDIR points at a directory without a CMakeLists.txt, so
    # MakeAvailable only downloads the sources; the library is defined below.
    FetchContent_Declare(vigemclient
        GIT_REPOSITORY https://github.com/nefarius/ViGEmClient.git
        GIT_TAG        b66d02d57e32cc8595369c53418b843e958649b4
        GIT_SHALLOW    FALSE
        SOURCE_SUBDIR  stickyaim-no-cmake)
    FetchContent_MakeAvailable(vigemclient)
    # Build the client ourselves instead of add_subdirectory(): upstream's
    # install() rules are not needed and its .rc needs no special handling.
    add_library(ViGEmClient STATIC
        ${vigemclient_SOURCE_DIR}/src/ViGEmClient.cpp)
    target_include_directories(ViGEmClient
        PRIVATE ${vigemclient_SOURCE_DIR}/src
        PUBLIC  ${vigemclient_SOURCE_DIR}/include)
    target_link_libraries(ViGEmClient PUBLIC setupapi)
    target_compile_definitions(ViGEmClient PRIVATE _UNICODE UNICODE)
endif()
