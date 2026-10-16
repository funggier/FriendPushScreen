#pragma once
#include "audio/wasapi_audio.h"
namespace fps::audio {
enum class AudioControlKind:std::uint8_t {Capabilities=1,SourceSelected,StreamState,Format,Ready};
struct AudioControl {
    AudioControlKind kind{AudioControlKind::Capabilities};
    std::uint64_t session{},generation{};
    AudioSourceKind source{AudioSourceKind::None};
    AudioState state{AudioState::Disabled};
    std::string endpoint_hash;
};
bool IsAudioControl(std::span<const std::uint8_t>);
std::vector<std::uint8_t> EncodeAudioControl(const AudioControl&);
std::optional<AudioControl> DecodeAudioControl(std::span<const std::uint8_t>,std::string&);
struct AudioPeerState {
    std::uint64_t session{},outgoing_generation{},incoming_generation{},selected_generation{};
    bool peer_capable{},outgoing_ready{},incoming_ready{},format_pending{};
    AudioSourceKind outgoing_source{AudioSourceKind::None},incoming_source{AudioSourceKind::None};
    AudioState remote_state{AudioState::Disabled};
    std::string remote_endpoint;
};
class AudioPeerSession {
public:
    void Bind(std::uint64_t session);
    void SelectSource(AudioSourceKind,const std::string& endpoint_hash);
    void SetOutgoingState(AudioState);
    bool OnControl(const AudioControl&);
    void ReceiverReady();
    std::vector<AudioControl> TakeControls(std::size_t limit=16);
    const AudioPeerState& State()const{return state_;}
private:
    AudioPeerState state_;std::uint64_t generation_{};
    std::vector<AudioControl> pending_;
};
}
