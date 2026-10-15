#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "capture/capture_source.h"
#include "diagnostics/diagnostics_snapshot.h"

#include <vector>

namespace fps::capture {

std::vector<CaptureSourceDescriptor>
BuildCaptureSourceCatalogFromProbes(
    const std::vector<diagnostics::MonitorInfo>& monitors,
    const std::vector<diagnostics::WindowInfo>& windows,
    const std::vector<diagnostics::CaptureDeviceInfo>& devices);

std::vector<CaptureSourceDescriptor>
BuildCaptureSourceCatalog(
    HWND self_root);

}  // namespace fps::capture
