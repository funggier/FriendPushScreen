#include "media/video_local_loop.h"
#include <algorithm>
#include <chrono>
#include <cstring>
namespace fps::media {
namespace {
std::int64_t Now() { return std::chrono::duration_cast<std::chrono::duration<std::int64_t,std::ratio<1,10000000>>>(
    std::chrono::steady_clock::now().time_since_epoch()).count(); }
void Record(std::deque<std::int64_t>& values,std::int64_t value) {
    if(values.size()==2048) values.pop_front();
    values.push_back(std::max<std::int64_t>(0,value));
}
double P95(const std::deque<std::int64_t>& values) {
    if(values.empty()) return 0;
    std::vector<std::int64_t> sorted(values.begin(),values.end());
    std::sort(sorted.begin(),sorted.end());
    return static_cast<double>(sorted[(sorted.size()-1)*95/100])/10000.0;
}
}
VideoLocalLoop::VideoLocalLoop(std::unique_ptr<IVideoEncoder> encoder,std::unique_ptr<IVideoDecoder> decoder):
    encoder_(std::move(encoder)),decoder_(std::move(decoder)) {}
bool VideoLocalLoop::Fail(std::string error) { error_=std::move(error);state_=VideoLoopState::Error;return false; }
bool VideoLocalLoop::Initialize(ID3D11Device* device,VideoCodecConfig config,VideoBackendPolicy policy) {
    return Initialize(device, std::move(config), policy,
        policy == VideoBackendPolicy::ForceSoftware ?
            VideoBackendPolicy::ForceSoftware : VideoBackendPolicy::PreferHardware);
}
bool VideoLocalLoop::Initialize(ID3D11Device* device,VideoCodecConfig config,
    VideoBackendPolicy encoder_policy,VideoBackendPolicy decoder_policy) {
    Stop();
    config_=config;clock_=VideoFrameClock(config.fps_numerator,config.fps_denominator);
    queue_=BoundedEncodedQueue(2);tracking_.clear();metrics_.Reset();
    encode_tail_.clear();decode_tail_.clear();e2e_tail_.clear();error_.clear();
    normalizer_.Configure(device,config.width,config.height);
    cpu_input_frames_=0;padded_frames_=0;encoded_bytes_=0;state_=VideoLoopState::Starting;
    if(!encoder_->Initialize(device,config,encoder_policy))return Fail("Encoder: "+encoder_->Error());
    if(!decoder_->Initialize(device,config,decoder_policy))return Fail("Decoder: "+decoder_->Error());
    return true;
}
void VideoLocalLoop::Stop() {
    encoder_->Stop();decoder_->Stop();
    tracking_.clear();queue_.Clear();normalizer_.Stop();state_=VideoLoopState::Stopped;
}
bool VideoLocalLoop::Normalize(const capture::CapturedFrame& source,capture::CapturedFrame& frame) {
    std::string error;
    if(!normalizer_.Normalize(source,frame,error))return Fail(error);
    if(source.width!=config_.width||source.height!=config_.height)++padded_frames_;
    return true;
}
bool VideoLocalLoop::Push(const capture::CapturedFrame& source,const FrameSink& sink) {
    if(state_==VideoLoopState::Stopped || state_==VideoLoopState::Error)return false;
    metrics_.OnCaptureFrame();
    if(tracking_.size()>=64)return Fail("Codec metadata bound exceeded");
    capture::CapturedFrame padded;
    const capture::CapturedFrame* input=&source;
    if(source.HasCpuPixels())++cpu_input_frames_;
    if(!source.IsValid())return Fail("Invalid capture frame");
    if(source.width!=config_.width || source.height!=config_.height) {
        if(!Normalize(source,padded))return false;
        input=&padded;
    }
    const auto timing=clock_.Normalize(source.sequence,source.timestamp_100ns);
    tracking_.emplace(timing.codec_sequence,Tracking{timing,Now(),0,0,source.width,source.height,source.cursor});
    std::vector<EncodedAccessUnit> units;
    if(!encoder_->Encode(*input,timing,units))return Fail("Encoder: "+encoder_->Error());
    metrics_.OnEncoderSubmission();
    return Enqueue(units,sink);
}
bool VideoLocalLoop::Enqueue(std::vector<EncodedAccessUnit>& units,const FrameSink& sink) {
    for(auto& unit:units) {
        auto found=tracking_.find(unit.sequence);
        if(found==tracking_.end())return Fail("Encoded frame has no capture metadata");
        const auto now=Now();found->second.encoded=now;
        auto latency=now-found->second.arrival;metrics_.OnEncoded(latency);Record(encode_tail_,latency);
        encoded_bytes_+=unit.bytes.size();
        const auto before=queue_.Stats();
        const auto result=queue_.Push(std::move(unit));
        const auto after=queue_.Stats();
        metrics_.OnDropped(after.dropped-before.dropped);
        metrics_.SetQueueDepth(queue_.Size());
        if(after.recovery_resets!=before.recovery_resets) {
            metrics_.OnRecoveryReset();state_=VideoLoopState::Recovering;
            if(!decoder_->Flush())return Fail("Recovery decoder: "+decoder_->Error());
            for(auto it=tracking_.begin();it!=tracking_.end();) {
                if(it->second.decode_start!=0) {metrics_.OnDropped();it=tracking_.erase(it);} else ++it;
            }
            if(!encoder_->RequestKeyframe())return Fail("Recovery IDR: "+encoder_->Error());
        }
        if(result==EncodedQueuePushResult::DroppedAwaitingKeyframe ||
            result==EncodedQueuePushResult::OverflowResetAwaitingKeyframe)tracking_.erase(found);
        if(result==EncodedQueuePushResult::RecoveryKeyframeQueued ||
            result==EncodedQueuePushResult::OverflowResetKeyframeQueued) {
            // Older metadata belongs to frames discarded by queue reset/decoder flush.
            auto first=tracking_.lower_bound(found->first);tracking_.erase(tracking_.begin(),first);
        }
    }
    return DecodeQueued(sink);
}
bool VideoLocalLoop::DecodeQueued(const FrameSink& sink) {
    while(auto unit=queue_.Pop()) {
        metrics_.SetQueueDepth(queue_.Size());
        auto found=tracking_.find(unit->sequence);
        if(found==tracking_.end())return Fail("Queue frame metadata missing");
        found->second.decode_start=Now();
        std::vector<capture::CapturedFrame> frames;
        if(!decoder_->Decode(*unit,frames))return Fail("Decoder: "+decoder_->Error());
        if(!Consume(frames,sink))return false;
    }
    return true;
}
bool VideoLocalLoop::Consume(std::vector<capture::CapturedFrame>& frames,const FrameSink& sink) {
    for(auto& frame:frames) {
        auto found=tracking_.find(frame.sequence);
        if(found==tracking_.end() || frame.timestamp_100ns!=found->second.timing.timestamp_100ns)
            return Fail("Decoded frame metadata mismatch");
        auto& t=found->second;auto latency=Now()-t.decode_start;
        metrics_.OnDecoded(latency);Record(decode_tail_,latency);
        frame.cursor=std::move(t.cursor);
        std::string crop_error;
        if(!normalizer_.RestoreVisible(frame,t.width,t.height,crop_error))return Fail(crop_error);
        if(!sink(frame))return Fail("Decoded preview renderer failed");
        const auto e2e=Now()-t.arrival;
        metrics_.OnRendered(e2e);Record(e2e_tail_,e2e);tracking_.erase(found);
        if(state_!=VideoLoopState::Recovering || !queue_.AwaitingKeyframe())state_=VideoLoopState::Running;
    }
    return true;
}
bool VideoLocalLoop::Poll(const FrameSink& sink) {
    if(state_==VideoLoopState::Stopped || state_==VideoLoopState::Error)return false;
    std::vector<EncodedAccessUnit> units;
    if(!encoder_->Poll(units))return Fail("Encoder poll: "+encoder_->Error());
    if(!Enqueue(units,sink))return false;
    std::vector<capture::CapturedFrame> frames;
    if(!decoder_->Poll(frames))return Fail("Decoder poll: "+decoder_->Error());
    return Consume(frames,sink);
}
bool VideoLocalLoop::Drain(const FrameSink& sink) {
    std::vector<EncodedAccessUnit> units;
    if(!encoder_->Drain(units))return Fail("Encoder drain: "+encoder_->Error());
    if(!Enqueue(units,sink))return false;
    std::vector<capture::CapturedFrame> frames;
    if(!decoder_->Drain(frames))return Fail("Decoder drain: "+decoder_->Error());
    if(!Consume(frames,sink))return false;
    // Entries discarded on queue recovery no longer represent live pending frames.
    tracking_.clear();state_=VideoLoopState::Stopped;return true;
}
std::string VideoLocalLoop::Status() const {
    if(state_==VideoLoopState::Error)return "Codec error: "+error_;
    const auto& info=encoder_->Info();
    return std::string(state_==VideoLoopState::Recovering ? "H.264 recovering IDR | " : "H.264 decoded local loop | ")+
        info.candidate.name+(info.surface_io ? " | GPU input" : " | CPU input");
}
VideoLoopTailLatency VideoLocalLoop::TailLatency()const {
    return {P95(encode_tail_),P95(decode_tail_),P95(e2e_tail_)};
}
}
