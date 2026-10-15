#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fps::media {

struct VideoCaptureMode {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t frame_rate_num{0};
    std::uint32_t frame_rate_den{1};
    std::string pixel_format;

    double FramesPerSecond() const noexcept {
        return frame_rate_den == 0
            ? 0.0
            : static_cast<double>(
                  frame_rate_num) /
                  static_cast<double>(
                      frame_rate_den);
    }
};

std::vector<diagnostics::CaptureDeviceInfo>
ProbeVideoCaptureDevices();

std::vector<VideoCaptureMode>
ProbeVideoCaptureDeviceModes(
    std::string_view symbolic_link);

diagnostics::ProbeResult
MakeMediaEnumerationResult(
    std::string_view name,
    long error_code,
    std::size_t count);

}  // namespace fps::media
