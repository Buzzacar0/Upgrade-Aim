add_library(stickyaim_platform STATIC
    win_util.cpp
    xinput_source.cpp
    rawinput.cpp
    system_probe.cpp
    vigem_sink.cpp)

target_include_directories(stickyaim_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(stickyaim_platform
    PUBLIC stickyaim::core
    PRIVATE xinput hid setupapi avrt winmm advapi32 user32 shell32 ole32)

if(STICKYAIM_WITH_VIGEM)
    target_link_libraries(stickyaim_platform PRIVATE ViGEmClient)
    target_compile_definitions(stickyaim_platform PRIVATE STICKYAIM_HAVE_VIGEM=1)
else()
    target_compile_definitions(stickyaim_platform PRIVATE STICKYAIM_HAVE_VIGEM=0)
endif()

add_library(stickyaim::platform ALIAS stickyaim_platform)

# Upstream ViGEm headers contain Latin-1 characters in comments.
if(MSVC)
    set_source_files_properties(vigem_sink.cpp PROPERTIES COMPILE_OPTIONS /wd4828)
endif()
