cmake_minimum_required(VERSION 3.25)

project(FriendPushScreen VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(UNICODE _UNICODE NOMINMAX)

include(cmake/warnings.cmake)

add_executable(FriendPushScreen WIN32
    src/app/main.cpp
    src/app/application.cpp
    src/ui/main_window.cpp
    src/ui/ui_state.cpp
    src/ui/settings_store.cpp
    src/ui/settings_panel.cpp
    src/graphics/d3d11_device.cpp
    src/graphics/placeholder_renderer.cpp
    src/diagnostics/diagnostics_formatter.cpp
    src/diagnostics/diagnostics_service.cpp
    src/diagnostics/logger.cpp
    src/platform/windows/os_probe.cpp
    src/platform/windows/cpu_memory_probe.cpp
    src/platform/windows/dxgi_adapter_probe.cpp
    src/platform/windows/monitor_probe.cpp
    src/platform/windows/window_probe.cpp
    src/platform/windows/network_probe.cpp
    src/media/capture_device_probe.cpp
    src/media/codec_capability_probe.cpp
)
target_include_directories(FriendPushScreen PRIVATE src)
fps_apply_warnings(FriendPushScreen)
target_link_libraries(FriendPushScreen PRIVATE
    Advapi32
    Comdlg32
    Comctl32
    D3D11
    Dxgi
    Gdi32
    Iphlpapi
    Mf
    Mfplat
    Mfuuid
    Ole32
    User32
)

enable_testing()

add_executable(FriendPushScreenTests
    tests/test_main.cpp
    tests/diagnostics_formatter_tests.cpp
    tests/platform_filter_tests.cpp
    tests/diagnostics_service_tests.cpp
    tests/ui_state_tests.cpp
    tests/settings_store_tests.cpp
    src/diagnostics/diagnostics_formatter.cpp
    src/diagnostics/diagnostics_service.cpp
    src/diagnostics/logger.cpp
    src/platform/windows/os_probe.cpp
    src/platform/windows/cpu_memory_probe.cpp
    src/platform/windows/dxgi_adapter_probe.cpp
    src/platform/windows/monitor_probe.cpp
    src/platform/windows/window_probe.cpp
    src/platform/windows/network_probe.cpp
    src/media/capture_device_probe.cpp
    src/media/codec_capability_probe.cpp
    src/graphics/d3d11_device.cpp
    src/ui/main_window.cpp
    src/ui/ui_state.cpp
    src/ui/settings_store.cpp
    src/ui/settings_panel.cpp
)
target_include_directories(FriendPushScreenTests PRIVATE
    tests
    src
)
fps_apply_warnings(FriendPushScreenTests)
target_link_libraries(FriendPushScreenTests PRIVATE
    Advapi32
    Dxgi
    User32
    Iphlpapi
    Mf
    Mfplat
    Mfuuid
    D3D11
    Gdi32
    Comdlg32
    Comctl32
)

add_test(NAME FriendPushScreenTests COMMAND FriendPushScreenTests)
