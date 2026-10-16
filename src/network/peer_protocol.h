#pragma once
#include "network/secure_peer_transport.h"
#include <optional>
namespace fps::network {
enum class ControlType:std::uint8_t {Hello=1,ShareStart,VideoFormat,ShareStop,KeyframeRequest,Ping,Pong,Goodbye,Error,VideoReady,VideoConsumed};
inline constexpr std::uint32_t kCapabilityH264Encode=1,kCapabilityH264Decode=2;
struct VideoFormat {
    std::uint32_t coded_width{},coded_height{},visible_width{},visible_height{};
    std::uint32_t fps_numerator{60},fps_denominator{1},bitrate{20'000'000},gop{120};
    bool low_latency{true};
    bool operator==(const VideoFormat&)const=default;
};
struct ControlMessage {
    ControlType type{ControlType::Hello};
    std::uint64_t session{},generation{},value{};
    std::uint32_t stream{1},capabilities{kCapabilityH264Encode|kCapabilityH264Decode};
    std::uint16_t listen_port{};
    VideoFormat format;
    std::string text;
};
struct CursorRecord {
    bool visible{};
    std::int32_t x{},y{};
    std::uint32_t width{},height{},row_pitch{};
    std::vector<std::uint8_t> bgra;
    bool operator==(const CursorRecord&)const=default;
};
struct VideoWireFrame {
    std::uint64_t session{},generation{},sequence{},source_sequence{};
    std::uint32_t stream{1};
    std::int64_t timestamp_100ns{},duration_100ns{},capture_time_100ns{};
    VideoFormat format;
    bool keyframe{};
    CursorRecord cursor;
    std::shared_ptr<void> receive_credit; // Local lifetime only; never serialized.
    std::vector<std::uint8_t> bytes;
};
bool ValidateVideoFormat(const VideoFormat&);
std::vector<std::uint8_t> EncodeControl(const ControlMessage&);
std::optional<ControlMessage> DecodeControl(std::span<const std::uint8_t>,std::string&);
std::vector<std::uint8_t> EncodeVideo(const VideoWireFrame&);
std::optional<VideoWireFrame> DecodeVideo(std::span<const std::uint8_t>,std::string&);
bool IsValidVideoWireFrame(const VideoWireFrame&);
bool HasRandomAccessParameterSets(std::span<const std::uint8_t>);
}
