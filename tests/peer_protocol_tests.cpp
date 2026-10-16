#include "network/peer_protocol.h"
#include "network/peer_video_flow.h"
#include "test_support.h"
#include <limits>
using namespace fps::network;
void RunPeerProtocolTests() {
    VideoFormat f{1920,1080,1919,1079,60,1,20'000'000,120,true};std::string error;
    FPS_EXPECT_TRUE(ValidateVideoFormat(f));auto bad=f;bad.coded_width=1919;FPS_EXPECT_TRUE(!ValidateVideoFormat(bad));
    bad=f;bad.visible_width=1921;FPS_EXPECT_TRUE(!ValidateVideoFormat(bad));
    bad={4096,4096,4096,4096};FPS_EXPECT_TRUE(!ValidateVideoFormat(bad));
    bad=f;bad.fps_denominator=0;FPS_EXPECT_TRUE(!ValidateVideoFormat(bad));
    for(auto type:{ControlType::Hello,ControlType::ShareStart,ControlType::VideoFormat,ControlType::ShareStop,
        ControlType::KeyframeRequest,ControlType::Ping,ControlType::Pong,ControlType::Goodbye,ControlType::Error,ControlType::VideoReady,ControlType::VideoConsumed}) {
        ControlMessage m;m.type=type;m.session=123;m.generation=7;m.value=99;m.listen_port=42801;m.format=f;m.text="controlled error";
        const auto bytes=EncodeControl(m);FPS_EXPECT_TRUE(!bytes.empty());
        const auto decoded=DecodeControl(bytes,error);FPS_EXPECT_TRUE(decoded.has_value());
        if(decoded){FPS_EXPECT_EQ(decoded->type,type);FPS_EXPECT_EQ(decoded->session,123ULL);FPS_EXPECT_EQ(decoded->generation,7ULL);
            if(type==ControlType::VideoFormat)FPS_EXPECT_EQ(decoded->format,f);}
        for(std::size_t n=0;n<bytes.size();++n)FPS_EXPECT_TRUE(!DecodeControl(std::span(bytes).first(n),error));
        if(!bytes.empty()) {
            auto mutated=bytes;mutated[5]=2;FPS_EXPECT_TRUE(!DecodeControl(mutated,error));
            mutated=bytes;mutated[6]=255;FPS_EXPECT_TRUE(!DecodeControl(mutated,error));
            mutated=bytes;mutated[7]=1;FPS_EXPECT_TRUE(!DecodeControl(mutated,error));
            mutated=bytes;mutated.push_back(0);FPS_EXPECT_TRUE(!DecodeControl(mutated,error));
        }
    }
    PeerVideoSendWindow window;window.Reset(123,7);
    auto first=std::make_shared<int>(1),second=std::make_shared<int>(2);
    std::weak_ptr<int> held_first=first,held_second=second;
    FPS_EXPECT_TRUE(window.Submit(1,100,std::move(first)));
    FPS_EXPECT_TRUE(window.Submit(2,110,std::move(second)));
    FPS_EXPECT_TRUE(!held_first.expired()&&!held_second.expired());
    FPS_EXPECT_TRUE(!window.CanSubmit());FPS_EXPECT_TRUE(!window.Submit(3,120,{}));
    ControlMessage consumed;consumed.type=ControlType::VideoConsumed;consumed.session=123;consumed.generation=7;consumed.value=1;
    const auto consumed_wire=EncodeControl(consumed);FPS_EXPECT_TRUE(!consumed_wire.empty());
    consumed.generation=6;FPS_EXPECT_TRUE(!window.Acknowledge(consumed));
    consumed.generation=7;consumed.session=999;FPS_EXPECT_TRUE(!window.Acknowledge(consumed));
    consumed.session=123;consumed.value=999;FPS_EXPECT_TRUE(!window.Acknowledge(consumed));
    consumed.value=1;FPS_EXPECT_TRUE(window.Acknowledge(consumed));
    FPS_EXPECT_TRUE(held_first.expired()&&!held_second.expired());
    FPS_EXPECT_TRUE(!window.Acknowledge(consumed));FPS_EXPECT_TRUE(window.CanSubmit());
    FPS_EXPECT_TRUE(!window.Expire(359));FPS_EXPECT_TRUE(window.Expire(360));
    FPS_EXPECT_TRUE(!held_second.expired());FPS_EXPECT_EQ(window.Pending(),std::size_t{1});
    FPS_EXPECT_TRUE(!window.CanSubmit());FPS_EXPECT_TRUE(!window.Expire(361));
    window.Reset(123,7);FPS_EXPECT_TRUE(held_second.expired());
    FPS_EXPECT_TRUE(window.Submit(3,400,{}));window.Reset(123,8);FPS_EXPECT_EQ(window.Pending(),std::size_t{0});
    VideoWireFrame v;v.session=789;v.generation=3;v.sequence=42;v.source_sequence=21;
    v.timestamp_100ns=700;v.duration_100ns=166666;v.capture_time_100ns=800;v.format=f;v.keyframe=true;
    v.bytes={0,0,0,1,0x67,1,0,0,0,1,0x68,1,0,0,1,0x65,3,4};
    v.cursor={true,-1,2,2,1,8,{1,2,3,4,5,6,7,8}};
    auto bytes=EncodeVideo(v);FPS_EXPECT_TRUE(!bytes.empty());auto result=DecodeVideo(bytes,error);
    FPS_EXPECT_TRUE(result.has_value());if(result){FPS_EXPECT_EQ(result->sequence,42ULL);FPS_EXPECT_EQ(result->source_sequence,21ULL);
        FPS_EXPECT_EQ(result->format,f);FPS_EXPECT_EQ(result->cursor,v.cursor);FPS_EXPECT_EQ(result->bytes,v.bytes);}
    for(std::size_t n=0;n<bytes.size();++n)FPS_EXPECT_TRUE(!DecodeVideo(std::span(bytes).first(n),error));
    if(!bytes.empty()){auto mutated=bytes;mutated[7]|=0x80;FPS_EXPECT_TRUE(!DecodeVideo(mutated,error));}
    FPS_EXPECT_TRUE(HasRandomAccessParameterSets(v.bytes));FPS_EXPECT_TRUE(!HasRandomAccessParameterSets({}));
    auto oversized=v;oversized.bytes.resize(kMaxMediaBytes);FPS_EXPECT_TRUE(EncodeVideo(oversized).empty());
    auto cursor=v;cursor.cursor.width=std::numeric_limits<std::uint32_t>::max();FPS_EXPECT_TRUE(EncodeVideo(cursor).empty());
    cursor=v;cursor.cursor.bgra.pop_back();FPS_EXPECT_TRUE(EncodeVideo(cursor).empty());
    auto seq=v;seq.sequence=0;FPS_EXPECT_TRUE(EncodeVideo(seq).empty());
}
