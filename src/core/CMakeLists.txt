add_library(stickyaim_core STATIC
    types.cpp
    curve.cpp
    deadzone.cpp
    response.cpp
    smoothing.cpp
    calibration.cpp
    profile.cpp
    pipeline.cpp
    stats.cpp
    logger.cpp
    config_io.cpp
    signals.cpp
    analysis.cpp
    io.cpp
    device_db.cpp
    recording.cpp)

target_include_directories(stickyaim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(stickyaim_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(stickyaim_core PUBLIC cxx_std_20)
add_library(stickyaim::core ALIAS stickyaim_core)
