#include "media/video_peer_stream.h"
#include "test_support.h"
using namespace fps;
namespace {
class E final:public media::IVideoEncoder {
public:
    media::VideoBackendPolicy policy{};media::VideoCodecConfig config;media::VideoCodecRuntimeInfo info;
    bool key{true};unsigned requests{};
    bool Initialize(ID3D11Device*,const media::VideoCodecConfig& c,media::VideoBackendPolicy p)override{config=c;policy=p;return true;}
    bool Encode(const capture::CapturedFrame&,const media::NormalizedVideoTiming& t,std::vector<media::EncodedAccessUnit>& out)override{
        media::EncodedAccessUnit u;u.sequence=t.codec_sequence;u.source_sequence=t.source_sequence;u.timestamp_100ns=t.timestamp_100ns;u.duration_100ns=t.duration_100ns;
        u.keyframe=key;u.bytes=key?std::vector<std::uint8_t>{0,0,1,0x67,1,0,0,1,0x68,1,0,0,1,0x65,1}:std::vector<std::uint8_t>{0,0,1,0x61,1};
        key=false;out.push_back(std::move(u));return true;
    }
    bool Poll(std::vector<media::EncodedAccessUnit>&)override{return true;}
    bool Drain(std::vector<media::EncodedAccessUnit>&)override{return true;}
    bool RequestKeyframe()override{++requests;key=true;return true;}
    void Stop()override{}
    const media::VideoCodecRuntimeInfo& Info()const override{return info;}
    std::string Error()const override{return {};}
};
class D final:public media::IVideoDecoder {
public:
    media::VideoBackendPolicy policy{};media::VideoCodecConfig config;media::VideoCodecRuntimeInfo info;unsigned flushes{},submits{};
    bool Initialize(ID3D11Device*,const media::VideoCodecConfig& c,media::VideoBackendPolicy p)override{config=c;policy=p;return true;}
    bool Decode(const media::EncodedAccessUnit& u,std::vector<capture::CapturedFrame>& out)override{
        ++submits;capture::CapturedFrame f;f.width=2;f.height=2;f.sequence=u.sequence;f.timestamp_100ns=u.timestamp_100ns;f.bgra.resize(16);out.push_back(std::move(f));return true;}
    bool Poll(std::vector<capture::CapturedFrame>&)override{return true;}
    bool Drain(std::vector<capture::CapturedFrame>&)override{return true;}
    bool Flush()override{++flushes;return true;}void Stop()override{}
    const media::VideoCodecRuntimeInfo& Info()const override{return info;}std::string Error()const override{return {};}
};
}
void RunVideoPeerStreamTests() {
    auto encoder=std::make_unique<E>();auto* e=encoder.get();media::VideoPeerSender sender(std::move(encoder));
    auto decoder=std::make_unique<D>();auto* d=decoder.get();media::VideoPeerReceiver receiver(std::move(decoder));
    network::VideoFormat format{2,2,2,2};media::VideoCodecConfig config;config.width=2;config.height=2;config.low_latency=false;
    FPS_EXPECT_TRUE(sender.Initialize(nullptr,format,config,media::VideoBackendPolicy::ForceSoftware,123,4));
    FPS_EXPECT_TRUE(receiver.Configure(nullptr,format,config,media::VideoBackendPolicy::RequireHardware,123,4));
    FPS_EXPECT_EQ(e->policy,media::VideoBackendPolicy::ForceSoftware);FPS_EXPECT_EQ(d->policy,media::VideoBackendPolicy::RequireHardware);
    FPS_EXPECT_TRUE(!e->config.low_latency);
    capture::CapturedFrame source;source.width=2;source.height=2;source.bgra.resize(16);source.sequence=42;
    source.cursor={true,1,1,1,1,4,{1,2,3,255}};
    std::vector<network::VideoWireFrame> wire;
    FPS_EXPECT_TRUE(sender.Push(source,[&](auto&& f){wire.push_back(std::move(f));return true;}));
    FPS_EXPECT_EQ(wire.size(),1ULL);
    unsigned rendered=0;auto sink=[&](const capture::CapturedFrame& f){++rendered;FPS_EXPECT_EQ(f.sequence,42ULL);FPS_EXPECT_EQ(f.cursor.x,1);return true;};
    if(!wire.empty()) {
        FPS_EXPECT_EQ(wire[0].source_sequence,42ULL);FPS_EXPECT_EQ(wire[0].generation,4ULL);
        auto stale=wire[0];stale.generation=3;FPS_EXPECT_TRUE(receiver.Push(std::move(stale),sink));FPS_EXPECT_EQ(rendered,0U);
        FPS_EXPECT_TRUE(receiver.Push(wire[0],sink));FPS_EXPECT_EQ(rendered,1U);
        auto gap=wire[0];gap.sequence=3;gap.keyframe=false;gap.bytes={0,0,1,0x61,1};
        FPS_EXPECT_TRUE(receiver.Push(gap,sink));FPS_EXPECT_EQ(rendered,1U);
        FPS_EXPECT_TRUE(receiver.AwaitingKeyframe());FPS_EXPECT_TRUE(receiver.TakeKeyframeRequest());FPS_EXPECT_TRUE(!receiver.TakeKeyframeRequest());
        gap.sequence=4;gap.keyframe=true;gap.bytes=wire[0].bytes;
        FPS_EXPECT_TRUE(receiver.Push(gap,sink));FPS_EXPECT_EQ(rendered,2U);FPS_EXPECT_TRUE(!receiver.AwaitingKeyframe());
        FPS_EXPECT_TRUE(d->flushes>0);
    }
    FPS_EXPECT_TRUE(sender.RequestKeyframe());FPS_EXPECT_TRUE(e->requests>0);
    sender.Stop();receiver.Stop();FPS_EXPECT_TRUE(!sender.Push(source,[](auto&&){return true;}));
}
