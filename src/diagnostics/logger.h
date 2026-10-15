#pragma once

#include <filesystem>
#include <mutex>
#include <string_view>

namespace fps::diagnostics {

enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& Instance();

    explicit Logger(std::filesystem::path path);

    void Log(
        LogLevel level,
        std::string_view subsystem,
        std::string_view message);

private:
    std::filesystem::path path_;
    std::mutex mutex_;
};

}  // namespace fps::diagnostics
