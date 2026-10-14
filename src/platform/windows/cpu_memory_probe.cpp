#include "platform/windows/cpu_memory_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace fps::platform::windows {
namespace {

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        required,
        nullptr,
        nullptr);
    if (written != required) {
        return {};
    }
    return result;
}

std::string ReadCpuModel() {
    DWORD type = 0;
    DWORD bytes = 0;
    constexpr wchar_t kCpuKey[] =
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

    LONG status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        kCpuKey,
        L"ProcessorNameString",
        RRF_RT_REG_SZ,
        &type,
        nullptr,
        &bytes);
    if (status != ERROR_SUCCESS || bytes < sizeof(wchar_t)) {
        return {};
    }

    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t), L'\0');
    status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        kCpuKey,
        L"ProcessorNameString",
        RRF_RT_REG_SZ,
        &type,
        buffer.data(),
        &bytes);
    if (status != ERROR_SUCCESS) {
        return {};
    }

    return NormalizeCpuModel(WideToUtf8(std::wstring{buffer.data()}));
}

std::string ArchitectureName(WORD architecture) {
    switch (architecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return "x64";
    case PROCESSOR_ARCHITECTURE_ARM64:
        return "ARM64";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return "x86";
    default:
        return "Unknown";
    }
}

CpuMemoryRaw QueryCpuAndMemory() {
    CpuMemoryRaw raw{};

    SYSTEM_INFO system_info{};
    GetNativeSystemInfo(&system_info);
    raw.architecture = ArchitectureName(system_info.wProcessorArchitecture);
    raw.logical_processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (raw.logical_processors == 0) {
        raw.logical_processors = system_info.dwNumberOfProcessors;
    }

    raw.cpu_model = ReadCpuModel();

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory) == FALSE) {
        raw.error_code = static_cast<long>(GetLastError());
        raw.message = "GlobalMemoryStatusEx failed";
        return raw;
    }

    raw.memory_query_success = true;
    raw.total_physical_memory_bytes = memory.ullTotalPhys;
    return raw;
}

}  // namespace

std::string NormalizeCpuModel(std::string value) {
    const auto not_space = [](unsigned char ch) {
        return std::isspace(ch) == 0;
    };

    const auto first = std::find_if(value.begin(), value.end(), not_space);
    if (first == value.end()) {
        return {};
    }
    const auto last = std::find_if(value.rbegin(), value.rend(), not_space).base();
    return std::string{first, last};
}

diagnostics::CpuMemoryInfo MakeCpuMemoryInfo(const CpuMemoryRaw& raw) {
    diagnostics::CpuMemoryInfo info{};
    info.cpu_model = NormalizeCpuModel(raw.cpu_model);
    info.architecture = raw.architecture;
    info.logical_processors = raw.logical_processors;
    info.total_physical_memory_bytes = raw.total_physical_memory_bytes;
    info.error_code = raw.error_code;
    info.message = raw.message;

    if (!raw.memory_query_success || raw.total_physical_memory_bytes == 0) {
        info.state = diagnostics::ProbeState::Error;
    } else {
        info.state = diagnostics::ProbeState::Available;
    }

    return info;
}

diagnostics::CpuMemoryInfo ProbeCpuAndMemory() {
    return MakeCpuMemoryInfo(QueryCpuAndMemory());
}

}  // namespace fps::platform::windows
