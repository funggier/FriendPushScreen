#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace fps::audio {
inline constexpr std::uint32_t kAudioRate=48000,kAudioChannels=2,kAudioFrames=960;
inline constexpr std::size_t kAudioPacketLimit=900,kAudioQueueLimit=6;
struct AudioPacket {
    std::uint64_t session{},generation{},sequence{};
    std::int64_t pts_100ns{};
    std::uint32_t stream{2},duration_frames{kAudioFrames};
    std::vector<std::uint8_t> bytes;
};
std::vector<std::uint8_t> EncodeAudio(const AudioPacket&);
std::optional<AudioPacket> DecodeAudio(std::span<const std::uint8_t>,std::string&);
struct PlayoutFrame {std::optional<AudioPacket> packet;};
class AudioJitter {
public:
    void Reset(std::uint64_t session,std::uint64_t generation);
    bool Push(AudioPacket,std::uint64_t now_ms);
    std::optional<PlayoutFrame> Pop(std::uint64_t now_ms);
    std::size_t Depth()const;
    std::uint64_t Dropped()const;
private:
    std::uint64_t session_{},generation_{},next_{},played_{},due_{},last_arrival_{},dropped_{};
    unsigned misses_{};
    std::map<std::uint64_t,AudioPacket> packets_;
};
bool FeedbackRisk(bool same_host,const std::string& remote_render,const std::string& output,const std::string& own_render);
void ApplyGain(std::span<float>,bool muted,int volume);
}
