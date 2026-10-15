#include "platform/windows/runtime_paths.h"

#include "test_support.h"

#include <filesystem>
#include <fstream>

void RunRuntimePathsTests() {
    const auto root =
        std::filesystem::temp_directory_path() /
        "FriendPushScreen-runtime-path-tests";

    std::error_code error;
    std::filesystem::remove_all(root, error);
    error.clear();

    const auto executable_dir =
        root / "portable-copy";
    const auto local_app_data =
        root / "local-app-data";

    std::filesystem::create_directories(
        executable_dir,
        error);
    FPS_EXPECT_TRUE(!error);

    const auto installed_root =
        fps::platform::windows::
            ResolveDataRoot(
                executable_dir,
                local_app_data);

    FPS_EXPECT_EQ(
        installed_root,
        local_app_data /
            "FriendPushScreen");

    {
        std::ofstream marker{
            executable_dir /
                "portable.flag",
            std::ios::binary |
                std::ios::trunc};
        marker << "portable\n";
    }

    FPS_EXPECT_TRUE(
        fps::platform::windows::
            IsPortableMode(
                executable_dir));

    const auto portable_root =
        fps::platform::windows::
            ResolveDataRoot(
                executable_dir,
                local_app_data);

    FPS_EXPECT_EQ(
        portable_root,
        executable_dir /
            "data");

    std::filesystem::remove_all(
        root,
        error);
}
