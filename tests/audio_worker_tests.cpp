#include "audio/audio_worker.h"
#include "test_support.h"
#include <Windows.h>
#include <atomic>
#include <iostream>
using namespace fps::audio;
namespace {
struct SlowEngine final:IPeerAudio {
    HANDLE entered{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    std::atomic<DWORD> tick_thread{0};AudioPreferences preferences;PeerAudioSnapshot snapshot;
    ~SlowEngine()override{CloseHandle(entered);}
    void Configure(AudioPreferences p,bool)override{preferences=std::move(p);}
    void Bind(std::uint64_t,std::uint64_t)override{}
    void OnControl(std::uint64_t,std::span<const std::uint8_t>)override{}
    void OnAudio(std::uint64_t,std::span<const std::uint8_t>)override{}
    void Tick(const ControlSender&,const AudioSender& send)override {
        if(preferences.source_id=="slow") {
            tick_thread=GetCurrentThreadId();SetEvent(entered);Sleep(400);send({1,2,3});
        }
        snapshot.send_state=preferences.source_id=="none"?AudioState::Disabled:AudioState::Active;
    }
    PeerAudioSnapshot Snapshot()const override{return snapshot;}
};
}
void RunAudioWorkerTests() {
    std::cout<<"AudioDeviceInitializationDoesNotBlockTransport\n";
    auto engine=std::make_unique<SlowEngine>();auto* observed=engine.get();
    PeerAudioWorker worker(std::move(engine));AudioPreferences p;p.source_id="slow";
    auto start=GetTickCount64();worker.Configure(3,7,p,true);const auto configureMs=GetTickCount64()-start;
    FPS_EXPECT_TRUE(configureMs<50);
    FPS_EXPECT_EQ(WaitForSingleObject(observed->entered,1000),DWORD{WAIT_OBJECT_0});
    FPS_EXPECT_TRUE(observed->tick_thread.load()!=GetCurrentThreadId());
    start=GetTickCount64();(void)worker.Snapshot();(void)worker.TakeOutgoing();
    p.source_id="none";worker.Configure(3,7,p,true);
    FPS_EXPECT_TRUE(GetTickCount64()-start<50);
    Sleep(500);
    FPS_EXPECT_TRUE(worker.TakeOutgoing().empty()); // Late old-source callback cannot escape No Audio.
    FPS_EXPECT_EQ(worker.Snapshot().send_state,AudioState::Disabled);
}

namespace {
struct ControlProducer final:IPeerAudio {
    HANDLE emitted{CreateEventW(nullptr,TRUE,FALSE,nullptr)};bool sent{},bound{};
    ~ControlProducer()override{CloseHandle(emitted);}
    void Configure(AudioPreferences,bool)override{}
    void Bind(std::uint64_t,std::uint64_t token)override{bound=token!=0;sent=false;}
    void OnControl(std::uint64_t,std::span<const std::uint8_t>)override{}
    void OnAudio(std::uint64_t,std::span<const std::uint8_t>)override{}
    void Tick(const ControlSender& send,const AudioSender&)override {
        if(bound&&!sent){send({1});send({2});send({3});sent=true;SetEvent(emitted);}
    }
    PeerAudioSnapshot Snapshot()const override{return {};}
};
}
namespace {
struct RefillingProducer final:IPeerAudio {
    std::atomic<unsigned> accepted{};bool bound{};
    void Configure(AudioPreferences,bool)override{}
    void Bind(std::uint64_t,std::uint64_t token)override{bound=token!=0;}
    void OnControl(std::uint64_t,std::span<const std::uint8_t>)override{}
    void OnAudio(std::uint64_t,std::span<const std::uint8_t>)override{}
    void Tick(const ControlSender& send,const AudioSender&)override {
        while(bound&&accepted<32){const auto next=accepted.load()+1;if(!send({static_cast<std::uint8_t>(next)}))break;accepted=next;}
    }
    PeerAudioSnapshot Snapshot()const override{return {};}
};
void ConcurrentRefillCannotEvictReliablePrefix() {
    std::cout<<"ConcurrentRefillCannotEvictReliablePrefix\n";
    auto producer=std::make_unique<RefillingProducer>();auto* observed=producer.get();
    PeerAudioWorker worker(std::move(producer));worker.Configure(3,7,{},false);
    auto until=GetTickCount64()+1000;while(observed->accepted<16&&GetTickCount64()<until)Sleep(1);
    FPS_EXPECT_EQ(observed->accepted.load(),16U);
    worker.Dispatch([&](const AudioOutbound&){const auto deadline=GetTickCount64()+100;while(observed->accepted<32&&GetTickCount64()<deadline)Sleep(1);return false;});
    std::vector<std::uint8_t> delivered;
    until=GetTickCount64()+1000;
    while(delivered.size()<32&&GetTickCount64()<until){worker.Dispatch([&](const AudioOutbound& p){delivered.push_back(p.bytes.front());return true;});Sleep(1);}
    std::vector<std::uint8_t> expected;for(unsigned n=1;n<=32;++n)expected.push_back(static_cast<std::uint8_t>(n));
    FPS_EXPECT_EQ(delivered,expected);
}
}
namespace {
struct GatedControlConsumer final:IPeerAudio {
    HANDLE entered{CreateEventW(nullptr,TRUE,FALSE,nullptr)};bool delayed{};
    std::atomic<std::uint64_t> selected{},formatted{},stream{},ready{};
    ~GatedControlConsumer()override{CloseHandle(entered);}
    void Configure(AudioPreferences,bool)override{}
    void Bind(std::uint64_t,std::uint64_t)override{}
    void OnControl(std::uint64_t,std::span<const std::uint8_t> bytes)override {
        std::string error;auto c=DecodeAudioControl(bytes,error);if(!c)return;
        switch(c->kind){case AudioControlKind::SourceSelected:selected=c->generation;break;case AudioControlKind::Format:formatted=c->generation;break;case AudioControlKind::StreamState:stream=c->generation;break;case AudioControlKind::Ready:ready=c->generation;break;default:break;}
    }
    void OnAudio(std::uint64_t,std::span<const std::uint8_t>)override{}
    void Tick(const ControlSender&,const AudioSender&)override{if(!delayed){delayed=true;SetEvent(entered);Sleep(400);}}
    PeerAudioSnapshot Snapshot()const override{return {};}
};
void IncomingBurstRetainsCompleteLatestAudioState() {
    std::cout<<"IncomingBurstRetainsCompleteLatestAudioState\n";
    auto engine=std::make_unique<GatedControlConsumer>();auto* observed=engine.get();PeerAudioWorker worker(std::move(engine));worker.Configure(3,7,{},false);
    FPS_EXPECT_EQ(WaitForSingleObject(observed->entered,1000),DWORD{WAIT_OBJECT_0});
    AudioControl c;c.session=7;worker.OnControl(3,EncodeAudioControl(c));
    for(unsigned n=1;n<=40;++n) {
        c.generation=n;c.kind=AudioControlKind::SourceSelected;c.source=AudioSourceKind::CaptureEndpoint;c.state=AudioState::Disabled;worker.OnControl(3,EncodeAudioControl(c));
        c.source=AudioSourceKind::None;c.kind=AudioControlKind::Format;worker.OnControl(3,EncodeAudioControl(c));
        c.kind=AudioControlKind::StreamState;c.state=AudioState::Starting;worker.OnControl(3,EncodeAudioControl(c));
        c.kind=AudioControlKind::Ready;c.state=AudioState::Disabled;worker.OnControl(3,EncodeAudioControl(c));
    }
    const auto until=GetTickCount64()+1000;while(observed->ready<40&&GetTickCount64()<until)Sleep(1);
    FPS_EXPECT_EQ(observed->selected.load(),40ULL);FPS_EXPECT_EQ(observed->formatted.load(),40ULL);
    FPS_EXPECT_EQ(observed->stream.load(),40ULL);FPS_EXPECT_EQ(observed->ready.load(),40ULL);
}
}
void RunAudioRetryTests() {
    ConcurrentRefillCannotEvictReliablePrefix();IncomingBurstRetainsCompleteLatestAudioState();
    std::cout<<"AudioControlBackpressurePreservesOrder\n";
    auto producer=std::make_unique<ControlProducer>();auto* observed=producer.get();
    PeerAudioWorker worker(std::move(producer));worker.Configure(3,7,{},false);
    FPS_EXPECT_EQ(WaitForSingleObject(observed->emitted,1000),DWORD{WAIT_OBJECT_0});
    worker.Dispatch([](const AudioOutbound&){return false;});
    std::vector<std::uint8_t> delivered;
    worker.Dispatch([&](const AudioOutbound& p){delivered.push_back(p.bytes.front());return true;});
    FPS_EXPECT_EQ(delivered,(std::vector<std::uint8_t>{1,2,3}));
}
