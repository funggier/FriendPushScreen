#include "capture/capture_source_catalog.h"

#include "test_support.h"

#include <cstdint>
#include <string>
#include <vector>

using fps::capture::BuildCaptureSourceCatalogFromProbes;
using fps::capture::CaptureSourceKind;
using fps::capture::CaptureSourceSupportsCursor;
using fps::capture::MakeMonitorSourceId;
using fps::capture::MakeVideoDeviceSourceId;
using fps::capture::MakeWindowSourceId;

void RunCaptureSourceCatalogTests() {
    fps::diagnostics::MonitorInfo monitor{};
    monitor.device_name = "\\\\.\\DISPLAY1";
    monitor.left = 0;
    monitor.top = 0;
    monitor.right = 2560;
    monitor.bottom = 1440;
    monitor.primary = true;

    fps::diagnostics::WindowInfo window{};
    window.title = "Firefox — YouTube";
    window.native_handle =
        static_cast<std::uintptr_t>(
            0x1234ABCDu);

    fps::diagnostics::CaptureDeviceInfo device{};
    device.friendly_name = "USB3 Video";
    device.symbolic_link =
        R"(\\?\usb#vid_345f&pid_2131#video)";

    const auto catalog =
        BuildCaptureSourceCatalogFromProbes(
            {monitor},
            {window},
            {device});

    FPS_EXPECT_EQ(
        catalog.size(),
        std::size_t{3});

    FPS_EXPECT_EQ(
        catalog[0].kind,
        CaptureSourceKind::Monitor);
    FPS_EXPECT_EQ(
        catalog[1].kind,
        CaptureSourceKind::Window);
    FPS_EXPECT_EQ(
        catalog[2].kind,
        CaptureSourceKind::VideoDevice);

    FPS_EXPECT_EQ(
        catalog[0].id,
        MakeMonitorSourceId(
            monitor.device_name));
    FPS_EXPECT_EQ(
        catalog[1].id,
        MakeWindowSourceId(
            window.native_handle));
    FPS_EXPECT_EQ(
        catalog[2].id,
        MakeVideoDeviceSourceId(
            device.symbolic_link));

    FPS_EXPECT_EQ(
        catalog[0].width,
        std::uint32_t{2560});
    FPS_EXPECT_EQ(
        catalog[0].height,
        std::uint32_t{1440});
    FPS_EXPECT_TRUE(
        catalog[0].primary);

    FPS_EXPECT_TRUE(
        catalog[0].display_name.find(
            "Screen") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        catalog[1].display_name.find(
            "Window") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        catalog[2].display_name.find(
            "Device") !=
        std::string::npos);

    FPS_EXPECT_TRUE(
        MakeWindowSourceId(
            window.native_handle)
            .find("0x1234ABCD") !=
        std::string::npos);

    FPS_EXPECT_TRUE(
        CaptureSourceSupportsCursor(
            CaptureSourceKind::Monitor));
    FPS_EXPECT_TRUE(
        CaptureSourceSupportsCursor(
            CaptureSourceKind::Window));
    FPS_EXPECT_TRUE(
        !CaptureSourceSupportsCursor(
            CaptureSourceKind::VideoDevice));

    const auto second_catalog =
        BuildCaptureSourceCatalogFromProbes(
            {monitor},
            {window},
            {device});

    FPS_EXPECT_EQ(
        catalog[0].id,
        second_catalog[0].id);
    FPS_EXPECT_EQ(
        catalog[1].id,
        second_catalog[1].id);
    FPS_EXPECT_EQ(
        catalog[2].id,
        second_catalog[2].id);
}
