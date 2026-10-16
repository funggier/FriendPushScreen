#include "media/video_peer_stream.h"
#include "media/video_frame_normalizer.h"
#include <algorithm>
#include <chrono>
namespace fps::media {
namespace {
std::int64_t Now(){return std::chrono::duration_cast<std::chrono::duration<std::int64_t,std::ratio<1,10000000>>>(
    std::chrono::steady_clock::now().time_since_epoch()).count();}
void Record(std::deque<std::int64_t>& values,std::int64_t value){if(values.size()>=18000)values.pop_front();values.push_back(std::max<std::int64_t>(0,value));}
LatencyPercentiles Percentiles(const std::deque<std::int64_t>& values) {
    if(values.empty())return {};std::vector<std::int64_t> sorted(values.begin(),values.end());std::sort(sorted.begin(),sorted.end());
    return {sorted[(sorted.size()-1)*50/100]/10000.0,sorted[(sorted.size()-1)*95/100]/10000.0,sorted[(sorted.size()-1)*99/100]/10000.0};
}
VideoCodecConfig ConfigureCodec(VideoCodecConfig config,const network::VideoFormat& f) {
    config.width=f.coded_width;config.height=f.coded_height;config.fps_numerator=f.fps_numerator;config.fps_denominator=f.fps_denominator;
    config.target_bitrate_bps=f.bitrate;config.keyframe_interval_frames=f.gop;return config;
}
network::CursorRecord Cursor(const capture::CapturedCursor& cursor) {
    if(!cursor.visible)return {};
    return {true,cursor.x,cursor.y,cursor.width,cursor.height,cursor.row_pitch?cursor.row_pitch:cursor.width*4,cursor.bgra};
}
std::vector<std::uint8_t> ParameterSets(std::span<const std::uint8_t> bytes) {
    std::vector<std::size_t> starts;
    for(std::size_t i=0;i+3<bytes.size();++i) {
        if(bytes[i]==0&&bytes[i+1]==0&&bytes[i+2]==1){starts.push_back(i);i+=2;}
        else if(i+4<bytes.size()&&bytes[i]==0&&bytes[i+1]==0&&bytes[i+2]==0&&bytes[i+3]==1){starts.push_back(i);i+=3;}
    }
    std::vector<std::uint8_t> result;
    for(std::size_t i=0;i<starts.size();++i) {
        const auto start=starts[i],end=i+1<starts.size()?starts[i+1]:bytes.size();
        const auto nal=start+(bytes[start+2]==1?3:4);if(nal>=end)continue;
        const auto type=bytes[nal]&31U;
        if(type==7||type==8){if(end-start>64*1024-result.size())return {};result.insert(result.end(),bytes.begin()+static_cast<std::ptrdiff_t>(start),bytes.begin()+static_cast<std::ptrdiff_t>(end));}
    }return result;
}
}
VideoPeerSender::VideoPeerSender(std::unique_ptr<IVideoEncoder> encoder):encoder_(std::move(encoder)),normalizer_(std::make_unique<VideoFrameNormalizer>()){}
VideoPeerSender::~VideoPeerSender(){Stop();}
bool VideoPeerSender::Initialize(ID3D11Device* device,const network::VideoFormat& format,VideoCodecConfig config,VideoBackendPolicy policy,
    std::uint64_t session,std::uint64_t generation) {
    Stop();error_.clear();metrics_={};latency_.clear();format_=format;session_=session;generation_=generation;
    if(!network::ValidateVideoFormat(format)||!session||!generation){error_="Unsupported outgoing video format/session";return false;}
    config=ConfigureCodec(std::move(config),format);clock_=VideoFrameClock(format.fps_numerator,format.fps_denominator);
    normalizer_->Configure(device,format.coded_width,format.coded_height);
    if(!encoder_->Initialize(device,config,policy)){error_="Encoder: "+encoder_->Error();return false;}
    running_=true;awaiting_keyframe_=true;return RequestKeyframe();
}
void VideoPeerSender::Stop(){encoder_->Stop();normalizer_->Stop();tracking_.clear();parameter_sets_.clear();running_=false;awaiting_keyframe_=true;}
bool VideoPeerSender::Push(const capture::CapturedFrame& source,const Sink& sink) {
    if(!running_)return false;
    if(!Poll(sink))return false;
    ++metrics_.captured;
    if(source.width!=format_.visible_width||source.height!=format_.visible_height){error_="Capture geometry changed; start a new stream generation";return false;}
    if(tracking_.size()>=64){error_="Encoder metadata bound exceeded";return false;}
    capture::CapturedFrame frame;
    if(!normalizer_->Normalize(source,frame,error_))return false;
    const auto timing=clock_.Normalize(source.sequence,source.timestamp_100ns);
    tracking_.emplace(timing.codec_sequence,Tracking{timing,Now(),Cursor(source.cursor)});
    metrics_.max_pending=std::max<std::uint64_t>(metrics_.max_pending,tracking_.size());
    std::vector<EncodedAccessUnit> units;
    if(!encoder_->Encode(frame,timing,units)){error_="Encoder: "+encoder_->Error();return false;}
    return Collect(units,sink);
}
bool VideoPeerSender::Collect(std::vector<EncodedAccessUnit>& units,const Sink& sink) {
    for(auto& unit:units) {
        auto found=tracking_.find(unit.sequence);if(found==tracking_.end()){error_="Encoded unit has no source metadata";return false;}
        auto track=std::move(found->second);tracking_.erase(found);++metrics_.encoded;metrics_.bytes+=unit.bytes.size();
        Record(latency_,Now()-track.capture_time);
        if(unit.keyframe) {
            auto parameters=ParameterSets(unit.bytes);if(!parameters.empty())parameter_sets_=std::move(parameters);
            if(!network::HasRandomAccessParameterSets(unit.bytes)&&!parameter_sets_.empty()) {
                if(unit.bytes.size()>network::kMaxMediaBytes-parameter_sets_.size()){error_="Outgoing access unit exceeds limit";return false;}
                unit.bytes.insert(unit.bytes.begin(),parameter_sets_.begin(),parameter_sets_.end());
            }
        }
        if(awaiting_keyframe_&&(!unit.keyframe||!network::HasRandomAccessParameterSets(unit.bytes))){++metrics_.dropped;continue;}
        network::VideoWireFrame wire;wire.session=session_;wire.generation=generation_;wire.sequence=unit.sequence;
        wire.source_sequence=track.timing.source_sequence;wire.timestamp_100ns=unit.timestamp_100ns;wire.duration_100ns=unit.duration_100ns;
        wire.capture_time_100ns=track.capture_time;wire.format=format_;wire.keyframe=unit.keyframe;wire.cursor=std::move(track.cursor);wire.bytes=std::move(unit.bytes);
        if(!network::IsValidVideoWireFrame(wire)){error_="Outgoing video record or cursor exceeds negotiated bounds";return false;}
        if(!sink(std::move(wire))) {
            ++metrics_.dropped;
            if(!awaiting_keyframe_&&!RequestKeyframe())return false;
            awaiting_keyframe_=true;
        } else {awaiting_keyframe_=false;++metrics_.delivered;}
    }return true;
}
bool VideoPeerSender::Poll(const Sink& sink) {
    if(!running_)return false;std::vector<EncodedAccessUnit> units;
    if(!encoder_->Poll(units)){error_="Encoder poll: "+encoder_->Error();return false;}return Collect(units,sink);
}
bool VideoPeerSender::RequestKeyframe() {
    if(!running_)return false;if(!encoder_->RequestKeyframe()){error_="Encoder keyframe: "+encoder_->Error();return false;}return true;
}
LatencyPercentiles VideoPeerSender::Latency()const{return Percentiles(latency_);}
VideoPeerReceiver::VideoPeerReceiver(std::unique_ptr<IVideoDecoder> decoder):decoder_(std::move(decoder)),normalizer_(std::make_unique<VideoFrameNormalizer>()){}
VideoPeerReceiver::~VideoPeerReceiver(){Stop();}
bool VideoPeerReceiver::Configure(ID3D11Device* device,const network::VideoFormat& format,VideoCodecConfig config,VideoBackendPolicy policy,
    std::uint64_t session,std::uint64_t generation) {
    Stop();error_.clear();metrics_={};decode_latency_.clear();end_to_end_.clear();format_=format;session_=session;generation_=generation;
    if(!network::ValidateVideoFormat(format)||!session||!generation){error_="Unsupported incoming video format/session";return false;}
    normalizer_->Configure(device,format.coded_width,format.coded_height);config=ConfigureCodec(std::move(config),format);
    if(!decoder_->Initialize(device,config,policy)){error_="Decoder: "+decoder_->Error();return false;}
    running_=true;awaiting_keyframe_=true;requested_keyframe_=needs_keyframe_=false;last_sequence_=0;return true;
}
void VideoPeerReceiver::Stop(){decoder_->Stop();normalizer_->Stop();tracking_.clear();running_=false;awaiting_keyframe_=true;requested_keyframe_=needs_keyframe_=false;last_sequence_=0;}
bool VideoPeerReceiver::Recover() {
    if(!running_)return false;
    tracking_.clear();awaiting_keyframe_=true;requested_keyframe_=false;needs_keyframe_=true;
    if(!decoder_->Flush()){error_="Recovery decoder: "+decoder_->Error();return false;}return true;
}
bool VideoPeerReceiver::TakeKeyframeRequest() {
    if(!running_||!awaiting_keyframe_||!needs_keyframe_||requested_keyframe_)return false;
    requested_keyframe_=true;return true;
}
bool VideoPeerReceiver::Push(network::VideoWireFrame wire,const Sink& sink) {
    if(!running_)return false;
    if(wire.session!=session_||wire.generation!=generation_||wire.format!=format_||wire.sequence<=last_sequence_) {++metrics_.dropped;return true;}
    if(last_sequence_&&wire.sequence!=last_sequence_+1){if(!Recover())return false;}
    last_sequence_=wire.sequence;
    if(tracking_.size()>=2){if(!Recover())return false;}
    if(awaiting_keyframe_) {
        if(!wire.keyframe||!network::HasRandomAccessParameterSets(wire.bytes)){++metrics_.dropped;needs_keyframe_=true;return true;}
        awaiting_keyframe_=false;requested_keyframe_=needs_keyframe_=false;
    }
    tracking_.emplace(wire.sequence,Tracking{wire.source_sequence,wire.timestamp_100ns,wire.capture_time_100ns,Now(),
        std::move(wire.cursor),std::move(wire.receive_credit)});
    metrics_.max_pending=std::max<std::uint64_t>(metrics_.max_pending,tracking_.size());
    EncodedAccessUnit unit;unit.sequence=wire.sequence;unit.source_sequence=wire.source_sequence;
    unit.timestamp_100ns=wire.timestamp_100ns;unit.duration_100ns=wire.duration_100ns;unit.keyframe=wire.keyframe;unit.bytes=std::move(wire.bytes);
    metrics_.bytes+=unit.bytes.size();
    std::vector<capture::CapturedFrame> frames;
    if(!decoder_->Decode(unit,frames)){error_="Decoder: "+decoder_->Error();return false;}
    return Consume(frames,sink);
}
bool VideoPeerReceiver::Consume(std::vector<capture::CapturedFrame>& frames,const Sink& sink) {
    for(auto& frame:frames) {
        auto found=tracking_.find(frame.sequence);if(found==tracking_.end()||frame.timestamp_100ns!=found->second.timestamp){error_="Decoded frame metadata mismatch";return false;}
        auto& t=found->second;++metrics_.decoded;Record(decode_latency_,Now()-t.decode_start);
        if(!normalizer_->RestoreVisible(frame,format_.visible_width,format_.visible_height,error_))return false;
        frame.sequence=t.source_sequence;
        frame.cursor={t.cursor.visible,t.cursor.x,t.cursor.y,t.cursor.width,t.cursor.height,t.cursor.row_pitch,std::move(t.cursor.bgra)};
        if(!sink(frame)){error_="Incoming viewer failed";return false;}
        ++metrics_.delivered;Record(end_to_end_,Now()-t.capture_time);tracking_.erase(found);
    }return true;
}
bool VideoPeerReceiver::Poll(const Sink& sink) {
    if(!running_)return false;std::vector<capture::CapturedFrame> frames;
    if(!decoder_->Poll(frames)){error_="Decoder poll: "+decoder_->Error();return false;}return Consume(frames,sink);
}
LatencyPercentiles VideoPeerReceiver::DecodeLatency()const{return Percentiles(decode_latency_);}
LatencyPercentiles VideoPeerReceiver::EndToEndLatency()const{return Percentiles(end_to_end_);}
}
