#pragma once
#include "audio/audio_model.h"
namespace fps::audio {
enum class AudioSourceKind {None,SystemLoopback,RenderLoopback,CaptureEndpoint,ProcessLoopback};
enum class AudioState {Disabled,Starting,Active,SourceLost,Error};
struct AudioSourceDescriptor {
    std::string id,name,reason;
    AudioSourceKind kind{AudioSourceKind::None};
    bool supported{true};
};
struct AudioEndpoint {std::string id,name;bool is_default{};};
struct AudioCatalog {std::vector<AudioSourceDescriptor> sources;std::vector<AudioEndpoint> outputs;std::string process_reason,error;};
struct AudioPcmFormat {std::uint32_t sample_rate{kAudioRate},channels{kAudioChannels},native_rate{},native_channels{};};
struct AudioPcmChunk {std::vector<float> samples;std::int64_t pts_100ns{};bool discontinuity{};};
std::int64_t AudioNow100ns();
AudioCatalog EnumerateAudioDevices(std::uint32_t windows_build);
std::string ResolveOutputEndpoint(const std::string& id,std::string& error);
class IAudioSource {
public:
    virtual ~IAudioSource()=default;
    virtual bool Start(const std::string& source_id,std::string& error)=0;
    virtual void Stop()=0;
    virtual bool Poll(std::vector<AudioPcmChunk>&,std::string& error)=0;
    virtual AudioState State()const=0;
    virtual AudioSourceDescriptor Descriptor()const=0;
    virtual AudioPcmFormat Format()const=0;
    virtual std::string LoopbackEndpoint()const=0;
};
class WasapiAudioSource final:public IAudioSource {
public:
    WasapiAudioSource();~WasapiAudioSource()override;
    bool Start(const std::string&,std::string&)override;
    void Stop()override;
    bool Poll(std::vector<AudioPcmChunk>&,std::string&)override;
    AudioState State()const override;
    AudioSourceDescriptor Descriptor()const override;
    AudioPcmFormat Format()const override;
    std::string LoopbackEndpoint()const override;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
struct AudioRenderMetrics {std::uint64_t frames{},underflows{},overruns{},max_queued_frames{};};
class WasapiAudioRenderer {
public:
    WasapiAudioRenderer();~WasapiAudioRenderer();
    bool Start(const std::string& output_id,std::string&);
    void Stop();
    void SetGain(bool muted,int volume);
    bool Queue(std::span<const float>,std::string&);
    bool Pump(std::string&);
    bool Active()const;
    std::string Endpoint()const;
    AudioRenderMetrics Metrics()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
