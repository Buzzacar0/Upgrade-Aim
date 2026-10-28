find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(UpgradeAim WIN32
    main.cpp
    app.rc
    resources/ui.qrc
    i18n.hpp i18n.cpp i18n_pt_br.cpp
    launcher.hpp launcher.cpp
    app_controller.hpp app_controller.cpp
    main_window.hpp main_window.cpp
    widgets/stick_view.hpp widgets/stick_view.cpp
    widgets/curve_editor.hpp widgets/curve_editor.cpp
    widgets/time_plot.hpp widgets/time_plot.cpp
    widgets/histogram_view.hpp widgets/histogram_view.cpp
    widgets/context_editor.hpp widgets/context_editor.cpp
    widgets/ui_helpers.hpp widgets/ui_helpers.cpp
    widgets/grouped_list.hpp widgets/grouped_list.cpp
    pages/pages.hpp
    pages/dashboard_page.cpp
    pages/stick_lab_page.cpp
    pages/profiles_page.cpp
    pages/levels_page.cpp
    pages/status_page.cpp
    pages/latency_page.cpp
    pages/compat_page.cpp
    pages/testing_page.cpp
    pages/settings_page.cpp)

target_include_directories(UpgradeAim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(UpgradeAim PRIVATE stickyaim::engine Qt6::Widgets dwmapi)
target_compile_definitions(UpgradeAim PRIVATE STICKYAIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}" QT_NO_KEYWORDS)
if(MSVC)
    # Qt headers trigger C4251/C4127 noise under /W4.
    target_compile_options(UpgradeAim PRIVATE /wd4251 /wd4127)
endif()

install(TARGETS UpgradeAim RUNTIME DESTINATION bin)
