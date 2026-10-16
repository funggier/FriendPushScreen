#pragma once
#include "media/mf_video_codec.h"
#include "media/video_frame_normalizer.h"
#include "media/video_loop_metrics.h"
#include <functional>
#include <map>
#include <deque>
namespace fps::media {
enum class VideoLoopState { Stopped, Starting, Running, Recovering, Error };
struct VideoLoopTailLatency { double encode_p95_ms{0},decode_p95_ms{0},end_to_end_p95_ms{0}; };
class VideoLocalLoop {
public:
    using FrameSink=std::function<bool(const capture::CapturedFrame&)>;
    VideoLocalLoop(std::unique_ptr<IVideoEncoder> encoder=std::make_unique<MftVideoEncoder>(),
        std::unique_ptr<IVideoDecoder> decoder=std::make_unique<MftVideoDecoder>());
    bool Initialize(ID3D11Device*,VideoCodecConfig,VideoBackendPolicy);
    bool Initialize(ID3D11Device*,VideoCodecConfig,VideoBackendPolicy encoder_policy,VideoBackendPolicy decoder_policy);
    bool Push(const capture::CapturedFrame&,const FrameSink&);
    bool Poll(const FrameSink&);
    bool Drain(const FrameSink&);
    void Stop();
    VideoLoopState State() const { return state_; }
    std::string Status() const;
    std::string Error() const { return error_; }
    const VideoLoopMetricsSnapshot& Metrics() const { return metrics_.Snapshot(); }
    VideoLoopTailLatency TailLatency() const;
    const VideoCodecRuntimeInfo& EncoderInfo() const { return encoder_->Info(); }
    const VideoCodecRuntimeInfo& DecoderInfo() const { return decoder_->Info(); }
    std::uint32_t Width() const { return config_.width; }
    std::uint32_t Height() const { return config_.height; }
    std::uint64_t CpuInputFrames() const { return cpu_input_frames_; }
    std::uint64_t PaddedFrames() const { return padded_frames_; }
    std::uint64_t EncodedBytes() const { return encoded_bytes_; }
private:
    bool Enqueue(std::vector<EncodedAccessUnit>&,const FrameSink&);
    bool DecodeQueued(const FrameSink&);
    bool Consume(std::vector<capture::CapturedFrame>&,const FrameSink&);
    bool Normalize(const capture::CapturedFrame&,capture::CapturedFrame&);
    bool Fail(std::string);
    std::unique_ptr<IVideoEncoder> encoder_;
    std::unique_ptr<IVideoDecoder> decoder_;
    VideoCodecConfig config_;
    VideoFrameClock clock_{60,1};
    BoundedEncodedQueue queue_{2};
    VideoLoopMetrics metrics_;
    struct Tracking {
        NormalizedVideoTiming timing;
        std::int64_t arrival{0},encoded{0},decode_start{0};
        std::uint32_t width{0},height{0};
        capture::CapturedCursor cursor;
    };
    std::map<std::uint64_t,Tracking> tracking_;
    VideoFrameNormalizer normalizer_;
    VideoLoopState state_{VideoLoopState::Stopped};
    std::string error_;
    std::uint64_t cpu_input_frames_{0},padded_frames_{0},encoded_bytes_{0};
    std::deque<std::int64_t> encode_tail_,decode_tail_,e2e_tail_;
};
}
