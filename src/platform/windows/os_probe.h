#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <cstdint>
#include <string>

namespace fps::platform::windows {

struct OsVersionQuery {
    bool success{false};
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::uint32_t build{0};
    std::string architecture;
    long error_code{0};
    std::string message;
};

diagnostics::OsInfo MakeOsInfo(const OsVersionQuery& raw);
diagnostics::OsInfo ProbeOperatingSystem();

}  // namespace fps::platform::windows
