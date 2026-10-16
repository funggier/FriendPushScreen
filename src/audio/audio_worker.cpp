#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>
#include "audio/audio_worker.h"
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace fps::audio {
struct PeerAudioWorker::Impl {
    struct IncomingControl {std::uint64_t connection{};AudioControl message;std::vector<std::uint8_t> bytes;};
    std::unique_ptr<IPeerAudio> engine;
    mutable std::mutex mutex;std::condition_variable changed;std::jthread thread;
    AudioPreferences preferences;bool share{};
    std::uint64_t connection{},session{},revision{1},sessionRevision{1},sourceRevision{1},drops{};
    std::deque<IncomingControl> incomingControl;std::deque<network::TransportEvent> incomingAudio;
    std::deque<AudioOutbound> outgoingControl,outgoingAudio;
    PeerAudioSnapshot snapshot;
    bool Emit(std::vector<std::uint8_t> bytes,bool control,std::uint64_t sr,std::uint64_t ar,std::uint64_t id) {
        std::lock_guard lock(mutex);
        if(sr!=sessionRevision||id!=connection||!session||(!control&&ar!=sourceRevision)){++drops;return false;}
        auto& queue=control?outgoingControl:outgoingAudio;
        if(queue.size()>=(control?16:6)){if(!control)++drops;return false;}
        queue.push_back({id,sr,control,std::move(bytes)});return true;
    }
    void Run(std::stop_token stop) {
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        std::uint64_t applied{},lastSnapshot{};
        while(!stop.stop_requested()) {
            AudioPreferences config;std::uint64_t currentRevision,sr,ar,id,token;bool sharing;
            std::deque<IncomingControl> controls;std::deque<network::TransportEvent> audio;
            {
                std::lock_guard lock(mutex);
                currentRevision=revision;sr=sessionRevision;ar=sourceRevision;id=connection;token=session;sharing=share;
                if(applied!=currentRevision)config=preferences;
                controls.swap(incomingControl);audio.swap(incomingAudio);
            }
            // No OS/codec call runs with the queue mutex or on the transport thread.
            if(applied!=currentRevision){engine->Configure(std::move(config),sharing);engine->Bind(id,token);applied=currentRevision;}
            for(auto& c:controls)engine->OnControl(c.connection,c.bytes);
            for(auto& frame:audio)engine->OnAudio(frame.connection_id,frame.bytes);
            audio.clear(); // Return transport receive credits immediately after bounded audio consumption.
            engine->Tick([&](auto bytes){return Emit(std::move(bytes),true,sr,ar,id);},
                [&](auto bytes){return Emit(std::move(bytes),false,sr,ar,id);});
            if(GetTickCount64()-lastSnapshot>=20) {
                auto next=engine->Snapshot();lastSnapshot=GetTickCount64();
                std::lock_guard lock(mutex);snapshot=std::move(next);
            }
            std::unique_lock lock(mutex);
            changed.wait_for(lock,std::chrono::milliseconds(session?1:100),[&]{return stop.stop_requested()||revision!=applied||!incomingControl.empty()||!incomingAudio.empty();});
        }
        engine->Bind(0,0);
        engine.reset(); // COM objects must die on their owning initialized audio thread.
        if(SUCCEEDED(com))CoUninitialize();
    }
};
PeerAudioWorker::PeerAudioWorker(std::unique_ptr<IPeerAudio> engine):impl_(std::make_unique<Impl>()) {
    impl_->engine=engine?std::move(engine):std::make_unique<PeerAudio>();
    impl_->thread=std::jthread([p=impl_.get()](std::stop_token stop){p->Run(stop);});
}
PeerAudioWorker::~PeerAudioWorker() {
    if(impl_->thread.joinable()){impl_->thread.request_stop();impl_->changed.notify_all();impl_->thread.join();}
}
void PeerAudioWorker::Configure(std::uint64_t connection,std::uint64_t session,AudioPreferences preferences,bool share) {
    auto& p=*impl_;std::lock_guard lock(p.mutex);
    const bool rebind=p.connection!=connection||p.session!=session;
    const bool sourceChanged=rebind||p.preferences.source_id!=preferences.source_id||p.share!=share;
    if(!rebind&&!sourceChanged&&p.preferences==preferences)return;
    if(rebind){++p.sessionRevision;p.outgoingControl.clear();p.incomingControl.clear();p.incomingAudio.clear();}
    if(sourceChanged){++p.sourceRevision;p.outgoingAudio.clear();}
    p.connection=connection;p.session=session;p.preferences=std::move(preferences);p.share=share;++p.revision;p.changed.notify_one();
}
void PeerAudioWorker::OnControl(std::uint64_t connection,std::vector<std::uint8_t> bytes) {
    std::string error;auto message=DecodeAudioControl(bytes,error);
    auto& p=*impl_;std::lock_guard lock(p.mutex);
    if(connection!=p.connection||!message||message->session!=p.session){++p.drops;return;}
    const auto kind=message->kind;
    const bool ready=kind==AudioControlKind::Ready,cap=kind==AudioControlKind::Capabilities;
    std::uint64_t latest=0;
    for(const auto& queued:p.incomingControl) {
        const auto k=queued.message.kind;
        if((ready&&k==AudioControlKind::Ready)||(!ready&&!cap&&k!=AudioControlKind::Ready&&k!=AudioControlKind::Capabilities))
            latest=std::max(latest,queued.message.generation);
    }
    if(!cap&&message->generation<latest){++p.drops;return;}
    // Reliable source/format/state form one latest-generation transaction.
    // Ready describes the opposite direction and is coalesced independently.
    std::erase_if(p.incomingControl,[&](const auto& queued){
        const auto k=queued.message.kind;
        if(k==kind)return true;
        return !ready&&!cap&&k!=AudioControlKind::Ready&&k!=AudioControlKind::Capabilities&&queued.message.generation<message->generation;
    });
    p.incomingControl.push_back({connection,std::move(*message),std::move(bytes)});
    p.changed.notify_one(); // At most five control kinds, never a partial overflow drop.
}
void PeerAudioWorker::OnAudio(network::TransportEvent event) {
    auto& p=*impl_;std::lock_guard lock(p.mutex);
    if(event.connection_id!=p.connection||event.bytes.size()>network::kMaxAudioBytes||p.incomingAudio.size()>=6){++p.drops;return;}
    p.incomingAudio.push_back(std::move(event));p.changed.notify_one();
}
std::vector<AudioOutbound> PeerAudioWorker::TakeOutgoing() {
    auto& p=*impl_;std::lock_guard lock(p.mutex);std::vector<AudioOutbound> result;
    result.reserve(p.outgoingControl.size()+p.outgoingAudio.size());
    for(auto* queue:{&p.outgoingControl,&p.outgoingAudio})while(!queue->empty()){result.push_back(std::move(queue->front()));queue->pop_front();}
    return result;
}
void PeerAudioWorker::Dispatch(const std::function<bool(const AudioOutbound&)>& send) {
    auto& p=*impl_;
    for(unsigned n=0;n<16;++n) {
        AudioOutbound packet;
        {
            std::lock_guard lock(p.mutex);if(p.outgoingControl.empty())break;
            packet=p.outgoingControl.front(); // Its slot stays reserved while send runs.
        }
        if(!send(packet))break;
        std::lock_guard lock(p.mutex);
        if(!p.outgoingControl.empty()&&p.outgoingControl.front().connection==packet.connection&&
           p.outgoingControl.front().session_revision==packet.session_revision)p.outgoingControl.pop_front();
    }
    std::deque<AudioOutbound> audio;
    {std::lock_guard lock(p.mutex);audio.swap(p.outgoingAudio);}
    for(const auto& packet:audio)if(!send(packet))SendDropped();
}
void PeerAudioWorker::SendDropped(){std::lock_guard lock(impl_->mutex);++impl_->drops;}
PeerAudioSnapshot PeerAudioWorker::Snapshot()const {std::lock_guard lock(impl_->mutex);auto result=impl_->snapshot;result.dropped+=impl_->drops;return result;}
}
