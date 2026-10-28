add_executable(stickyaim_core_tests
    test_curve.cpp
    test_deadzone.cpp
    test_pipeline.cpp
    test_smoothing.cpp
    test_noise_drift.cpp
    test_stats.cpp
    test_config.cpp
    test_analysis.cpp)
target_link_libraries(stickyaim_core_tests PRIVATE stickyaim::core GTest::gtest_main)
target_compile_definitions(stickyaim_core_tests PRIVATE STICKYAIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

include(GoogleTest)
gtest_discover_tests(stickyaim_core_tests DISCOVERY_TIMEOUT 60)

if(TARGET stickyaim_engine)
    add_executable(stickyaim_engine_tests test_engine.cpp)
    target_link_libraries(stickyaim_engine_tests PRIVATE stickyaim::engine GTest::gtest_main)
    gtest_discover_tests(stickyaim_engine_tests DISCOVERY_TIMEOUT 60)
endif()
