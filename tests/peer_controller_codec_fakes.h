#pragma once
#include "media/mf_video_codec.h"
// Test doubles for controller orchestration only. Runtime qualification uses real MFTs.
namespace peer_controller_test {
class Encoder final:public fps::media::IVideoEncoder {
public:
    fps::media::VideoCodecRuntimeInfo info;bool key{true};
    bool Initialize(ID3D11Device*,const fps::media::VideoCodecConfig&,fps::media::VideoBackendPolicy)override{key=true;return true;}
    bool Encode(const fps::capture::CapturedFrame&,const fps::media::NormalizedVideoTiming& t,std::vector<fps::media::EncodedAccessUnit>& out)override {
        fps::media::EncodedAccessUnit unit;unit.sequence=t.codec_sequence;unit.source_sequence=t.source_sequence;
        unit.timestamp_100ns=t.timestamp_100ns;unit.duration_100ns=t.duration_100ns;unit.keyframe=key;
        unit.bytes=key?std::vector<std::uint8_t>{0,0,1,0x67,1,0,0,1,0x68,1,0,0,1,0x65,1}:std::vector<std::uint8_t>{0,0,1,0x61,1};
        key=false;out.push_back(std::move(unit));return true;
    }
    bool Poll(std::vector<fps::media::EncodedAccessUnit>&)override{return true;}
    bool Drain(std::vector<fps::media::EncodedAccessUnit>&)override{return true;}
    bool RequestKeyframe()override{key=true;return true;}void Stop()override{}
    const fps::media::VideoCodecRuntimeInfo& Info()const override{return info;}
    std::string Error()const override{return {};}
};
class Decoder final:public fps::media::IVideoDecoder {
public:
    fps::media::VideoCodecRuntimeInfo info;std::string error;unsigned initializations{};
    bool Initialize(ID3D11Device*,const fps::media::VideoCodecConfig& config,fps::media::VideoBackendPolicy)override {
        ++initializations;error=config.decoder_id=="missing-decoder"?"missing decoder registration":"";return error.empty();
    }
    bool Decode(const fps::media::EncodedAccessUnit& unit,std::vector<fps::capture::CapturedFrame>& out)override {
        fps::capture::CapturedFrame frame;frame.width=frame.height=2;frame.bgra.resize(16);
        frame.sequence=unit.sequence;frame.timestamp_100ns=unit.timestamp_100ns;out.push_back(std::move(frame));return true;
    }
    bool Poll(std::vector<fps::capture::CapturedFrame>&)override{return true;}
    bool Drain(std::vector<fps::capture::CapturedFrame>&)override{return true;}
    bool Flush()override{return true;}void Stop()override{}
    const fps::media::VideoCodecRuntimeInfo& Info()const override{return info;}
    std::string Error()const override{return error;}
};
}
