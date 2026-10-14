#include "diagnostics/diagnostics_formatter.h"

#include <sstream>

namespace fps::diagnostics {
namespace {

const char* CodecName(CodecKind codec) {
    switch (codec) {
    case CodecKind::H264:
        return "H.264";
    case CodecKind::HEVC:
        return "HEVC";
    case CodecKind::AV1:
        return "AV1";
    }
    return "Unknown";
}

const char* DirectionName(CodecDirection direction) {
    return direction == CodecDirection::Encode ? "Encode" : "Decode";
}

const char* HardwareName(HardwareClassification classification) {
    switch (classification) {
    case HardwareClassification::Hardware:
        return "Hardware";
    case HardwareClassification::Software:
        return "Software";
    case HardwareClassification::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

}  // namespace

std::string ProbeStateToString(ProbeState state) {
    switch (state) {
    case ProbeState::Available:
        return "Available";
    case ProbeState::Unavailable:
        return "Unavailable";
    case ProbeState::Unsupported:
        return "Unsupported";
    case ProbeState::Error:
        return "Error";
    }
    return "Error";
}

std::string FormatDiagnosticsText(
    const DiagnosticsSnapshot& snapshot,
    bool include_window_titles) {
    std::ostringstream out;

    out << "FriendPushScreen Diagnostics\n\n";

    out << "Application\n";
    out << "Version: " << snapshot.app_version << "\n";
    if (!snapshot.generated_at.empty()) {
        out << "Generated: " << snapshot.generated_at << "\n";
    }

    out << "\nOperating System\n";
    out << "State: " << ProbeStateToString(snapshot.os.state) << "\n";
    out << "Build: " << snapshot.os.build << "\n";
    if (!snapshot.os.architecture.empty()) {
        out << "Architecture: " << snapshot.os.architecture << "\n";
    }

    out << "\nCPU / Memory\n";
    out << "State: " << ProbeStateToString(snapshot.cpu_memory.state) << "\n";
    out << "CPU: " << snapshot.cpu_memory.cpu_model << "\n";
    out << "Logical processors: " << snapshot.cpu_memory.logical_processors << "\n";
    out << "Physical memory bytes: " << snapshot.cpu_memory.total_physical_memory_bytes << "\n";

    out << "\nGraphics Adapters\n";
    out << "Count: " << snapshot.graphics_adapters.size() << "\n";
    for (const auto& adapter : snapshot.graphics_adapters) {
        out << "- " << adapter.description << "\n";
    }

    out << "\nMonitors\n";
    out << "Count: " << snapshot.monitors.size() << "\n";

    out << "\nVisible Windows\n";
    out << "Count: " << snapshot.visible_windows.size() << "\n";
    if (include_window_titles) {
        for (const auto& window : snapshot.visible_windows) {
            out << "- " << window.title << "\n";
        }
    } else {
        out << "Titles: omitted from default export\n";
    }

    out << "\nCapture Devices\n";
    out << "Count: " << snapshot.capture_devices.size() << "\n";
    for (const auto& device : snapshot.capture_devices) {
        out << "- " << device.friendly_name << "\n";
    }

    out << "\nCodec Capabilities\n";
    out << "Count: " << snapshot.codec_capabilities.size() << "\n";
    for (const auto& codec : snapshot.codec_capabilities) {
        out << "- " << CodecName(codec.codec) << ' '
            << DirectionName(codec.direction) << " / "
            << HardwareName(codec.hardware);
        if (!codec.name.empty()) {
            out << " / " << codec.name;
        }
        out << "\n";
    }

    out << "\nNetwork\n";
    out << "Adapters: " << snapshot.network.adapters.size() << "\n";
    for (const auto& adapter : snapshot.network.adapters) {
        out << "- " << adapter.name
            << " / " << (adapter.operational ? "Up" : "Down") << "\n";
    }

    out << "\nProbe Warnings / Errors\n";
    if (snapshot.probe_results.empty()) {
        out << "None\n";
    }
    for (const auto& result : snapshot.probe_results) {
        out << "- " << result.name << ": " << ProbeStateToString(result.state);
        if (result.error_code != 0) {
            out << " (" << result.error_code << ')';
        }
        if (!result.message.empty()) {
            out << " - " << result.message;
        }
        out << "\n";
    }

    return out.str();
}

}  // namespace fps::diagnostics
