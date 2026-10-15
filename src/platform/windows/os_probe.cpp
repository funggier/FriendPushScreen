#include "platform/windows/os_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winternl.h>

#include <string>

namespace fps::platform::windows {
namespace {

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

OsVersionQuery QueryOperatingSystem() {
    OsVersionQuery raw{};

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        raw.error_code = static_cast<long>(GetLastError());
        raw.message = "GetModuleHandleW(ntdll.dll) failed";
        return raw;
    }

    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtl_get_version == nullptr) {
        raw.error_code = static_cast<long>(GetLastError());
        raw.message = "RtlGetVersion not available";
        return raw;
    }

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    const LONG status = rtl_get_version(&version);
    if (status != 0) {
        raw.error_code = status;
        raw.message = "RtlGetVersion failed";
        return raw;
    }

    SYSTEM_INFO system_info{};
    GetNativeSystemInfo(&system_info);

    raw.success = true;
    raw.major = version.dwMajorVersion;
    raw.minor = version.dwMinorVersion;
    raw.build = version.dwBuildNumber;
    raw.architecture = ArchitectureName(system_info.wProcessorArchitecture);
    return raw;
}

}  // namespace

diagnostics::OsInfo MakeOsInfo(const OsVersionQuery& raw) {
    diagnostics::OsInfo info{};
    info.major = raw.major;
    info.minor = raw.minor;
    info.build = raw.build;
    info.architecture = raw.architecture;
    info.error_code = raw.error_code;
    info.message = raw.message;

    if (raw.success) {
        info.state = diagnostics::ProbeState::Available;
    } else {
        info.state = diagnostics::ProbeState::Error;
    }
    return info;
}

diagnostics::OsInfo ProbeOperatingSystem() {
    return MakeOsInfo(QueryOperatingSystem());
}

}  // namespace fps::platform::windows
