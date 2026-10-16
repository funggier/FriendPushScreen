#include "audio/opus_codec.h"
#include <opus.h>
#include <algorithm>
#include <cmath>
namespace fps::audio {
struct OpusCodec::Impl {OpusEncoder* encoder{};OpusDecoder* decoder{};};
OpusCodec::OpusCodec():impl_(std::make_unique<Impl>()){}
OpusCodec::~OpusCodec(){Stop();}
void OpusCodec::Stop() {
    if(impl_->encoder)opus_encoder_destroy(impl_->encoder);
    if(impl_->decoder)opus_decoder_destroy(impl_->decoder);
    impl_->encoder=nullptr;impl_->decoder=nullptr;
}
bool OpusCodec::Start(std::string& error) {
    Stop();int status=OPUS_OK;
    impl_->encoder=opus_encoder_create(kAudioRate,kAudioChannels,OPUS_APPLICATION_RESTRICTED_LOWDELAY,&status);
    if(status==OPUS_OK&&impl_->encoder)impl_->decoder=opus_decoder_create(kAudioRate,kAudioChannels,&status);
    if(status==OPUS_OK&&impl_->encoder&&impl_->decoder) {
        status=opus_encoder_ctl(impl_->encoder,OPUS_SET_BITRATE(128000));
        if(status==OPUS_OK)status=opus_encoder_ctl(impl_->encoder,OPUS_SET_VBR(1));
        if(status==OPUS_OK)status=opus_encoder_ctl(impl_->encoder,OPUS_SET_VBR_CONSTRAINT(1));
        if(status==OPUS_OK)status=opus_encoder_ctl(impl_->encoder,OPUS_SET_COMPLEXITY(5));
    }
    if(status!=OPUS_OK||!impl_->encoder||!impl_->decoder){error=std::string("Opus initialization: ")+opus_strerror(status);Stop();return false;}
    error.clear();return true;
}
std::vector<std::uint8_t> OpusCodec::Encode(std::span<const float> samples,std::string& error) {
    error.clear();
    if(!impl_->encoder||samples.size()!=kAudioFrames*kAudioChannels||
       !std::all_of(samples.begin(),samples.end(),[](float x){return std::isfinite(x)&&std::abs(x)<=1.0f;})) {error="Invalid PCM frame or inactive encoder";return {};}
    std::vector<std::uint8_t> packet(kAudioPacketLimit);
    const int size=opus_encode_float(impl_->encoder,samples.data(),kAudioFrames,packet.data(),static_cast<opus_int32>(packet.size()));
    if(size<=0){error=std::string("Opus encode: ")+opus_strerror(size);return {};}
    packet.resize(static_cast<std::size_t>(size));return packet;
}
std::vector<float> OpusCodec::Decode(std::span<const std::uint8_t> packet,std::string& error) {
    error.clear();
    if(!impl_->decoder||packet.size()>kAudioPacketLimit||
       (!packet.empty()&&opus_packet_get_nb_samples(packet.data(),static_cast<opus_int32>(packet.size()),kAudioRate)!=kAudioFrames)) {
        error="Invalid Opus packet duration/length or inactive decoder";return {};
    }
    std::vector<float> samples(kAudioFrames*kAudioChannels);
    const int count=opus_decode_float(impl_->decoder,packet.empty()?nullptr:packet.data(),static_cast<opus_int32>(packet.size()),samples.data(),kAudioFrames,0);
    if(count!=static_cast<int>(kAudioFrames)){error="Opus decode failed or changed duration";return {};}
    for(auto& sample:samples)sample=std::isfinite(sample)?std::clamp(sample,-1.0f,1.0f):0.0f;
    return samples;
}
}
