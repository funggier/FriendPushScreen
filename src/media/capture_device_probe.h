#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace fps::media {

std::vector<diagnostics::CaptureDeviceInfo> ProbeVideoCaptureDevices();

diagnostics::ProbeResult MakeMediaEnumerationResult(
    std::string_view name,
    long error_code,
    std::size_t count);

}  // namespace fps::media
