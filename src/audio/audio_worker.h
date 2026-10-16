#pragma once
#include "audio/peer_audio.h"
#include "network/secure_peer_transport.h"
namespace fps::audio {
struct AudioOutbound {std::uint64_t connection{},session_revision{};bool control{};std::vector<std::uint8_t> bytes;};
class PeerAudioWorker {
public:
    explicit PeerAudioWorker(std::unique_ptr<IPeerAudio> engine={});
    ~PeerAudioWorker();
    void Configure(std::uint64_t connection,std::uint64_t session,AudioPreferences,bool share);
    void OnControl(std::uint64_t,std::vector<std::uint8_t>);
    void OnAudio(network::TransportEvent);
    std::vector<AudioOutbound> TakeOutgoing();
    void Dispatch(const std::function<bool(const AudioOutbound&)>&);
    void SendDropped();
    PeerAudioSnapshot Snapshot()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
