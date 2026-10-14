#include "platform/windows/monitor_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <vector>

namespace fps::platform::windows {
namespace {

std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }

    const int length = static_cast<int>(wcslen(value));
    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        length,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            length,
            output.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }

    return output;
}

BOOL CALLBACK MonitorCallback(
    HMONITOR monitor,
    HDC,
    LPRECT,
    LPARAM user_data) {
    auto* output =
        reinterpret_cast<std::vector<diagnostics::MonitorInfo>*>(user_data);

    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info) == FALSE) {
        return TRUE;
    }

    diagnostics::MonitorInfo monitor_info{};
    monitor_info.device_name = WideToUtf8(info.szDevice);
    monitor_info.left = info.rcMonitor.left;
    monitor_info.top = info.rcMonitor.top;
    monitor_info.right = info.rcMonitor.right;
    monitor_info.bottom = info.rcMonitor.bottom;
    monitor_info.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    output->push_back(std::move(monitor_info));
    return TRUE;
}

}  // namespace

std::vector<diagnostics::MonitorInfo> ProbeMonitors() {
    std::vector<diagnostics::MonitorInfo> monitors;
    EnumDisplayMonitors(
        nullptr,
        nullptr,
        MonitorCallback,
        reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

}  // namespace fps::platform::windows
