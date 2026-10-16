#include "audio/audio_model.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace fps::audio {
namespace {
bool Valid(const AudioPacket& p) {
    return p.session&&p.generation&&p.sequence&&p.sequence<std::numeric_limits<std::uint64_t>::max()&&
        p.pts_100ns>=0&&p.stream==2&&p.duration_frames==kAudioFrames&&!p.bytes.empty()&&p.bytes.size()<=kAudioPacketLimit;
}
void Put(std::vector<std::uint8_t>& out,std::uint64_t n,unsigned bytes) {
    for(unsigned i=bytes;i>0;--i)out.push_back(static_cast<std::uint8_t>(n>>((i-1)*8)));
}
std::uint64_t Get(std::span<const std::uint8_t> in,std::size_t offset,unsigned n) {
    std::uint64_t value=0;for(unsigned i=0;i<n;++i)value=(value<<8)|in[offset+i];return value;
}
}
std::vector<std::uint8_t> EncodeAudio(const AudioPacket& p) {
    if(!Valid(p))return {};
    std::vector<std::uint8_t> out{'F','P','A','1',1,0,0,52};out.reserve(52+p.bytes.size());
    Put(out,p.session,8);Put(out,p.generation,8);Put(out,p.sequence,8);Put(out,static_cast<std::uint64_t>(p.pts_100ns),8);
    Put(out,p.stream,4);Put(out,p.duration_frames,4);Put(out,p.bytes.size(),4);
    out.insert(out.end(),p.bytes.begin(),p.bytes.end());return out;
}
std::optional<AudioPacket> DecodeAudio(std::span<const std::uint8_t> in,std::string& error) {
    error.clear();
    if(in.size()<53||in.size()>52+kAudioPacketLimit||in[0]!='F'||in[1]!='P'||in[2]!='A'||in[3]!='1'||
       in[4]!=1||in[5]||in[6]||in[7]!=52||Get(in,48,4)!=in.size()-52) {error="Invalid audio record header/length";return {};}
    AudioPacket p;p.session=Get(in,8,8);p.generation=Get(in,16,8);p.sequence=Get(in,24,8);
    const auto pts=Get(in,32,8);
    if(pts>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())){error="Invalid audio timestamp";return {};}
    p.pts_100ns=static_cast<std::int64_t>(pts);p.stream=static_cast<std::uint32_t>(Get(in,40,4));
    p.duration_frames=static_cast<std::uint32_t>(Get(in,44,4));p.bytes.assign(in.begin()+52,in.end());
    if(!Valid(p)){error="Invalid audio record fields";return {};}return p;
}
void AudioJitter::Reset(std::uint64_t session,std::uint64_t generation) {
    session_=session;generation_=generation;next_=played_=due_=last_arrival_=0;misses_=0;packets_.clear();
}
bool AudioJitter::Push(AudioPacket p,std::uint64_t now) {
    if(!Valid(p)||p.session!=session_||p.generation!=generation_){++dropped_;return false;}
    if(last_arrival_&&now-last_arrival_>500){dropped_+=packets_.size();packets_.clear();next_=due_=0;misses_=0;}
    if(p.sequence<=played_||(next_&&p.sequence<next_&&due_<=now)){++dropped_;return false;}
    if(packets_.contains(p.sequence)){++dropped_;return false;}
    if(!next_){next_=p.sequence;due_=now+40;}
    else if(p.sequence<next_&&now<due_&&misses_==0)next_=p.sequence;
    last_arrival_=now;packets_.emplace(p.sequence,std::move(p));
    while(packets_.size()>kAudioQueueLimit){packets_.erase(packets_.begin());++dropped_;next_=std::max(next_,packets_.begin()->first);}
    return true;
}
std::optional<PlayoutFrame> AudioJitter::Pop(std::uint64_t now) {
    if(!next_||now<due_)return {};
    if(now-due_>120) {
        if(packets_.empty()){next_=due_=0;return {};}
        auto newest=std::move(packets_.rbegin()->second);dropped_+=packets_.size()-1;packets_.clear();
        next_=newest.sequence;packets_.emplace(newest.sequence,std::move(newest));due_=now+40;return {};
    }
    PlayoutFrame result;
    if(auto found=packets_.find(next_);found!=packets_.end()){result.packet=std::move(found->second);packets_.erase(found);misses_=0;}
    else ++misses_;
    played_=next_;++next_;due_+=20;
    if(misses_>3&&packets_.empty()){next_=due_=0;misses_=0;}
    return result;
}
std::size_t AudioJitter::Depth()const{return packets_.size();}
std::uint64_t AudioJitter::Dropped()const{return dropped_;}
bool FeedbackRisk(bool same,const std::string& remote,const std::string& output,const std::string& own) {
    return !output.empty()&&((same&&!remote.empty()&&remote==output)||(!own.empty()&&own==output));
}
void ApplyGain(std::span<float> samples,bool muted,int volume) {
    const float gain=muted?0.0f:static_cast<float>(std::clamp(volume,0,100))/100.0f;
    for(auto& sample:samples)sample=std::isfinite(sample)?std::clamp(sample,-1.0f,1.0f)*gain:0.0f;
}
}
