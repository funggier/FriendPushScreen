#pragma once
#include "media/mf_video_codec.h"
#include "network/peer_protocol.h"
#include <deque>
#include <functional>
#include <map>
namespace fps::media {
struct LatencyPercentiles {double p50_ms{},p95_ms{},p99_ms{};};
struct PeerStreamMetrics {
    std::uint64_t captured{},encoded{},decoded{},delivered{},dropped{},bytes{},max_pending{};
};
class VideoPeerSender {
public:
    using Sink=std::function<bool(network::VideoWireFrame&&)>;
    explicit VideoPeerSender(std::unique_ptr<IVideoEncoder> = std::make_unique<MftVideoEncoder>());
    bool Initialize(ID3D11Device*,const network::VideoFormat&,VideoCodecConfig,VideoBackendPolicy,std::uint64_t session,std::uint64_t generation);
    bool Push(const capture::CapturedFrame&,const Sink&);
    bool Poll(const Sink&);
    bool RequestKeyframe();
    void Stop();
    const VideoCodecRuntimeInfo& Info()const{return encoder_->Info();}
    const PeerStreamMetrics& Metrics()const{return metrics_;}
    LatencyPercentiles Latency()const;
    std::string Error()const{return error_;}
private:
    bool Collect(std::vector<EncodedAccessUnit>&,const Sink&);
    std::unique_ptr<IVideoEncoder> encoder_;
    struct Tracking {NormalizedVideoTiming timing;std::int64_t capture_time{};network::CursorRecord cursor;};
    std::map<std::uint64_t,Tracking> tracking_;
    std::uint64_t session_{},generation_{};
    network::VideoFormat format_;
    VideoFrameClock clock_{60,1};
    std::unique_ptr<class VideoFrameNormalizer> normalizer_;
    PeerStreamMetrics metrics_;
    std::deque<std::int64_t> latency_;
    std::vector<std::uint8_t> parameter_sets_;
    bool running_{},awaiting_keyframe_{};
    std::string error_;
public:
    ~VideoPeerSender();
};
class VideoPeerReceiver {
public:
    using Sink=std::function<bool(const capture::CapturedFrame&)>;
    explicit VideoPeerReceiver(std::unique_ptr<IVideoDecoder> = std::make_unique<MftVideoDecoder>());
    ~VideoPeerReceiver();
    bool Configure(ID3D11Device*,const network::VideoFormat&,VideoCodecConfig,VideoBackendPolicy,std::uint64_t session,std::uint64_t generation);
    bool Push(network::VideoWireFrame,const Sink&);
    bool Poll(const Sink&);
    bool Recover();
    bool TakeKeyframeRequest();
    void Stop();
    bool AwaitingKeyframe()const{return awaiting_keyframe_;}
    const VideoCodecRuntimeInfo& Info()const{return decoder_->Info();}
    const PeerStreamMetrics& Metrics()const{return metrics_;}
    LatencyPercentiles DecodeLatency()const;
    LatencyPercentiles EndToEndLatency()const;
    std::string Error()const{return error_;}
private:
    bool Consume(std::vector<capture::CapturedFrame>&,const Sink&);
    std::unique_ptr<IVideoDecoder> decoder_;
    struct Tracking {
        std::uint64_t source_sequence{};std::int64_t timestamp{},capture_time{},decode_start{};
        network::CursorRecord cursor;std::shared_ptr<void> receive_credit;
    };
    std::map<std::uint64_t,Tracking> tracking_;
    std::unique_ptr<class VideoFrameNormalizer> normalizer_;
    network::VideoFormat format_;
    std::uint64_t session_{},generation_{},last_sequence_{};
    bool running_{},awaiting_keyframe_{true},requested_keyframe_{},needs_keyframe_{};
    PeerStreamMetrics metrics_;
    std::deque<std::int64_t> decode_latency_,end_to_end_;
    std::string error_;
};
}
