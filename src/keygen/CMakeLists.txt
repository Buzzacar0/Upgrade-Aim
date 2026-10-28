# Owner-only tool that issues activation codes. Not installed and not shipped.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_executable(upgradeaim-keygen WIN32 main.cpp)
target_link_libraries(upgradeaim-keygen PRIVATE stickyaim::platform Qt6::Widgets shell32)
target_compile_definitions(upgradeaim-keygen PRIVATE STICKYAIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(upgradeaim-keygen PRIVATE /wd4251 /wd4127)
endif()
