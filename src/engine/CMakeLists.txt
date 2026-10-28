add_library(stickyaim_engine STATIC engine.cpp)
target_include_directories(stickyaim_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(stickyaim_engine PUBLIC stickyaim::core stickyaim::platform)
add_library(stickyaim::engine ALIAS stickyaim_engine)
