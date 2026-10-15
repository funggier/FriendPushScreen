#include "platform/windows/runtime_paths.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace fps::platform::windows {
namespace {

std::filesystem::path LocalAppDataPath() {
    char* value = nullptr;
    std::size_t size = 0;

    if (_dupenv_s(
            &value,
            &size,
            "LOCALAPPDATA") != 0 ||
        value == nullptr ||
        size <= 1) {
        std::free(value);
        return {};
    }

    std::filesystem::path path{value};
    std::free(value);
    return path;
}

}  // namespace

std::filesystem::path ExecutableDirectory() {
    std::array<wchar_t, 32768> buffer{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    if (length == 0 ||
        length >= buffer.size()) {
        return
            std::filesystem::
                current_path();
    }

    return std::filesystem::path{
               buffer.data(),
               buffer.data() + length}
        .parent_path();
}

bool IsPortableMode(
    const std::filesystem::path&
        executable_directory) {
    std::error_code error;

    return std::filesystem::exists(
        executable_directory /
            "portable.flag",
        error) &&
        !error;
}

std::filesystem::path ResolveDataRoot(
    const std::filesystem::path&
        executable_directory,
    const std::filesystem::path&
        local_app_data) {
    if (IsPortableMode(
            executable_directory)) {
        return executable_directory /
            "data";
    }

    if (!local_app_data.empty()) {
        return local_app_data /
            "FriendPushScreen";
    }

    return executable_directory /
        "data";
}

std::filesystem::path DefaultDataRoot() {
    return ResolveDataRoot(
        ExecutableDirectory(),
        LocalAppDataPath());
}

}  // namespace fps::platform::windows
