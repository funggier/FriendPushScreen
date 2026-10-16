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
void RunAudioRetryTests() {
    std::cout<<"AudioControlBackpressurePreservesOrder\n";
    auto producer=std::make_unique<ControlProducer>();auto* observed=producer.get();
    PeerAudioWorker worker(std::move(producer));worker.Configure(3,7,{},false);
    FPS_EXPECT_EQ(WaitForSingleObject(observed->emitted,1000),DWORD{WAIT_OBJECT_0});
    worker.Dispatch([](const AudioOutbound&){return false;});
    std::vector<std::uint8_t> delivered;
    worker.Dispatch([&](const AudioOutbound& p){delivered.push_back(p.bytes.front());return true;});
    FPS_EXPECT_EQ(delivered,(std::vector<std::uint8_t>{1,2,3}));
}
