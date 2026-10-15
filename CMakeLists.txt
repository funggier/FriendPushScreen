cmake_minimum_required(VERSION 3.25)

project(FriendPushScreen VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(UNICODE _UNICODE NOMINMAX)

include(cmake/warnings.cmake)


add_executable(FriendPushScreen WIN32
    resources/FriendPushScreen.rc
    src/app/main.cpp
    src/app/application.cpp
    src/ui/main_window.cpp
    src/ui/ui_state.cpp
    src/ui/settings_store.cpp
    src/ui/settings_panel.cpp
    src/ui/theme.cpp
    src/capture/capture_source.cpp
    src/capture/capture_source_catalog.cpp
    src/capture/desktop_duplication_capture.cpp
    src/capture/windows_graphics_capture.cpp
    src/capture/media_foundation_capture.cpp
    src/graphics/d3d11_device.cpp
    src/graphics/placeholder_renderer.cpp
    src/graphics/preview_renderer.cpp
    src/diagnostics/diagnostics_formatter.cpp
    src/diagnostics/diagnostics_service.cpp
    src/diagnostics/logger.cpp
    src/platform/windows/os_probe.cpp
    src/platform/windows/cpu_memory_probe.cpp
    src/platform/windows/dxgi_adapter_probe.cpp
    src/platform/windows/monitor_probe.cpp
    src/platform/windows/window_probe.cpp
    src/platform/windows/network_probe.cpp
    src/platform/windows/runtime_paths.cpp
    src/media/capture_device_probe.cpp
    src/media/codec_capability_probe.cpp
    src/media/video_codec_model.cpp
    src/media/video_frame_clock.cpp
    src/media/encoded_access_unit_queue.cpp
    src/media/mft_video_codec_catalog.cpp
    src/media/video_loop_metrics.cpp
    src/media/mf_d3d11_device_manager.cpp
    src/media/mft_video_media_type.cpp
)
target_include_directories(FriendPushScreen PRIVATE
    src
    resources
)
fps_apply_warnings(FriendPushScreen)
target_link_libraries(FriendPushScreen PRIVATE
    Advapi32
    Comdlg32
    Comctl32
    D3D11
    D3DCompiler
    Dxgi
    Dwmapi
    Gdi32
    Iphlpapi
    Mf
    Mfplat
    Mfreadwrite
    Mfuuid
    Ole32
    User32
    UxTheme
    WindowsApp
)

enable_testing()

add_executable(FriendPushScreenTests
    tests/test_main.cpp
    tests/diagnostics_formatter_tests.cpp
    tests/platform_filter_tests.cpp
    tests/diagnostics_service_tests.cpp
    tests/ui_state_tests.cpp
    tests/settings_store_tests.cpp
    tests/capture_source_catalog_tests.cpp
    tests/main_window_layout_tests.cpp
    tests/preview_renderer_tests.cpp
    tests/runtime_paths_tests.cpp
    tests/video_pipeline_model_tests.cpp
    tests/video_loop_metrics_tests.cpp
    tests/mft_video_media_type_tests.cpp
    src/capture/capture_source.cpp
    src/capture/capture_source_catalog.cpp
    src/capture/desktop_duplication_capture.cpp
    src/capture/windows_graphics_capture.cpp
    src/capture/media_foundation_capture.cpp
    src/diagnostics/diagnostics_formatter.cpp
    src/diagnostics/diagnostics_service.cpp
    src/diagnostics/logger.cpp
    src/platform/windows/os_probe.cpp
    src/platform/windows/cpu_memory_probe.cpp
    src/platform/windows/dxgi_adapter_probe.cpp
    src/platform/windows/monitor_probe.cpp
    src/platform/windows/window_probe.cpp
    src/platform/windows/network_probe.cpp
    src/platform/windows/runtime_paths.cpp
    src/media/capture_device_probe.cpp
    src/media/codec_capability_probe.cpp
    src/media/video_codec_model.cpp
    src/media/video_frame_clock.cpp
    src/media/encoded_access_unit_queue.cpp
    src/media/mft_video_codec_catalog.cpp
    src/media/video_loop_metrics.cpp
    src/media/mf_d3d11_device_manager.cpp
    src/media/mft_video_media_type.cpp
    src/graphics/d3d11_device.cpp
    src/graphics/preview_renderer.cpp
    src/ui/main_window.cpp
    src/ui/ui_state.cpp
    src/ui/settings_store.cpp
    src/ui/settings_panel.cpp
    src/ui/theme.cpp
)
target_include_directories(FriendPushScreenTests PRIVATE
    tests
    src
    resources
)
fps_apply_warnings(FriendPushScreenTests)
target_link_libraries(FriendPushScreenTests PRIVATE
    Advapi32
    Dxgi
    Dwmapi
    User32
    UxTheme
    WindowsApp
    Iphlpapi
    Mf
    Mfplat
    Mfreadwrite
    Mfuuid
    D3D11
    D3DCompiler
    Gdi32
    Comdlg32
    Comctl32
)

add_test(NAME FriendPushScreenTests COMMAND FriendPushScreenTests)
