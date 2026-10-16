#pragma once
#include "capture/capture_frame.h"
#include "media/video_codec_model.h"
#include "media/video_frame_clock.h"
#include "media/encoded_access_unit_queue.h"
#include <memory>
#include <string>
#include <vector>
namespace fps::media {
struct VideoCodecRuntimeInfo {
    VideoCodecCandidate candidate;
    bool asynchronous{false};
    bool surface_io{false};
    bool low_latency_applied{false};
    bool keyframe_interval_applied{false};
    std::uint64_t gpu_frames{0};
    std::uint64_t cpu_frames{0};
};
class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    virtual bool Initialize(ID3D11Device*, const VideoCodecConfig&, VideoBackendPolicy)=0;
    virtual bool Encode(const capture::CapturedFrame&, const NormalizedVideoTiming&,
        std::vector<EncodedAccessUnit>&)=0;
    virtual bool Poll(std::vector<EncodedAccessUnit>&)=0;
    virtual bool Drain(std::vector<EncodedAccessUnit>&)=0;
    virtual bool RequestKeyframe()=0;
    virtual void Stop()=0;
    virtual const VideoCodecRuntimeInfo& Info() const=0;
    virtual std::string Error() const=0;
};
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;
    virtual bool Initialize(ID3D11Device*, const VideoCodecConfig&, VideoBackendPolicy policy)=0;
    virtual bool Decode(const EncodedAccessUnit&, std::vector<capture::CapturedFrame>&)=0;
    virtual bool Poll(std::vector<capture::CapturedFrame>&)=0;
    virtual bool Drain(std::vector<capture::CapturedFrame>&)=0;
    virtual bool Flush()=0;
    virtual void Stop()=0;
    virtual const VideoCodecRuntimeInfo& Info() const=0;
    virtual std::string Error() const=0;
};
class MftVideoEncoder final : public IVideoEncoder {
public:
    MftVideoEncoder(); ~MftVideoEncoder() override;
    bool Initialize(ID3D11Device*, const VideoCodecConfig&, VideoBackendPolicy) override;
    bool Encode(const capture::CapturedFrame&, const NormalizedVideoTiming&, std::vector<EncodedAccessUnit>&) override;
    bool Poll(std::vector<EncodedAccessUnit>&) override;
    bool Drain(std::vector<EncodedAccessUnit>&) override;
    bool RequestKeyframe() override;
    void Stop() override;
    const VideoCodecRuntimeInfo& Info() const override;
    std::string Error() const override;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
class MftVideoDecoder final : public IVideoDecoder {
public:
    MftVideoDecoder(); ~MftVideoDecoder() override;
    bool Initialize(ID3D11Device*, const VideoCodecConfig&, VideoBackendPolicy policy) override;
    // Compatibility for physical probes; policy-based callers can require GPU output.
    bool Initialize(ID3D11Device* device, const VideoCodecConfig& config, bool software) {
        return Initialize(device, config, software ?
            VideoBackendPolicy::ForceSoftware : VideoBackendPolicy::PreferHardware);
    }
    bool Decode(const EncodedAccessUnit&, std::vector<capture::CapturedFrame>&) override;
    bool Poll(std::vector<capture::CapturedFrame>&) override;
    bool Drain(std::vector<capture::CapturedFrame>&) override;
    bool Flush() override;
    void Stop() override;
    const VideoCodecRuntimeInfo& Info() const override;
    std::string Error() const override;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
}
