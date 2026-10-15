#pragma once

#include <filesystem>

namespace fps::platform::windows {

std::filesystem::path ExecutableDirectory();

bool IsPortableMode(
    const std::filesystem::path& executable_directory);

std::filesystem::path ResolveDataRoot(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& local_app_data);

std::filesystem::path DefaultDataRoot();

}  // namespace fps::platform::windows
