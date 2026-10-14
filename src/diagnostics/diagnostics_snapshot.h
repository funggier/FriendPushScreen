#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fps::diagnostics {

enum class ProbeState {
    Available,
    Unavailable,
    Unsupported,
    Error
};

struct ProbeResult {
    std::string name;
    ProbeState state{ProbeState::Unavailable};
    long error_code{0};
    std::string message;
};

struct OsInfo {
    ProbeState state{ProbeState::Unavailable};
    std::string edition;
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::uint32_t build{0};
    std::string architecture;
    long error_code{0};
    std::string message;
};

struct CpuMemoryInfo {
    ProbeState state{ProbeState::Unavailable};
    std::string cpu_model;
    std::string architecture;
    std::uint32_t logical_processors{0};
    std::uint64_t total_physical_memory_bytes{0};
    long error_code{0};
    std::string message;
};

struct GraphicsAdapterInfo {
    std::string description;
    std::uint32_t vendor_id{0};
    std::uint32_t device_id{0};
    std::uint64_t dedicated_video_memory_bytes{0};
    std::uint64_t shared_system_memory_bytes{0};
    bool software{false};
};

struct MonitorInfo {
    std::string device_name;
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
    bool primary{false};
};

struct WindowInfo {
    std::string title;
    std::uintptr_t native_handle{0};
};

struct CaptureDeviceInfo {
    std::string friendly_name;
    std::string symbolic_link;
};

enum class CodecKind {
    H264,
    HEVC,
    AV1
};

enum class CodecDirection {
    Encode,
    Decode
};

enum class HardwareClassification {
    Hardware,
    Software,
    Unknown
};

struct CodecCapabilityInfo {
    CodecKind codec{CodecKind::H264};
    CodecDirection direction{CodecDirection::Encode};
    HardwareClassification hardware{HardwareClassification::Unknown};
    std::string name;
};

struct NetworkAdapterInfo {
    std::string name;
    bool operational{false};
    std::string description;
    std::vector<std::string> ipv4_addresses;
    std::uint64_t link_speed_bps{0};
    bool loopback{false};
    bool tunnel{false};
};

enum class TailscalePresence {
    Present,
    Absent,
    Unknown
};

struct NetworkInfo {
    std::vector<NetworkAdapterInfo> adapters;
    TailscalePresence tailscale{TailscalePresence::Unknown};
    ProbeResult result{"network", ProbeState::Unavailable, 0, {}};
};

struct DiagnosticsSnapshot {
    std::string generated_at;
    std::string app_version;
    OsInfo os;
    CpuMemoryInfo cpu_memory;
    std::vector<GraphicsAdapterInfo> graphics_adapters;
    std::vector<MonitorInfo> monitors;
    std::vector<WindowInfo> visible_windows;
    std::vector<CaptureDeviceInfo> capture_devices;
    std::vector<CodecCapabilityInfo> codec_capabilities;
    NetworkInfo network;
    std::vector<ProbeResult> probe_results;
};

}  // namespace fps::diagnostics
