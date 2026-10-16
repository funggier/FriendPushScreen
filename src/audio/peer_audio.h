#pragma once
#include "audio/audio_session.h"
#include <functional>
namespace fps::audio {
struct AudioPreferences {
    std::string source_id{"none"},output_id;
    bool muted{};int volume{100};std::uint32_t windows_build{19045};
    bool operator==(const AudioPreferences&)const=default;
};
struct PeerAudioSnapshot {
    AudioState send_state{AudioState::Disabled};
    bool receiving{},feedback_blocked{},muted{};
    std::uint64_t outgoing_generation{},incoming_generation{},captured_frames{},encoded{},sent{},received{},decoded{},played_frames{},plc{},dropped{},underflows{},overruns{},source_restarts{},max_render_queue_frames{};
    std::size_t jitter_depth{},max_jitter_depth{};
    AudioPcmFormat input_format;
    double capture_latency_mean_ms{},capture_latency_max_ms{},playout_age_ms{};
    std::string source,output,status,error;
    AudioCatalog catalog;
};
class IPeerAudio {
public:
    using ControlSender=std::function<bool(std::vector<std::uint8_t>)>;
    using AudioSender=std::function<bool(std::vector<std::uint8_t>)>;
    virtual ~IPeerAudio()=default;
    virtual void Configure(AudioPreferences,bool)=0;
    virtual void Bind(std::uint64_t,std::uint64_t)=0;
    virtual void OnControl(std::uint64_t,std::span<const std::uint8_t>)=0;
    virtual void OnAudio(std::uint64_t,std::span<const std::uint8_t>)=0;
    virtual void Tick(const ControlSender&,const AudioSender&)=0;
    virtual PeerAudioSnapshot Snapshot()const=0;
};
class PeerAudio final:public IPeerAudio {
public:
    using ControlSender=std::function<bool(std::vector<std::uint8_t>)>;
    using AudioSender=std::function<bool(std::vector<std::uint8_t>)>;
    PeerAudio();~PeerAudio();
    void Configure(AudioPreferences,bool share);
    void Bind(std::uint64_t connection,std::uint64_t session);
    void OnControl(std::uint64_t connection,std::span<const std::uint8_t>);
    void OnAudio(std::uint64_t connection,std::span<const std::uint8_t>);
    void Tick(const ControlSender&,const AudioSender&);
    PeerAudioSnapshot Snapshot()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
