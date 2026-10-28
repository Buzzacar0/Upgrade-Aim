add_executable(upgradeaim-cli main.cpp)
target_link_libraries(upgradeaim-cli PRIVATE stickyaim::engine)
target_compile_definitions(upgradeaim-cli PRIVATE STICKYAIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
install(TARGETS upgradeaim-cli RUNTIME DESTINATION bin)
