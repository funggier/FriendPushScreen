#pragma once

#include <cstdint>
#include <string>

namespace fps::capture {

enum class CaptureSourceKind {
    Monitor,
    Window,
    VideoDevice
};

enum class CaptureAvailability {
    Available,
    Unavailable
};

struct CaptureSourceDescriptor {
    CaptureSourceKind kind{CaptureSourceKind::Monitor};
    std::string id;
    std::string display_name;
    std::string native_id;
    std::uintptr_t native_handle{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool primary{false};
    CaptureAvailability availability{
        CaptureAvailability::Available};
};

std::string CaptureSourceKindName(
    CaptureSourceKind kind);

std::string MakeMonitorSourceId(
    const std::string& device_name);

std::string MakeWindowSourceId(
    std::uintptr_t native_handle);

std::string MakeVideoDeviceSourceId(
    const std::string& symbolic_link);

}  // namespace fps::capture
