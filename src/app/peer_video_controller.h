#pragma once
#include "media/video_peer_stream.h"
#include "network/peer_session.h"
#include "audio/peer_audio.h"
namespace fps::app {
struct PeerVideoSnapshot {
    bool active{},connecting{},connected{},wants_share{},sharing{},receiving{},awaiting_keyframe{};
    std::uint16_t listen_port{};
    std::uint64_t connection{},session{},outgoing_generation{},incoming_generation{};
    std::string status,error,own_fingerprint,peer_fingerprint,encoder,decoder,runtime;
    std::optional<std::uint64_t> round_trip_ms;
    network::TransportMetrics transport;
    audio::PeerAudioSnapshot audio;
    media::PeerStreamMetrics outgoing,incoming;
    media::LatencyPercentiles encode_latency,decode_latency,end_to_end;
    std::uint64_t encoder_gpu_frames{},encoder_cpu_frames{},decoder_gpu_frames{},decoder_cpu_frames{};
    std::uint64_t outbound_pending{},outbound_bytes{},max_outbound_pending{},max_outbound_bytes{};
    std::uint64_t flow_recoveries{},skipped_capture{},oldest_outbound_ms{};
};
class PeerVideoController {
public:
    using FrameSink=media::VideoPeerReceiver::Sink;
    using ClearSink=std::function<void()>;
    using EncodedObserver=std::function<void(const network::VideoWireFrame&)>;
    explicit PeerVideoController(std::unique_ptr<media::IVideoEncoder> encoder={},std::unique_ptr<media::IVideoDecoder> decoder={});
    ~PeerVideoController();
    bool Initialize(ID3D11Device*,const std::filesystem::path&,std::string&);
    const network::PublicPeerIdentity* Identity()const;
    const network::PublicPeerIdentity* TrustedPeer()const;
    bool ExportIdentity(const std::filesystem::path&,std::string&);
    // Caller presents and explicitly authorizes the exact full fingerprint first.
    bool AuthorizePeer(const network::PublicPeerIdentity&,std::string&);
    void ConfigureCodecs(media::VideoCodecConfig,media::VideoBackendPolicy encoder,media::VideoBackendPolicy decoder);
    void ConfigureAudio(audio::AudioPreferences);
    bool Listen(std::uint16_t port,std::string&);
    bool Connect(std::string host,std::uint16_t remote_port,std::uint16_t own_port,std::string&);
    bool StartShare(std::string&);
    void StopShare();
    bool PushCapture(const capture::CapturedFrame&);
    bool Poll(const FrameSink&,const ClearSink&);
    void Disconnect();
    PeerVideoSnapshot Snapshot()const;
    HANDLE WakeEvent()const;
    bool WantsShare()const;
    bool IsReceiving()const;
    bool IsActive()const;
    void SetEncodedObserver(EncodedObserver);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
