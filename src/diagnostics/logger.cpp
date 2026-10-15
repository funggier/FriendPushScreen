#include "diagnostics/logger.h"

#include "platform/windows/runtime_paths.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace fps::diagnostics {
namespace {

std::filesystem::path DefaultLogPath() {
    return platform::windows::
        DefaultDataRoot() /
        "logs" /
        "FriendPushScreen.log";
}

const char* LevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    }
    return "Error";
}

std::string TimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
    localtime_s(&local_time, &value);

    std::ostringstream out;
    out << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}  // namespace

Logger& Logger::Instance() {
    static Logger instance{DefaultLogPath()};
    return instance;
}

Logger::Logger(std::filesystem::path path)
    : path_(std::move(path)) {}

void Logger::Log(
    LogLevel level,
    std::string_view subsystem,
    std::string_view message) {
    std::lock_guard lock{mutex_};

    std::error_code error;
    const auto parent = path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
    }

    std::ofstream output{path_, std::ios::binary | std::ios::app};
    if (!output) {
        return;
    }

    output << TimestampNow()
           << " [" << LevelName(level) << "]"
           << " [" << subsystem << "] "
           << message << '\n';
}

}  // namespace fps::diagnostics
