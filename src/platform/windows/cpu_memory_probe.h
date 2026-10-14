#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <cstdint>
#include <string>

namespace fps::platform::windows {

struct CpuMemoryRaw {
    std::string cpu_model;
    std::string architecture;
    std::uint32_t logical_processors{0};
    bool memory_query_success{false};
    std::uint64_t total_physical_memory_bytes{0};
    long error_code{0};
    std::string message;
};

std::string NormalizeCpuModel(std::string value);
diagnostics::CpuMemoryInfo MakeCpuMemoryInfo(const CpuMemoryRaw& raw);
diagnostics::CpuMemoryInfo ProbeCpuAndMemory();

}  // namespace fps::platform::windows
