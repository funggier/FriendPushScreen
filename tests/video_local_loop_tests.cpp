#include "media/video_local_loop.h"
#include "test_support.h"
using namespace fps;
namespace {
class Encoder final:public media::IVideoEncoder {
public:
    media::VideoCodecRuntimeInfo info; bool key=false; unsigned requests=0;
    std::vector<media::EncodedAccessUnit> delayed;
    void Stop()override{}
    bool Initialize(ID3D11Device*,const media::VideoCodecConfig&,media::VideoBackendPolicy) override {return true;}
    bool Encode(const capture::CapturedFrame&,const media::NormalizedVideoTiming& t,std::vector<media::EncodedAccessUnit>& out) override {
        media::EncodedAccessUnit unit;unit.sequence=t.codec_sequence;unit.source_sequence=t.source_sequence;
        unit.timestamp_100ns=t.timestamp_100ns;unit.duration_100ns=t.duration_100ns;
        unit.bytes={1};unit.keyframe=key || t.codec_sequence==1;key=false;
        if(t.codec_sequence<=3){delayed.push_back(std::move(unit));if(t.codec_sequence==3)out=std::move(delayed);}
        else out.push_back(std::move(unit));return true;
    }
    bool Poll(std::vector<media::EncodedAccessUnit>&)override{return true;}
    bool Drain(std::vector<media::EncodedAccessUnit>&)override{return true;}
    bool RequestKeyframe()override{++requests;key=true;return true;}
    const media::VideoCodecRuntimeInfo& Info()const override{return info;}
    std::string Error()const override{return {};}
};
class Decoder final:public media::IVideoDecoder {
public:
    media::VideoCodecRuntimeInfo info;unsigned flushes=0;bool fail=false;
    void Stop()override{}
    bool Initialize(ID3D11Device*,const media::VideoCodecConfig&,bool)override{return true;}
    bool Decode(const media::EncodedAccessUnit& unit,std::vector<capture::CapturedFrame>& out)override {
        if(fail)return false;
        capture::CapturedFrame f;f.sequence=unit.sequence;f.timestamp_100ns=unit.timestamp_100ns;
        f.width=2;f.height=2;f.bgra.resize(16);out.push_back(std::move(f));return true;
    }
    bool Poll(std::vector<capture::CapturedFrame>&)override{return true;}
    bool Drain(std::vector<capture::CapturedFrame>&)override{return true;}
    bool Flush()override{++flushes;return true;}
    const media::VideoCodecRuntimeInfo& Info()const override{return info;}
    std::string Error()const override{return "decode test failure";}
};
}
void RunVideoLocalLoopTests() {
    auto encoder=std::make_unique<Encoder>();auto* e=encoder.get();
    auto decoder=std::make_unique<Decoder>();auto* d=decoder.get();
    media::VideoLocalLoop loop(std::move(encoder),std::move(decoder));
    media::VideoCodecConfig config;config.width=2;config.height=2;
    FPS_EXPECT_TRUE(loop.Initialize(nullptr,config,media::VideoBackendPolicy::PreferHardware));
    capture::CapturedFrame source;source.width=2;source.height=2;source.bgra.resize(16);
    source.cursor.visible=true;source.cursor.width=1;source.cursor.height=1;source.cursor.bgra.resize(4);
    source.cursor.x=42;
    unsigned delivered=0;
    auto sink=[&](const capture::CapturedFrame& frame){
        ++delivered;FPS_EXPECT_EQ(frame.cursor.x,42);FPS_EXPECT_TRUE(frame.cursor.IsValid());return true;
    };
    for(unsigned i=1;i<=3;++i){source.sequence=i;source.timestamp_100ns=0;FPS_EXPECT_TRUE(loop.Push(source,sink));}
    FPS_EXPECT_EQ(delivered,0U);FPS_EXPECT_EQ(loop.Metrics().max_queue_depth,2U);
    FPS_EXPECT_EQ(loop.Metrics().dropped_frames,3U);
    FPS_EXPECT_EQ(loop.Metrics().recovery_resets,1U);
    FPS_EXPECT_EQ(loop.State(),media::VideoLoopState::Recovering);
    FPS_EXPECT_EQ(e->requests,1U);FPS_EXPECT_EQ(d->flushes,1U);
    source.sequence=4;FPS_EXPECT_TRUE(loop.Push(source,sink));
    FPS_EXPECT_TRUE(loop.Poll(sink));
    FPS_EXPECT_EQ(delivered,1U);FPS_EXPECT_EQ(loop.State(),media::VideoLoopState::Running);
    FPS_EXPECT_EQ(loop.Metrics().capture_frames,4U);FPS_EXPECT_EQ(loop.Metrics().rendered_frames,1U);
    FPS_EXPECT_EQ(loop.Metrics().current_queue_depth,0U);
    d->fail=true;source.sequence=5;
    FPS_EXPECT_TRUE(!loop.Push(source,sink));
    FPS_EXPECT_EQ(loop.State(),media::VideoLoopState::Error);
    FPS_EXPECT_TRUE(loop.Error().find("decode test failure")!=std::string::npos);
    FPS_EXPECT_TRUE(!loop.Poll(sink));
    loop.Stop();FPS_EXPECT_EQ(loop.State(),media::VideoLoopState::Stopped);
    FPS_EXPECT_TRUE(!loop.Push(source,sink));
}
