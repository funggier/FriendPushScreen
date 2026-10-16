#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "audio/peer_audio.h"
#include "audio/opus_codec.h"
#include "network/peer_identity.h"
#include <algorithm>
#include <cmath>
namespace fps::audio {
namespace {
std::string EndpointHash(const std::string& endpoint){return endpoint.empty()?std::string{}:network::Sha256({reinterpret_cast<const std::uint8_t*>(endpoint.data()),endpoint.size()});}
}
struct PeerAudio::Impl {
    AudioPreferences preferences;bool share{},restart{true};
    std::uint64_t connection{},lastCatalog{},retrySource{},retryOutput{},lastOutputCheck{};
    std::uint64_t configuredIncoming{},sequence{};
    AudioPeerSession session;WasapiAudioSource source;WasapiAudioRenderer renderer;
    OpusCodec encoder,decoder;AudioJitter jitter;PeerAudioSnapshot snapshot;
    std::vector<float> pending;std::int64_t pendingPts{};double captureLatencySum{};
    std::string sendError,receiveError,outputEndpoint;
    std::deque<std::vector<std::uint8_t>> controls;
    void StopSource(){source.Stop();encoder.Stop();pending.clear();sequence=0;}
    void StopReceiver(){renderer.Stop();decoder.Stop();jitter.Reset(0,0);configuredIncoming=0;snapshot.receiving=false;}
    void FailSource(const std::string& error,AudioState state) {
        sendError=error;StopSource();snapshot.send_state=state;retrySource=GetTickCount64()+2000;session.SetOutgoingState(state);
    }
    void StartSource() {
        StopSource();sendError.clear();retrySource=0;
        if(!share||preferences.source_id.empty()||preferences.source_id=="none") {
            snapshot.send_state=AudioState::Disabled;session.SelectSource(AudioSourceKind::None,{});return;
        }
        std::string error;
        if(!source.Start(preferences.source_id,error)||!encoder.Start(error)) {
            // Publish a disabled new generation before its error, removing any former incoming audio.
            session.SelectSource(AudioSourceKind::None,{});FailSource(error,AudioState::Error);return;
        }
        ++snapshot.source_restarts;snapshot.input_format=source.Format();snapshot.source=source.Descriptor().name;
        session.SelectSource(source.Descriptor().kind,EndpointHash(source.LoopbackEndpoint()));snapshot.send_state=AudioState::Starting;
    }
    void Incoming() {
        const auto& state=session.State();
        if(!state.incoming_generation||(!state.incoming_ready&&!state.format_pending)) {
            if(configuredIncoming)StopReceiver();return;
        }
        if(state.format_pending&&configuredIncoming!=state.incoming_generation) {
            StopReceiver();receiveError.clear();std::string error;
            if(!decoder.Start(error)){receiveError=error;return;}
            configuredIncoming=state.incoming_generation;jitter.Reset(state.session,configuredIncoming);session.ReceiverReady();
        }
    }
    void Output(std::uint64_t now) {
        snapshot.feedback_blocked=false;
        if(!session.State().incoming_ready||!configuredIncoming){if(renderer.Active())renderer.Stop();return;}
        if(now-lastOutputCheck>=1000||outputEndpoint.empty()) {
            lastOutputCheck=now;std::string error;auto endpoint=ResolveOutputEndpoint(preferences.output_id,error);
            if(endpoint!=outputEndpoint){renderer.Stop();outputEndpoint=endpoint;retryOutput=0;}
            if(endpoint.empty()){receiveError=error;retryOutput=now+2000;}
        }
        const auto remote=session.State().remote_endpoint,own=EndpointHash(source.LoopbackEndpoint()),outputHash=EndpointHash(outputEndpoint);
        snapshot.feedback_blocked=FeedbackRisk(true,remote,outputHash,own);
        if(snapshot.feedback_blocked||preferences.muted||preferences.volume==0||outputEndpoint.empty()) {
            if(renderer.Active())renderer.Stop();return;
        }
        if(!renderer.Active()&&now>=retryOutput) {
            std::string error;if(!renderer.Start(outputEndpoint,error)){receiveError=error;retryOutput=now+2000;}else receiveError.clear();
        }
        renderer.SetGain(preferences.muted,preferences.volume);
    }
};
PeerAudio::PeerAudio():impl_(std::make_unique<Impl>()){}
PeerAudio::~PeerAudio()=default;
void PeerAudio::Configure(AudioPreferences preferences,bool share) {
    auto& p=*impl_;preferences.volume=std::clamp(preferences.volume,0,100);
    if(preferences.source_id!=p.preferences.source_id||share!=p.share)p.restart=true;
    if(preferences.output_id!=p.preferences.output_id){p.renderer.Stop();p.outputEndpoint.clear();p.retryOutput=p.lastOutputCheck=0;}
    p.preferences=std::move(preferences);p.share=share;p.renderer.SetGain(p.preferences.muted,p.preferences.volume);
}
void PeerAudio::Bind(std::uint64_t connection,std::uint64_t token) {
    auto& p=*impl_;if(p.connection==connection&&p.session.State().session==token)return;
    p.StopSource();p.StopReceiver();p.controls.clear();p.session.Bind(token);p.connection=token?connection:0;
    p.restart=true;p.retrySource=0;p.sendError.clear();p.receiveError.clear();p.snapshot.send_state=AudioState::Disabled;
}
void PeerAudio::OnControl(std::uint64_t id,std::span<const std::uint8_t> wire) {
    auto& p=*impl_;if(!id||id!=p.connection)return;std::string error;auto message=DecodeAudioControl(wire,error);
    if(!message||!p.session.OnControl(*message)){++p.snapshot.dropped;return;}p.Incoming();
}
void PeerAudio::OnAudio(std::uint64_t id,std::span<const std::uint8_t> wire) {
    auto& p=*impl_;std::string error;auto packet=DecodeAudio(wire,error);
    if(id!=p.connection||!packet||!p.session.State().incoming_ready||packet->session!=p.session.State().session||
       packet->generation!=p.configuredIncoming){++p.snapshot.dropped;return;}
    // Current transport is loopback-only: QPC is the shared session clock on this host.
    const auto age=AudioNow100ns()-packet->pts_100ns;
    if(age< -2500000||age>2500000){++p.snapshot.dropped;return;}
    ++p.snapshot.received;
    if(!p.jitter.Push(std::move(*packet),GetTickCount64()))++p.snapshot.dropped;
    p.snapshot.max_jitter_depth=std::max(p.snapshot.max_jitter_depth,p.jitter.Depth());
}
void PeerAudio::Tick(const ControlSender& sendControl,const AudioSender& sendAudio) {
    auto& p=*impl_;auto now=GetTickCount64();
    if(!p.lastCatalog||now-p.lastCatalog>=5000) {p.lastCatalog=now;p.snapshot.catalog=EnumerateAudioDevices(p.preferences.windows_build);}
    if(p.session.State().session&&p.session.State().peer_capable&&(p.restart||(p.retrySource&&now>=p.retrySource))) {p.restart=false;p.StartSource();}
    p.Incoming();
    // Retain all unsent controls. Leave a coalesced latest-generation transaction
    // in the session until the finite retry queue has room for it.
    while(!p.controls.empty()) {if(!sendControl(p.controls.front()))break;p.controls.pop_front();}
    for(auto& control:p.session.TakeControls(16-p.controls.size())) {
        auto wire=EncodeAudioControl(control);if(!wire.empty())p.controls.push_back(std::move(wire));
    }
    while(!p.controls.empty()) {if(!sendControl(p.controls.front()))break;p.controls.pop_front();}
    p.Output(now);now=GetTickCount64();
    if(p.source.State()==AudioState::Active) {
        std::vector<AudioPcmChunk> chunks;std::string error;
        if(!p.source.Poll(chunks,error))p.FailSource(error,AudioState::SourceLost);
        else for(auto& chunk:chunks) {
            p.snapshot.captured_frames+=chunk.samples.size()/2;
            if(!p.session.State().outgoing_ready){p.pending.clear();continue;}
            if(p.pending.empty())p.pendingPts=chunk.pts_100ns;
            if(chunk.discontinuity&&!p.pending.empty()){p.pending.clear();p.pendingPts=chunk.pts_100ns;++p.snapshot.dropped;}
            p.pending.insert(p.pending.end(),chunk.samples.begin(),chunk.samples.end());
            if(p.pending.size()>5760) {const auto extra=p.pending.size()-5760;p.pending.erase(p.pending.begin(),p.pending.begin()+static_cast<std::ptrdiff_t>(extra));p.pendingPts+=static_cast<std::int64_t>(extra/2)*10000000/48000;++p.snapshot.dropped;}
            while(p.pending.size()>=1920) {
                const auto age=AudioNow100ns()-p.pendingPts;
                if(age< -2500000||age>2000000) {p.pending.erase(p.pending.begin(),p.pending.begin()+1920);p.pendingPts+=200000;++p.snapshot.dropped;continue;}
                auto encoded=p.encoder.Encode({p.pending.data(),1920},error);
                if(encoded.empty()){p.FailSource(error,AudioState::Error);break;}
                AudioPacket packet;packet.session=p.session.State().session;packet.generation=p.session.State().outgoing_generation;
                packet.sequence=++p.sequence;packet.pts_100ns=p.pendingPts;packet.bytes=std::move(encoded);
                ++p.snapshot.encoded;const double latency=std::max(0.0,static_cast<double>(age)/10000.0);p.captureLatencySum+=latency;
                p.snapshot.capture_latency_mean_ms=p.captureLatencySum/static_cast<double>(p.snapshot.encoded);
                p.snapshot.capture_latency_max_ms=std::max(p.snapshot.capture_latency_max_ms,latency);
                if(sendAudio(EncodeAudio(packet)))++p.snapshot.sent;else ++p.snapshot.dropped;
                p.pending.erase(p.pending.begin(),p.pending.begin()+1920);p.pendingPts+=200000;
                if(p.snapshot.send_state!=AudioState::Active){p.snapshot.send_state=AudioState::Active;p.session.SetOutgoingState(AudioState::Active);}
            }
        }
    }
    if(p.configuredIncoming&&p.session.State().incoming_ready) {
        std::string error;
        for(unsigned n=0;n<6;++n) {
            auto frame=p.jitter.Pop(now);if(!frame)break;
            const auto packet=frame->packet?std::span<const std::uint8_t>(frame->packet->bytes):std::span<const std::uint8_t>{};
            if(frame->packet) {
                const auto age=AudioNow100ns()-frame->packet->pts_100ns;
                if(age< -2500000||age>2500000){++p.snapshot.dropped;continue;}
            }
            if(!frame->packet)++p.snapshot.plc;
            auto pcm=p.decoder.Decode(packet,error);
            if(pcm.empty()){++p.snapshot.dropped;p.receiveError=error;continue;}
            ++p.snapshot.decoded;
            if(frame->packet)p.snapshot.playout_age_ms=std::max(0.0,static_cast<double>(AudioNow100ns()-frame->packet->pts_100ns)/10000.0);
            if(p.renderer.Active()&&!p.renderer.Queue(pcm,error))p.receiveError=error;
        }
        if(p.renderer.Active()&&!p.renderer.Pump(error)){p.receiveError=error;p.retryOutput=now+2000;}
    }
    const auto render=p.renderer.Metrics();auto& snap=p.snapshot;
    snap.played_frames=render.frames;snap.underflows=render.underflows;snap.overruns=render.overruns;snap.max_render_queue_frames=render.max_queued_frames;
    snap.outgoing_generation=p.session.State().outgoing_generation;snap.incoming_generation=p.session.State().incoming_generation;
    snap.jitter_depth=p.jitter.Depth();snap.receiving=p.session.State().incoming_ready;snap.muted=p.preferences.muted;
    snap.output=p.outputEndpoint;snap.error=p.sendError;
    if(!p.receiveError.empty()){if(!snap.error.empty())snap.error+=" | ";snap.error+=p.receiveError;}
    snap.status="Send: "+std::string(snap.send_state==AudioState::Active?"Opus active":snap.send_state==AudioState::Starting?"waiting for receiver / audio":snap.send_state==AudioState::Error?"error":snap.send_state==AudioState::SourceLost?"source lost":"No Audio");
    snap.status+=" | Receive: "+std::string(snap.feedback_blocked?"same-PC feedback blocked - choose another output":p.preferences.muted?"muted":p.preferences.volume==0?"volume 0":snap.receiving?"Opus 48kHz stereo":"inactive");
    if(!snap.error.empty())snap.status+=" | "+snap.error;
}
PeerAudioSnapshot PeerAudio::Snapshot()const{return impl_->snapshot;}
}
