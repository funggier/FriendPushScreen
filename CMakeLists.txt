cmake_minimum_required(VERSION 3.25)

project(FriendPushScreen VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(UNICODE _UNICODE)

include(cmake/warnings.cmake)

# Task 1 reserves the product target name. Task 7 replaces this placeholder
# with the first Win32 executable after its RED tests exist.
add_custom_target(FriendPushScreen)

enable_testing()

add_executable(FriendPushScreenTests
    tests/test_main.cpp
    tests/diagnostics_formatter_tests.cpp
    tests/platform_filter_tests.cpp
    src/diagnostics/diagnostics_formatter.cpp
    src/diagnostics/logger.cpp
    src/platform/windows/os_probe.cpp
    src/platform/windows/cpu_memory_probe.cpp
)
target_include_directories(FriendPushScreenTests PRIVATE
    tests
    src
)
fps_apply_warnings(FriendPushScreenTests)
target_link_libraries(FriendPushScreenTests PRIVATE Advapi32)

add_test(NAME FriendPushScreenTests COMMAND FriendPushScreenTests)
