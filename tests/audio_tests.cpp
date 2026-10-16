#include "test_support.h"
#include "audio/audio_model.h"
#include "audio/opus_codec.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace fps::audio;
namespace {
AudioPacket Packet(std::uint64_t seq=1,std::int64_t pts=1000000) {
    AudioPacket p;p.session=7;p.generation=2;p.sequence=seq;p.pts_100ns=pts;p.bytes={0xfc,0xff,0xfe};return p;
}
void AudioWireRejectsInvalidAndStale() {
    std::cout<<"AudioWireRejectsInvalidAndStale\n";
    auto p=Packet();const auto wire=EncodeAudio(p);
    FPS_EXPECT_TRUE(!wire.empty());std::string error;auto round=DecodeAudio(wire,error);
    FPS_EXPECT_TRUE(round.has_value());if(round)FPS_EXPECT_EQ(round->bytes,p.bytes);
    auto truncated=wire;if(!truncated.empty())truncated.pop_back();
    FPS_EXPECT_TRUE(!DecodeAudio(truncated,error));
    auto extra=wire;extra.push_back(0);FPS_EXPECT_TRUE(!DecodeAudio(extra,error));
    p.bytes.resize(901);FPS_EXPECT_TRUE(EncodeAudio(p).empty());
    p=Packet();p.session=0;FPS_EXPECT_TRUE(EncodeAudio(p).empty());
    p=Packet();p.sequence=0;FPS_EXPECT_TRUE(EncodeAudio(p).empty());
    p=Packet();p.pts_100ns=-1;FPS_EXPECT_TRUE(EncodeAudio(p).empty());
    p=Packet();p.duration_frames=480;FPS_EXPECT_TRUE(EncodeAudio(p).empty());
    p=Packet();p.stream=1;FPS_EXPECT_TRUE(EncodeAudio(p).empty());
    auto v=wire;if(v.size()>4)v[4]=99;FPS_EXPECT_TRUE(!DecodeAudio(v,error));
    AudioJitter jitter;jitter.Reset(7,2);
    p=Packet();p.session=8;FPS_EXPECT_TRUE(!jitter.Push(p,100));
    p=Packet();p.generation=1;FPS_EXPECT_TRUE(!jitter.Push(p,100));
    p=Packet();FPS_EXPECT_TRUE(jitter.Push(p,100));
    FPS_EXPECT_TRUE(!jitter.Push(p,101));
}
void AudioJitterBoundsReordersAndRebases() {
    std::cout<<"AudioJitterBoundsReordersAndRebases\n";
    AudioJitter j;j.Reset(7,2);
    FPS_EXPECT_TRUE(j.Push(Packet(2,1200000),100));
    FPS_EXPECT_TRUE(j.Push(Packet(1,1000000),101));
    FPS_EXPECT_TRUE(!j.Pop(139).has_value());
    auto f=j.Pop(140);FPS_EXPECT_TRUE(f&&f->packet&&f->packet->sequence==1);
    FPS_EXPECT_TRUE(!j.Push(Packet(1,1000000),150));
    f=j.Pop(160);FPS_EXPECT_TRUE(f&&f->packet&&f->packet->sequence==2);
    f=j.Pop(180);FPS_EXPECT_TRUE(f&&!f->packet); // Opus20ms PLC.
    FPS_EXPECT_TRUE(j.Push(Packet(4,1600000),181));
    f=j.Pop(200);FPS_EXPECT_TRUE(f&&f->packet&&f->packet->sequence==4);
    for(std::uint64_t n=5;n<100;++n)j.Push(Packet(n,1000000+static_cast<std::int64_t>(n-1)*200000),200);
    FPS_EXPECT_TRUE(j.Depth()<=6);FPS_EXPECT_TRUE(j.Dropped()>0);
    // After a 2 second scheduling gap only a recent frame may restart playback.
    FPS_EXPECT_TRUE(j.Push(Packet(200,40800000),2200));
    f=j.Pop(2240);FPS_EXPECT_TRUE(f&&f->packet&&f->packet->sequence==200);
    FPS_EXPECT_TRUE(j.Depth()<=6);
    j.Reset(7,3);FPS_EXPECT_EQ(j.Depth(),std::size_t{0});
    FPS_EXPECT_TRUE(!j.Push(Packet(201,41000000),2260));
}
void AudioRoutingGuardAndGain() {
    std::cout<<"AudioRoutingGuardAndGain\n";
    FPS_EXPECT_TRUE(FeedbackRisk(true,"endpoint-a","endpoint-a",""));
    FPS_EXPECT_TRUE(FeedbackRisk(true,"endpoint-b","endpoint-a","endpoint-a"));
    FPS_EXPECT_TRUE(!FeedbackRisk(true,"endpoint-b","endpoint-a","endpoint-c"));
    FPS_EXPECT_TRUE(!FeedbackRisk(false,"endpoint-a","endpoint-a",""));
    std::vector<float> data{1,-1,0.5f,-0.5f};
    ApplyGain(data,false,50);FPS_EXPECT_EQ(data[0],0.5f);FPS_EXPECT_EQ(data[1],-0.5f);
    ApplyGain(data,true,100);FPS_EXPECT_TRUE(std::all_of(data.begin(),data.end(),[](float x){return x==0;}));
    data={1,-1};ApplyGain(data,false,0);FPS_EXPECT_EQ(data[0],0.0f);
    data={1,-1};ApplyGain(data,false,100);FPS_EXPECT_EQ(data[0],1.0f);
}
void OpusRoundTripAndPlc() {
    std::cout<<"OpusRoundTripAndPlc\n";
    OpusCodec c;std::string error;FPS_EXPECT_TRUE(c.Start(error));
    double energy=0;std::size_t samples=0;
    for(int frame=0;frame<30;++frame) {
        std::vector<float> input(1920);
        for(int i=0;i<960;++i) {
            const float x=0.1f*std::sin(6.28318530718f*440*(frame*960+i)/48000);
            input[2*i]=input[2*i+1]=x;
        }
        const auto packet=c.Encode(input,error);FPS_EXPECT_TRUE(!packet.empty()&&packet.size()<=900);
        auto output=c.Decode(packet,error);FPS_EXPECT_EQ(output.size(),std::size_t{1920});
        if(frame>3)for(float x:output){FPS_EXPECT_TRUE(std::isfinite(x)&&std::abs(x)<=1);energy+=x*x;++samples;}
    }
    FPS_EXPECT_TRUE(samples>0&&energy/static_cast<double>(std::max<std::size_t>(1,samples))>0.002);
    FPS_EXPECT_EQ(c.Decode({},error).size(),std::size_t{1920}); // Real codec PLC
    std::vector<float> bad(1919);FPS_EXPECT_TRUE(c.Encode(bad,error).empty());
    bad.assign(1920,std::numeric_limits<float>::quiet_NaN());FPS_EXPECT_TRUE(c.Encode(bad,error).empty());
    const std::vector<std::uint8_t> malformed(901,255);FPS_EXPECT_TRUE(c.Decode(malformed,error).empty());
    c.Stop();FPS_EXPECT_TRUE(c.Encode(std::vector<float>(1920),error).empty());
}
}
void RunAudioTests(){AudioWireRejectsInvalidAndStale();AudioJitterBoundsReordersAndRebases();AudioRoutingGuardAndGain();OpusRoundTripAndPlc();}
