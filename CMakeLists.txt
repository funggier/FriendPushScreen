cmake_minimum_required(VERSION 3.25)

project(FriendPushScreen VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(UNICODE _UNICODE NOMINMAX)

include(cmake/warnings.cmake)
include(cmake/msquic.cmake)
include(cmake/opus.cmake)

add_library(fps_network STATIC src/network/peer_identity.cpp src/network/secure_peer_transport.cpp src/network/peer_protocol.cpp src/network/peer_session.cpp)
target_include_directories(fps_network PUBLIC src)
target_link_libraries(fps_network PUBLIC fps_msquic_headers Crypt32 Ncrypt Bcrypt Ws2_32 Advapi32)
fps_apply_warnings(fps_network)


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
    src/capture/feedback_protection.cpp
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
    src/media/mft_video_transform_activation.cpp
    src/media/mft_transform_session.cpp
    src/media/mf_video_codec.cpp
    src/media/video_frame_normalizer.cpp
    src/app/peer_video_controller.cpp
    src/media/video_peer_stream.cpp
    src/media/video_local_loop.cpp
    src/graphics/d3d11_video_converter.cpp
)
target_include_directories(FriendPushScreen PRIVATE
    src
    resources
)
fps_apply_warnings(FriendPushScreen)
target_link_libraries(FriendPushScreen PRIVATE
    OleAut32
    Ole32
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
    Winmm
)


add_executable(FriendPushScreenCodecProbe
    tools/codec_probe/main.cpp
    tools/codec_probe/d3d11_inventory.cpp
    src/media/video_codec_model.cpp
    src/media/mft_video_codec_catalog.cpp
    src/media/mft_video_transform_activation.cpp
)
target_include_directories(FriendPushScreenCodecProbe PRIVATE
    src
)
fps_apply_warnings(FriendPushScreenCodecProbe)
target_link_libraries(FriendPushScreenCodecProbe PRIVATE
    D3D11
    Dxgi
    Dxguid
    Mf
    Mfplat
    Mfuuid
    Ole32
)

# Physical D3D11 checks run explicitly; generic CI may have no video processor.
add_executable(FriendPushScreenConversionTests
    tests/video_conversion_runtime_tests.cpp
    src/graphics/d3d11_video_converter.cpp
)
target_include_directories(FriendPushScreenConversionTests PRIVATE src tests)
fps_apply_warnings(FriendPushScreenConversionTests)
target_link_libraries(FriendPushScreenConversionTests PRIVATE D3D11 Dxgi)

add_executable(FriendPushScreenCodecLoopProbe
    tools/codec_loop_probe/main.cpp
    src/media/mf_video_codec.cpp
    src/media/mft_transform_session.cpp
    src/media/mft_video_transform_activation.cpp
    src/media/mft_video_codec_catalog.cpp
    src/media/mft_video_media_type.cpp
    src/media/mf_d3d11_device_manager.cpp
    src/media/video_codec_model.cpp
    src/media/video_frame_clock.cpp
    src/media/encoded_access_unit_queue.cpp
    src/graphics/d3d11_video_converter.cpp
)
target_include_directories(FriendPushScreenCodecLoopProbe PRIVATE src)
fps_apply_warnings(FriendPushScreenCodecLoopProbe)
target_link_libraries(FriendPushScreenCodecLoopProbe PRIVATE D3D11 Dxgi Mf Mfplat Mfuuid Ole32 OleAut32)

enable_testing()

add_executable(FriendPushScreenTests
    tests/test_main.cpp
    tests/peer_identity_tests.cpp
    tests/peer_protocol_tests.cpp
    tests/peer_session_tests.cpp
    tests/feedback_protection_tests.cpp
    tests/diagnostics_formatter_tests.cpp
    tests/platform_filter_tests.cpp
    tests/diagnostics_service_tests.cpp
    tests/ui_state_tests.cpp
    tests/peer_ui_state_tests.cpp
    tests/peer_video_controller_tests.cpp
    tests/settings_store_tests.cpp
    tests/capture_source_catalog_tests.cpp
    tests/main_window_layout_tests.cpp
    tests/preview_renderer_tests.cpp
    tests/runtime_paths_tests.cpp
    tests/video_pipeline_model_tests.cpp
    tests/video_loop_metrics_tests.cpp
    tests/mft_video_media_type_tests.cpp
    src/capture/capture_source.cpp
    src/capture/feedback_protection.cpp
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
    src/media/mft_video_transform_activation.cpp
    src/graphics/d3d11_device.cpp
    src/graphics/preview_renderer.cpp
    src/ui/main_window.cpp
    src/ui/ui_state.cpp
    src/ui/settings_store.cpp
    src/ui/settings_panel.cpp
    src/ui/theme.cpp
    src/media/mft_transform_session.cpp
    src/media/mf_video_codec.cpp
    src/media/video_frame_normalizer.cpp
    src/app/peer_video_controller.cpp
    src/media/video_peer_stream.cpp
    src/media/video_local_loop.cpp
    src/graphics/d3d11_video_converter.cpp
    tests/video_local_loop_tests.cpp
    tests/video_peer_stream_tests.cpp
)
target_include_directories(FriendPushScreenTests PRIVATE
    tests
    src
    resources
)
fps_apply_warnings(FriendPushScreenTests)
target_link_libraries(FriendPushScreenTests PRIVATE
    Ole32
    OleAut32
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

add_executable(FriendPushScreenCaptureLoopProbe
    tools/capture_loop_probe/main.cpp
    src/media/video_frame_normalizer.cpp
    src/app/peer_video_controller.cpp
    src/media/video_peer_stream.cpp
    src/media/video_local_loop.cpp
    src/media/mf_video_codec.cpp
    src/media/mft_transform_session.cpp
    src/media/mft_video_transform_activation.cpp
    src/media/mft_video_codec_catalog.cpp
    src/media/mft_video_media_type.cpp
    src/media/mf_d3d11_device_manager.cpp
    src/media/video_codec_model.cpp
    src/media/video_frame_clock.cpp
    src/media/encoded_access_unit_queue.cpp
    src/media/video_loop_metrics.cpp
    src/media/capture_device_probe.cpp
    src/capture/capture_source.cpp
    src/capture/feedback_protection.cpp
    src/capture/capture_source_catalog.cpp
    src/capture/desktop_duplication_capture.cpp
    src/capture/windows_graphics_capture.cpp
    src/capture/media_foundation_capture.cpp
    src/platform/windows/monitor_probe.cpp
    src/platform/windows/window_probe.cpp
    src/platform/windows/runtime_paths.cpp
    src/diagnostics/logger.cpp
    src/graphics/d3d11_device.cpp
    src/graphics/d3d11_video_converter.cpp
    src/graphics/preview_renderer.cpp
    src/ui/theme.cpp
)
target_include_directories(FriendPushScreenCaptureLoopProbe PRIVATE src)
fps_apply_warnings(FriendPushScreenCaptureLoopProbe)
target_link_libraries(FriendPushScreenCaptureLoopProbe PRIVATE
    D3D11 D3DCompiler Dxgi Dxguid Mf Mfplat Mfreadwrite Mfuuid
    Ole32 OleAut32 User32 Gdi32 Dwmapi WindowsApp Psapi Comctl32 UxTheme Winmm)

target_link_libraries(FriendPushScreenTests PRIVATE fps_network Winmm)
fps_copy_msquic_runtime(FriendPushScreenTests)
target_link_libraries(FriendPushScreen PRIVATE fps_network)
fps_copy_msquic_runtime(FriendPushScreen)

add_executable(FriendPushScreenPeerProbe tools/peer_probe/main.cpp)
target_link_libraries(FriendPushScreenPeerProbe PRIVATE fps_network)
fps_apply_warnings(FriendPushScreenPeerProbe)
fps_copy_msquic_runtime(FriendPushScreenPeerProbe)

add_test(NAME FriendPushScreenSecurePeerTests COMMAND FriendPushScreenPeerProbe --self-test "${CMAKE_CURRENT_BINARY_DIR}/secure-peer-tests")
set_tests_properties(FriendPushScreenSecurePeerTests PROPERTIES TIMEOUT 45)

target_link_libraries(FriendPushScreenCaptureLoopProbe PRIVATE fps_network)

add_executable(FriendPushScreenVideoPeerProbe tools/video_peer_probe/main.cpp
    src/media/video_peer_stream.cpp src/media/video_frame_normalizer.cpp
    src/media/mf_video_codec.cpp src/media/mft_transform_session.cpp
    src/media/mft_video_transform_activation.cpp src/media/mft_video_codec_catalog.cpp
    src/media/mft_video_media_type.cpp src/media/mf_d3d11_device_manager.cpp
    src/media/video_codec_model.cpp src/media/video_frame_clock.cpp src/media/encoded_access_unit_queue.cpp
    src/graphics/d3d11_video_converter.cpp)
target_include_directories(FriendPushScreenVideoPeerProbe PRIVATE src)
target_link_libraries(FriendPushScreenVideoPeerProbe PRIVATE fps_network D3D11 Dxgi Mf Mfplat Mfuuid Ole32 OleAut32)
fps_apply_warnings(FriendPushScreenVideoPeerProbe)
# Physical capture qualification; reuse the established capture probe dependencies.
get_target_property(fps_feedback_probe_sources FriendPushScreenCaptureLoopProbe SOURCES)
list(REMOVE_ITEM fps_feedback_probe_sources tools/capture_loop_probe/main.cpp)
add_executable(FriendPushScreenFeedbackProbe tools/feedback_probe/main.cpp ${fps_feedback_probe_sources})
target_include_directories(FriendPushScreenFeedbackProbe PRIVATE src)
get_target_property(fps_feedback_probe_libraries FriendPushScreenCaptureLoopProbe LINK_LIBRARIES)
target_link_libraries(FriendPushScreenFeedbackProbe PRIVATE ${fps_feedback_probe_libraries})
fps_apply_warnings(FriendPushScreenFeedbackProbe)
fps_copy_msquic_runtime(FriendPushScreenFeedbackProbe)
target_sources(FriendPushScreenTests PRIVATE tests/audio_tests.cpp)
target_link_libraries(FriendPushScreenTests PRIVATE fps_audio)

target_sources(FriendPushScreenTests PRIVATE tests/wasapi_audio_tests.cpp)

add_executable(FriendPushScreenAudioProbe tools/audio_probe/main.cpp)
target_link_libraries(FriendPushScreenAudioProbe PRIVATE fps_audio)
fps_apply_warnings(FriendPushScreenAudioProbe)

target_sources(FriendPushScreenTests PRIVATE tests/peer_audio_tests.cpp)

target_sources(FriendPushScreenTests PRIVATE tests/audio_ui_tests.cpp)

foreach(fps_audio_app FriendPushScreen FriendPushScreenCaptureLoopProbe FriendPushScreenFeedbackProbe)
  target_link_libraries(${fps_audio_app} PRIVATE fps_audio)
endforeach()

set_tests_properties(FriendPushScreenTests PROPERTIES TIMEOUT 60)

# The portable app statically contains Opus; retain its upstream notice beside it.
add_custom_command(TARGET FriendPushScreen POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/third-party/opus-notice.txt"
    "$<TARGET_FILE_DIR:FriendPushScreen>/opus-notice.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "$<TARGET_FILE_DIR:FriendPushScreen>/LICENSE")

find_program(FPS_POWERSHELL NAMES powershell REQUIRED)
add_test(NAME FriendPushScreenSeparateProcessTests
  COMMAND "${FPS_POWERSHELL}" -NoProfile -File
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/secure_peer_process_tests.ps1"
    -ProbePath "$<TARGET_FILE:FriendPushScreenPeerProbe>"
    -WorkDirectory "${CMAKE_CURRENT_BINARY_DIR}/secure-peer-process-tests-$<CONFIG>")
set_tests_properties(FriendPushScreenSeparateProcessTests PROPERTIES TIMEOUT 30)

target_sources(FriendPushScreenTests PRIVATE tests/audio_worker_tests.cpp)
