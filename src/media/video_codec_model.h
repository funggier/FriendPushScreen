#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fps::media {

enum class VideoCodecKind {
    H264,
    HEVC,
    AV1
};

enum class VideoCodecDirection {
    Encode,
    Decode
};

enum class VideoBackendClassification {
    Hardware,
    Software,
    Unknown
};

enum class VideoBackendPolicy {
    PreferHardware,
    RequireHardware,
    ForceSoftware
};

struct VideoCodecCandidate {
    VideoCodecKind codec{VideoCodecKind::H264};
    VideoCodecDirection direction{
        VideoCodecDirection::Encode};
    std::string name;
    VideoBackendClassification classification{
        VideoBackendClassification::Unknown};
    bool d3d11_aware{false};
};

struct VideoCodecConfig {
    std::string encoder_id;
    std::string decoder_id;
    VideoCodecKind codec{VideoCodecKind::H264};
    std::uint32_t width{1920};
    std::uint32_t height{1080};
    std::uint32_t fps_numerator{60};
    std::uint32_t fps_denominator{1};
    std::uint32_t target_bitrate_bps{
        20'000'000};
    std::uint32_t keyframe_interval_frames{
        120};
    // Encoder latency option; the synchronous decoder always uses real-time mode.
    bool low_latency{true};
};

const char* VideoCodecKindName(
    VideoCodecKind codec) noexcept;

bool IsValidVideoCodecConfig(
    const VideoCodecConfig& config) noexcept;

std::optional<VideoCodecCandidate>
SelectVideoCodecCandidate(
    const std::vector<VideoCodecCandidate>& candidates,
    VideoCodecKind codec,
    VideoCodecDirection direction,
    VideoBackendPolicy policy);

}  // namespace fps::media
