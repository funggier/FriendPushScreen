#include "capture/capture_source_catalog.h"

#include "media/capture_device_probe.h"
#include "platform/windows/monitor_probe.h"
#include "platform/windows/window_probe.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fps::capture {
namespace {

std::string MonitorLabel(
    const diagnostics::MonitorInfo& monitor) {
    const int width =
        std::max(
            0,
            monitor.right -
                monitor.left);
    const int height =
        std::max(
            0,
            monitor.bottom -
                monitor.top);

    std::string label =
        "Screen — " +
        monitor.device_name;

    if (width > 0 &&
        height > 0) {
        label +=
            " (" +
            std::to_string(width) +
            "x" +
            std::to_string(height) +
            ")";
    }

    if (monitor.primary) {
        label += " [Primary]";
    }

    return label;
}

std::string WindowLabel(
    const diagnostics::WindowInfo& window) {
    return "Window — " +
        window.title;
}

std::string DeviceLabel(
    const diagnostics::CaptureDeviceInfo& device) {
    return "Device — " +
        (device.friendly_name.empty()
             ? std::string{"Video Capture Device"}
             : device.friendly_name);
}

}  // namespace

std::vector<CaptureSourceDescriptor>
BuildCaptureSourceCatalogFromProbes(
    const std::vector<diagnostics::MonitorInfo>& monitors,
    const std::vector<diagnostics::WindowInfo>& windows,
    const std::vector<diagnostics::CaptureDeviceInfo>& devices) {
    std::vector<CaptureSourceDescriptor> output;
    output.reserve(
        monitors.size() +
        windows.size() +
        devices.size());

    for (const auto& monitor : monitors) {
        CaptureSourceDescriptor source{};
        source.kind =
            CaptureSourceKind::Monitor;
        source.id =
            MakeMonitorSourceId(
                monitor.device_name);
        source.display_name =
            MonitorLabel(monitor);
        source.native_id =
            monitor.device_name;
        source.width =
            static_cast<std::uint32_t>(
                std::max(
                    0,
                    monitor.right -
                        monitor.left));
        source.height =
            static_cast<std::uint32_t>(
                std::max(
                    0,
                    monitor.bottom -
                        monitor.top));
        source.primary = monitor.primary;
        output.push_back(
            std::move(source));
    }

    for (const auto& window : windows) {
        CaptureSourceDescriptor source{};
        source.kind =
            CaptureSourceKind::Window;
        source.id =
            MakeWindowSourceId(
                window.native_handle);
        source.display_name =
            WindowLabel(window);
        source.native_handle =
            window.native_handle;
        output.push_back(
            std::move(source));
    }

    for (const auto& device : devices) {
        CaptureSourceDescriptor source{};
        source.kind =
            CaptureSourceKind::VideoDevice;
        source.id =
            MakeVideoDeviceSourceId(
                device.symbolic_link);
        source.display_name =
            DeviceLabel(device);
        source.native_id =
            device.symbolic_link;
        output.push_back(
            std::move(source));
    }

    return output;
}

std::vector<CaptureSourceDescriptor>
BuildCaptureSourceCatalog(
    HWND self_root) {
    const auto monitors =
        platform::windows::ProbeMonitors();
    const auto windows =
        platform::windows::
            ProbeVisibleTopLevelWindows(
                self_root);
    const auto devices =
        media::ProbeVideoCaptureDevices();

    return
        BuildCaptureSourceCatalogFromProbes(
            monitors,
            windows,
            devices);
}

}  // namespace fps::capture
