add_executable(stickyaim-cli main.cpp)
target_link_libraries(stickyaim-cli PRIVATE stickyaim::engine)
target_compile_definitions(stickyaim-cli PRIVATE STICKYAIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
install(TARGETS stickyaim-cli RUNTIME DESTINATION bin)
