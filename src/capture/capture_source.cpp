#include "capture/capture_source.h"

#include <iomanip>
#include <sstream>

namespace fps::capture {
namespace {

std::string HexHandle(
    std::uintptr_t value) {
    std::ostringstream stream;
    stream
        << std::hex
        << std::uppercase
        << value;
    return stream.str();
}

}  // namespace

std::string CaptureSourceKindName(
    CaptureSourceKind kind) {
    switch (kind) {
    case CaptureSourceKind::Monitor:
        return "Screen";
    case CaptureSourceKind::Window:
        return "Window";
    case CaptureSourceKind::VideoDevice:
        return "Device";
    }

    return "Source";
}

bool CaptureSourceSupportsCursor(
    CaptureSourceKind kind) {
    return kind ==
               CaptureSourceKind::Monitor ||
        kind ==
               CaptureSourceKind::Window;
}

std::string MakeMonitorSourceId(
    const std::string& device_name) {
    return "monitor:" + device_name;
}

std::string MakeWindowSourceId(
    std::uintptr_t native_handle) {
    return "window:0x" +
        HexHandle(native_handle);
}

std::string MakeVideoDeviceSourceId(
    const std::string& symbolic_link) {
    return "device:" + symbolic_link;
}

}  // namespace fps::capture
