#include "app/peer_video_controller.h"
#include "network/peer_video_flow.h"
#include <bcrypt.h>
#include <objbase.h>
#include <mmsystem.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <fstream>
namespace fps::app {
using namespace network;
namespace {
std::uint64_t RandomNonce(){std::uint64_t value=0;if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&value),sizeof(value),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)return 0;return value?value:1;}
struct SendBudget {std::atomic<std::uint64_t> count{0},bytes{0},max_count{0},max_bytes{0};};
struct SendCredit {
    std::shared_ptr<SendBudget> budget;std::uint64_t size{};
    ~SendCredit(){budget->bytes.fetch_sub(size);budget->count.fetch_sub(1);}
};
void Peak(std::atomic<std::uint64_t>& peak,std::uint64_t value){auto old=peak.load();while(old<value&&!peak.compare_exchange_weak(old,value)){}}
}
namespace {
struct ConsumedReceipt {std::uint64_t connection{};ControlMessage message;};
struct ReceiptQueue {std::mutex mutex;std::deque<ConsumedReceipt> ready;};
struct InboundCredit {
    std::shared_ptr<void> transport;
    std::weak_ptr<ReceiptQueue> queue;
    ConsumedReceipt receipt;
    ~InboundCredit() {
        transport.reset(); // Return local capacity before the peer can send another unit.
        if(auto destination=queue.lock()) {
            std::lock_guard lock(destination->mutex);
            if(destination->ready.size()<8)destination->ready.push_back(std::move(receipt));
        }
    }
};
}
struct PeerVideoController::Impl {
    enum class CommandKind {Start,Connect,Control,Video,AudioState,AudioControl,Close,Stop};
    struct Command {
        CommandKind kind{CommandKind::Stop};std::uint64_t epoch{},connection{};
        std::uint16_t port{};std::string host;
        PublicPeerIdentity peer;std::vector<std::uint8_t> bytes;std::shared_ptr<void> credit;
        audio::AudioPreferences audio_preferences;std::uint64_t audio_session{};bool audio_share{};
    };
    enum class NoticeKind {Started,Event,Failure,Overflow,SendLost};
    struct Notice {NoticeKind kind{NoticeKind::Event};std::uint64_t epoch{};TransportEvent event;};
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    std::filesystem::path directory;
    std::unique_ptr<PeerIdentity> identity;
    std::optional<PublicPeerIdentity> peer;
    std::unique_ptr<PeerSession> session;
    media::VideoPeerSender sender;
    media::VideoPeerReceiver receiver;
    media::VideoCodecConfig codec;
    media::VideoBackendPolicy encodePolicy{media::VideoBackendPolicy::PreferHardware},decodePolicy{media::VideoBackendPolicy::PreferHardware};
    bool wantsShare{},senderRunning{},receiverRunning{},startPending{},connecting{},clearPending{},restartSender{},restartReceiver{};
    std::uint16_t requestedPort{};
    std::uint16_t remotePort{};std::string remoteHost,error,incomingError,outgoingError;
    std::uint64_t epoch{1},workerEpoch{},lastWake{};
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::deque<Command> commands;
    std::deque<Notice> notices;
    std::jthread worker;
    HANDLE wake{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    std::uint16_t actualPort{};
    TransportMetrics transportMetrics;
    audio::AudioPreferences audioPreferences,syncedAudioPreferences;
    audio::PeerAudioSnapshot audioSnapshot;
    std::uint64_t syncedAudioConnection{},syncedAudioSession{};bool syncedAudioShare{},audioSynced{};
    std::string runtime;
    std::shared_ptr<SendBudget> sendBudget{std::make_shared<SendBudget>()};
    PeerVideoSendWindow sendWindow;
    std::shared_ptr<ReceiptQueue> receipts{std::make_shared<ReceiptQueue>()};
    std::uint64_t skippedCapture{};
    EncodedObserver observer;
    unsigned traces{},lossTraces{};
    std::uint64_t lastPoll{},flowRecoveries{};
    void Trace(const std::string& text) {
        wchar_t flag[2]{};
        if(GetEnvironmentVariableW(L"FPS_CODEC_METRICS",flag,2)!=1||flag[0]!=L'1')return;
        const bool loss=text.starts_with("transport lost:")||text.starts_with("flow ")||text=="decoder requests keyframe";
        if(loss?lossTraces++>=200:traces++>=200)return;
        std::ofstream log(directory/"video-trace.log",std::ios::app);log<<GetTickCount64()<<" "<<text<<"\n";
    }
    Impl(std::unique_ptr<media::IVideoEncoder> encoder,std::unique_ptr<media::IVideoDecoder> decoder):
        sender(encoder?std::move(encoder):std::unique_ptr<media::IVideoEncoder>(std::make_unique<media::MftVideoEncoder>())),
        receiver(decoder?std::move(decoder):std::unique_ptr<media::IVideoDecoder>(std::make_unique<media::MftVideoDecoder>())){}
    ~Impl(){
        receiver.Stop();sender.Stop();sendWindow.Reset(0,0);receipts.reset();
        if(worker.joinable()){worker.request_stop();condition.notify_all();worker.join();}
        if(wake)CloseHandle(wake);
    }
    bool Queue(Command command,bool critical=false) {
        std::lock_guard lock(mutex);
        if(critical)commands.clear();
        if(commands.size()>=64){error="Peer command queue is full";return false;}
        commands.push_back(std::move(command));condition.notify_one();return true;
    }
    void NoticeOut(Notice notice) {
        std::lock_guard lock(mutex);
        if(notices.size()>=128) {
            notices.clear();Notice failure;failure.kind=NoticeKind::Overflow;failure.epoch=workerEpoch;notices.push_back(std::move(failure));
        }else notices.push_back(std::move(notice));
        if(wake)SetEvent(wake);
    }
    void Failure(const std::string& message) {
        Notice notice;notice.kind=NoticeKind::Failure;notice.epoch=workerEpoch;notice.event.detail=message;NoticeOut(std::move(notice));
    }
    void Worker(std::stop_token stop) {
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        {
        SecurePeerTransport transport;audio::PeerAudio audioEngine;bool precision=false;std::uint64_t lastAudioSnapshot=0,syncedWorkerAudioConnection=0;
        while(!stop.stop_requested()) {
            std::deque<Command> pending;std::deque<Command> videos;
            {std::lock_guard lock(mutex);pending.swap(commands);}
            for(auto& command:pending) {
                if(command.epoch<workerEpoch)continue;
                if(command.kind==CommandKind::Start) {
                    workerEpoch=command.epoch;audioEngine.Bind(0,0);std::string reason;
                    if(transport.Start(*identity,command.peer,{directory/"transport",command.port,true,250},reason)) {
                        if(!precision)precision=timeBeginPeriod(1)==TIMERR_NOERROR;
                        {std::lock_guard lock(mutex);actualPort=transport.ListenPort();runtime=transport.RuntimeInfo();}
                        Notice notice;notice.kind=NoticeKind::Started;notice.epoch=workerEpoch;NoticeOut(std::move(notice));
                    }else {{std::lock_guard lock(mutex);actualPort=0;}Failure(reason);}
                } else if(command.kind==CommandKind::Stop) {
                    workerEpoch=command.epoch;audioEngine.Bind(0,0);transport.Stop();if(precision){timeEndPeriod(1);precision=false;}
                    std::lock_guard lock(mutex);actualPort=0;transportMetrics={};
                } else if(command.epoch==workerEpoch) {
                    switch(command.kind) {
                    case CommandKind::Connect: {
                        std::string reason;if(!transport.Connect(command.host,command.port,reason))Failure(reason);break;
                    }
                    case CommandKind::Control:
                        if(transport.SendControl(command.bytes,command.connection)!=TransportSendResult::Sent)Failure("Peer control send failed");
                        break;
                    case CommandKind::Video:videos.push_back(std::move(command));break;
                    case CommandKind::AudioState:
                        audioEngine.Configure(command.audio_preferences,command.audio_share);
                        audioEngine.Bind(command.connection,command.audio_session);syncedWorkerAudioConnection=command.connection;break;
                    case CommandKind::AudioControl:audioEngine.OnControl(command.connection,command.bytes);break;
                    case CommandKind::Close:transport.Disconnect(command.connection);break;
                    default:break;
                    }
                }
            }
            auto events=transport.PollEvents();
            for(auto& event:events) {
                if(event.kind==TransportEventKind::Audio){audioEngine.OnAudio(event.connection_id,event.bytes);continue;}
                Notice notice;notice.epoch=workerEpoch;notice.event=std::move(event);NoticeOut(std::move(notice));
            }
            audioEngine.Tick([&](std::vector<std::uint8_t> bytes){return transport.SendControl(bytes,syncedWorkerAudioConnection)==TransportSendResult::Sent;},
                [&](std::vector<std::uint8_t> bytes){return transport.SendAudio(std::move(bytes),syncedWorkerAudioConnection)==TransportSendResult::Sent;});
            for(auto& command:videos)if(command.epoch==workerEpoch) {
                if(transport.SendVideo(std::move(command.bytes),command.connection,std::move(command.credit))!=TransportSendResult::Sent) {
                    Notice lost;lost.kind=NoticeKind::SendLost;lost.epoch=workerEpoch;lost.event.kind=TransportEventKind::MediaSendLost;lost.event.connection_id=command.connection;NoticeOut(std::move(lost));
                }
            }
            if(GetTickCount64()-lastAudioSnapshot>=20) {
                lastAudioSnapshot=GetTickCount64();auto snapshot=audioEngine.Snapshot();
                std::lock_guard lock(mutex);audioSnapshot=std::move(snapshot);
            }
            const auto metrics=transport.Metrics();
            {std::unique_lock lock(mutex);transportMetrics=metrics;
                if(actualPort&&GetTickCount64()-lastWake>=10){lastWake=GetTickCount64();if(wake)SetEvent(wake);}
                condition.wait_for(lock,std::chrono::milliseconds(actualPort?1:100),[&]{return stop.stop_requested()||!commands.empty();});}
        }
        audioEngine.Bind(0,0);transport.Stop();if(precision)timeEndPeriod(1);
        }
        if(SUCCEEDED(com))CoUninitialize();
    }
    void SyncAudio(bool force=false) {
        const auto connection=session&&session->State().connected?session->State().connection:0;
        const auto token=connection?session->State().token:0;
        if(!force&&audioSynced&&connection==syncedAudioConnection&&token==syncedAudioSession&&
           wantsShare==syncedAudioShare&&audioPreferences==syncedAudioPreferences)return;
        Command command;command.kind=CommandKind::AudioState;command.epoch=epoch;command.connection=connection;
        command.audio_session=token;command.audio_share=wantsShare;command.audio_preferences=audioPreferences;
        if(Queue(std::move(command))) {
            syncedAudioConnection=connection;syncedAudioSession=token;syncedAudioShare=wantsShare;syncedAudioPreferences=audioPreferences;audioSynced=true;
        }
    }
    bool QueueControl(const ControlMessage& message,std::uint64_t id) {
        auto bytes=EncodeControl(message);if(bytes.empty()){error="Invalid local control message";return false;}
        Command command;command.kind=CommandKind::Control;command.epoch=epoch;command.connection=id;command.bytes=std::move(bytes);
        return Queue(std::move(command));
    }
    bool QueueVideo(VideoWireFrame&& wire) {
        if(observer)observer(wire);
        if(wire.sequence<=3)Trace("send generation="+std::to_string(wire.generation)+" sequence="+std::to_string(wire.sequence)+" sha256="+Sha256(wire.bytes));
        const auto size=136ULL+wire.bytes.size()+wire.cursor.bgra.size();
        if(size>kMaxMediaBytes||!sendWindow.CanSubmit()||sendBudget->count.load()>=kMaxMediaInFlight)return false;
        auto credit=std::make_shared<SendCredit>();credit->budget=sendBudget;credit->size=size;
        Peak(sendBudget->max_count,sendBudget->count.fetch_add(1)+1);
        Peak(sendBudget->max_bytes,sendBudget->bytes.fetch_add(size)+size);
        auto bytes=EncodeVideo(wire);if(bytes.empty())return false;
        Command command;command.kind=CommandKind::Video;command.epoch=epoch;command.connection=session->State().connection;
        command.bytes=std::move(bytes);command.credit=credit;
        if(!Queue(std::move(command)))return false;
        return sendWindow.Submit(wire.sequence,GetTickCount64(),std::move(credit));
    }
    media::VideoPeerSender::Sink SenderSink(){return [this](VideoWireFrame&& wire){return QueueVideo(std::move(wire));};}
    void SenderFailed(){outgoingError=sender.Error();sender.Stop();senderRunning=false;wantsShare=false;if(session)session->StopShare();}
    void ReceiverFailed(){
        incomingError=receiver.Error();receiver.Stop();receiverRunning=false;clearPending=true;
        if(session&&session->State().connected) {
            const auto& state=session->State();ControlMessage message;message.type=ControlType::Error;
            message.session=state.token;message.generation=state.incoming_generation;
            message.text="Remote decoder: ";
            for(const unsigned char c:incomingError) {
                if(message.text.size()==1024)break;
                message.text.push_back(c>=32&&c<127?static_cast<char>(c):'?');
            }
            QueueControl(message,state.connection);
        }
    }
    void Actions() {
        if(!session)return;
        for(auto& action:session->TakeActions()) {
            const auto& state=session->State();
            switch(action.kind) {
            case SessionActionKind::SendControl:QueueControl(action.message,action.connection);break;
            case SessionActionKind::CloseConnection: {
                Command command;command.kind=CommandKind::Close;command.epoch=epoch;command.connection=action.connection;Queue(std::move(command));break;
            }
            case SessionActionKind::StartSender:
                sendWindow.Reset(action.message.session,action.message.generation);skippedCapture=0;
                senderRunning=sender.Initialize(device.Get(),action.message.format,codec,encodePolicy,action.message.session,action.message.generation);
                if(!senderRunning)SenderFailed();else outgoingError.clear();break;
            case SessionActionKind::StopSender:sender.Stop();senderRunning=false;sendWindow.Reset(0,0);break;
            case SessionActionKind::StartReceiver:
                if(action.message.value!=1)clearPending=true;
                receiverRunning=receiver.Configure(device.Get(),action.message.format,codec,decodePolicy,action.message.session,action.message.generation);
                if(!receiverRunning)ReceiverFailed();else {incomingError.clear();session->ReceiverReady(action.message.generation);}break;
            case SessionActionKind::StopReceiver:receiver.Stop();receiverRunning=false;if(action.message.value!=1)clearPending=true;break;
            case SessionActionKind::ForceKeyframe:if(senderRunning&&!sender.RequestKeyframe())SenderFailed();break;
            case SessionActionKind::RecoverReceiver:
                if(receiverRunning&&!receiver.Recover())ReceiverFailed();break;
            case SessionActionKind::Reconnect: {
                if(remoteHost.empty()||!remotePort)break;
                Command command;command.kind=CommandKind::Connect;command.epoch=epoch;command.host=remoteHost;command.port=remotePort;
                connecting=Queue(std::move(command));break;
            }
            case SessionActionKind::Error:
                if(action.message.type==ControlType::Error) {
                    if(action.message.generation==state.outgoing_generation)outgoingError=action.detail;
                }else error=action.detail;
                break;
            }
            (void)state;
        }
    }
};

PeerVideoController::PeerVideoController(std::unique_ptr<media::IVideoEncoder> encoder,std::unique_ptr<media::IVideoDecoder> decoder):
    impl_(std::make_unique<Impl>(std::move(encoder),std::move(decoder))){}
PeerVideoController::~PeerVideoController()=default;
bool PeerVideoController::Initialize(ID3D11Device* device,const std::filesystem::path& directory,std::string& error) {
    auto& p=*impl_;if(p.worker.joinable()){error="Peer controller already initialized";return false;}
    if(!p.wake){error="Peer wake event creation failed";return false;}
    p.device=device;p.directory=directory;p.identity=PeerIdentity::LoadOrCreate(directory/"identity",error);
    if(!p.identity)return false;
    std::error_code ec;if(std::filesystem::exists(directory/"trusted.friendpair",ec)) {
        p.peer=ReadPublicBundle(directory/"trusted.friendpair",error);
        if(p.peer&&!ValidateTrustedPeer(p.identity->Public(),*p.peer,error))p.peer.reset();
        if(!p.peer)p.error="Stored peer trust is unusable: "+error;
    }
    if(p.peer)p.session=std::make_unique<PeerSession>(p.identity->Public().fingerprint,p.peer->fingerprint,RandomNonce());
    p.worker=std::jthread([&p](std::stop_token stop){p.Worker(stop);});return true;
}
const PublicPeerIdentity* PeerVideoController::Identity()const{return impl_->identity?&impl_->identity->Public():nullptr;}
const PublicPeerIdentity* PeerVideoController::TrustedPeer()const{return impl_->peer?&*impl_->peer:nullptr;}
bool PeerVideoController::ExportIdentity(const std::filesystem::path& path,std::string& error) {
    if(!impl_->identity){error="Local identity is unavailable";return false;}return WritePublicBundle(path,impl_->identity->Public(),error);
}
bool PeerVideoController::AuthorizePeer(const PublicPeerIdentity& peer,std::string& error) {
    auto& p=*impl_;if(!p.identity||!ValidateTrustedPeer(p.identity->Public(),peer,error))return false;
    if(!SaveTrustedPeer(p.directory/"trusted.friendpair",peer,error))return false;
    Disconnect();p.peer=peer;p.session=std::make_unique<PeerSession>(p.identity->Public().fingerprint,peer.fingerprint,RandomNonce());
    p.error.clear();return true;
}
void PeerVideoController::ConfigureCodecs(media::VideoCodecConfig config,media::VideoBackendPolicy encoder,media::VideoBackendPolicy decoder) {
    auto& p=*impl_;
    p.restartSender=p.senderRunning&&(encoder!=p.encodePolicy||config.encoder_id!=p.codec.encoder_id||
        config.target_bitrate_bps!=p.codec.target_bitrate_bps||config.keyframe_interval_frames!=p.codec.keyframe_interval_frames||
        config.low_latency!=p.codec.low_latency);
    p.restartReceiver=p.session&&p.session->State().receiving&&(decoder!=p.decodePolicy||config.decoder_id!=p.codec.decoder_id);
    p.codec=std::move(config);p.encodePolicy=encoder;p.decodePolicy=decoder;
}
void PeerVideoController::ConfigureAudio(audio::AudioPreferences preferences) {auto& p=*impl_;p.audioPreferences=std::move(preferences);p.SyncAudio(true);}
bool PeerVideoController::Listen(std::uint16_t port,std::string& error) {
    auto& p=*impl_;if(!p.identity||!p.peer||!p.session){error="Export your identity and pair with your friend before connecting";return false;}
    std::uint16_t actual=0;{std::lock_guard lock(p.mutex);actual=p.actualPort;}
    if(actual||p.startPending) {
        if(!port||port==actual||port==p.requestedPort)return true;
        error="Disconnect before changing the active Share Port";return false;
    }
    p.requestedPort=port;p.startPending=true;p.error.clear();++p.epoch;p.sendBudget=std::make_shared<SendBudget>();
    Impl::Command command;command.kind=Impl::CommandKind::Start;command.epoch=p.epoch;command.port=port;command.peer=*p.peer;
    if(!p.Queue(std::move(command))){p.startPending=false;error=p.error;return false;}return true;
}
bool PeerVideoController::Connect(std::string host,std::uint16_t remotePort,std::uint16_t ownPort,std::string& error) {
    if(host.empty()||host.size()>253||!remotePort){error="Enter a valid peer host and Remote Port";return false;}
    if(!Listen(ownPort,error))return false;
    auto& p=*impl_;p.remoteHost=host;p.remotePort=remotePort;p.connecting=true;p.session->SetConnectionIntent(true,GetTickCount64());
    Impl::Command command;command.kind=Impl::CommandKind::Connect;command.epoch=p.epoch;command.host=std::move(host);command.port=remotePort;
    if(!p.Queue(std::move(command))){p.connecting=false;error=p.error;return false;}return true;
}
bool PeerVideoController::StartShare(std::string& error) {
    auto& p=*impl_;if(!p.peer||!p.session){error="Pair with your friend before sharing";return false;}
    p.wantsShare=true;p.error.clear();p.SyncAudio();return true;
}
void PeerVideoController::StopShare() {
    auto& p=*impl_;p.wantsShare=false;if(p.session){p.session->StopShare();p.Actions();}
    p.sender.Stop();p.senderRunning=false;p.sendWindow.Reset(0,0);p.SyncAudio();
}
bool PeerVideoController::PushCapture(const capture::CapturedFrame& frame) {
    auto& p=*impl_;if(!p.wantsShare||!p.session)return true;
    if(!frame.IsValid()||frame.width>4096||frame.height>4096||static_cast<std::uint64_t>(frame.width)*frame.height>3840ULL*2160) {
        p.error="Capture exceeds peer video limits (4096 edge, 3840 x 2160 visible pixels)";StopShare();return false;
    }
    VideoFormat format{(frame.width+1)&~1U,(frame.height+1)&~1U,frame.width,frame.height,
        p.codec.fps_numerator,p.codec.fps_denominator,p.codec.target_bitrate_bps,p.codec.keyframe_interval_frames,p.codec.low_latency};
    if(p.restartSender){p.session->StopShare();p.restartSender=false;}
    if(!p.session->RequestShare(format)){p.error="Unsupported capture format";StopShare();return false;}
    p.Actions();
    if(p.senderRunning&&(!p.sendWindow.CanSubmit()||p.sendBudget->count.load()>=kMaxMediaInFlight)){++p.skippedCapture;return true;}
    if(p.senderRunning&&!p.sender.Push(frame,p.SenderSink())){p.SenderFailed();p.Actions();return false;}
    return true;
}
bool PeerVideoController::Poll(const FrameSink& sink,const ClearSink& clear) {
    auto& p=*impl_;if(!p.session)return false;
    const auto before=p.receiver.Metrics().delivered;
    const auto polled=GetTickCount64();
    if(p.lastPoll&&polled-p.lastPoll>100)p.Trace("flow UI gap ms="+std::to_string(polled-p.lastPoll));
    p.lastPoll=polled;
    const auto delivered=[&](const capture::CapturedFrame& frame) {
        if(!sink(frame))return false;
        p.session->ReceiverRecovered(p.session->State().incoming_generation);return true;
    };
    if(p.clearPending){p.clearPending=false;clear();}
    std::deque<Impl::Notice> pending;{std::lock_guard lock(p.mutex);pending.swap(p.notices);}
    for(auto& notice:pending) {
        if(notice.epoch!=p.epoch)continue;
        if(notice.kind==Impl::NoticeKind::Started){p.startPending=false;p.error.clear();}
        else if(notice.kind==Impl::NoticeKind::SendLost) {
            if(p.senderRunning&&!p.sender.RequestKeyframe())p.SenderFailed();
        }
        else if(notice.kind==Impl::NoticeKind::Failure) {
            p.error=notice.event.detail;p.startPending=false;p.connecting=false;
            p.session->OnDisconnected(0,GetTickCount64());
        }else if(notice.kind==Impl::NoticeKind::Overflow) {
            Disconnect();p.error="Peer event queue overflow; connection stopped";break;
        }else {
            auto& event=notice.event;const auto now=GetTickCount64();
            switch(event.kind) {
            case TransportEventKind::Connected:
                p.session->OnConnected(event.connection_id,event.detail=="client",now,event.detail=="client"?RandomNonce():0);break;
            case TransportEventKind::Disconnected:
                p.session->OnDisconnected(event.connection_id,now);p.connecting=false;break;
            case TransportEventKind::Control: {
                if(audio::IsAudioControl(event.bytes)) {
                    if(p.session->State().connected&&event.connection_id==p.session->State().connection) {
                        p.SyncAudio();Impl::Command command;command.kind=Impl::CommandKind::AudioControl;
                        command.epoch=p.epoch;command.connection=event.connection_id;command.bytes=std::move(event.bytes);p.Queue(std::move(command));
                    }break;
                }
                std::string error;const auto message=DecodeControl(event.bytes,error);
                if(message) {
                    p.session->OnControl(event.connection_id,*message,now);
                    if(event.connection_id==p.session->State().connection&&message->type==ControlType::VideoConsumed)
                        p.sendWindow.Acknowledge(*message);
                }
                else {p.error=error;Impl::Command command;command.kind=Impl::CommandKind::Close;command.epoch=p.epoch;command.connection=event.connection_id;p.Queue(std::move(command));}
                break;
            }
            case TransportEventKind::Video: {
                std::string error;auto wire=DecodeVideo(event.bytes,error);
                if(!wire){p.error=error;p.session->OnMediaLost(event.connection_id);break;}
                p.Trace("wire generation="+std::to_string(wire->generation)+" sequence="+std::to_string(wire->sequence)+" key="+std::to_string(wire->keyframe)+" params="+std::to_string(HasRandomAccessParameterSets(wire->bytes))+(wire->sequence<=3?" sha256="+Sha256(wire->bytes):""));
                auto credit=std::make_shared<InboundCredit>();credit->transport=std::move(event.receive_credit);credit->queue=p.receipts;
                credit->receipt.connection=event.connection_id;credit->receipt.message.type=ControlType::VideoConsumed;
                credit->receipt.message.session=wire->session;credit->receipt.message.generation=wire->generation;credit->receipt.message.value=wire->sequence;
                wire->receive_credit=std::move(credit);
                if(p.session->AcceptVideo(event.connection_id,*wire,now)&&p.receiverRunning) {
                    if(p.clearPending){p.clearPending=false;clear();}
                    if(!p.receiver.Push(std::move(*wire),delivered))p.ReceiverFailed();
                    p.Trace("decode total="+std::to_string(p.receiver.Metrics().delivered)+" dropped="+std::to_string(p.receiver.Metrics().dropped)+" wait="+std::to_string(p.receiver.AwaitingKeyframe()));
                    if(p.receiver.TakeKeyframeRequest()){p.Trace("decoder requests keyframe");p.session->OnMediaLost(event.connection_id);}
                }break;
            }
            case TransportEventKind::Audio:break; // consumed on the network/audio worker
            case TransportEventKind::MediaSendLost:
                if(p.senderRunning&&!p.sender.RequestKeyframe())p.SenderFailed();break;
            case TransportEventKind::MediaLost:p.Trace("transport lost: "+event.detail);p.session->OnMediaLost(event.connection_id);break;
            case TransportEventKind::Error:p.error=event.detail;break;
            }
        }
        p.Actions();
    }
    if(p.restartReceiver) {
        p.restartReceiver=false;const auto state=p.session->State();
        if(state.receiving) {
            const bool wasRunning=p.receiverRunning;p.clearPending=true;
            p.receiverRunning=p.receiver.Configure(p.device.Get(),state.incoming_format,p.codec,p.decodePolicy,state.token,state.incoming_generation);
            if(!p.receiverRunning)p.ReceiverFailed();
            else {
                p.incomingError.clear();
                if(wasRunning)p.session->OnMediaLost(state.connection);
                else p.session->ReceiverReady(state.incoming_generation);
            }
        }
    }
    p.session->Tick(GetTickCount64());p.Actions();
    if(p.clearPending){p.clearPending=false;clear();}
    std::deque<ConsumedReceipt> consumed;
    {std::lock_guard lock(p.receipts->mutex);consumed.swap(p.receipts->ready);}
    for(const auto& receipt:consumed)
        if(receipt.connection==p.session->State().connection&&receipt.message.session==p.session->State().token)
            p.QueueControl(receipt.message,receipt.connection);
    if(p.senderRunning&&p.sendWindow.Expire(GetTickCount64())) {
        ++p.flowRecoveries;p.Trace("flow consumer timeout pending="+std::to_string(p.sendWindow.Pending())+
            " oldest_ms="+std::to_string(p.sendWindow.OldestAge(GetTickCount64())));
        p.session->RestartOutgoing();p.Actions();
    }
    if(p.senderRunning&&p.sendWindow.CanSubmit()&&!p.sender.Poll(p.SenderSink())){p.SenderFailed();p.Actions();}
    if(p.receiverRunning&&!p.receiver.Poll(delivered))p.ReceiverFailed();
    if(p.session->State().connected)p.connecting=false;
    p.SyncAudio();return p.receiver.Metrics().delivered!=before;
}
void PeerVideoController::Disconnect() {
    auto& p=*impl_;p.wantsShare=p.startPending=p.connecting=false;p.incomingError.clear();p.outgoingError.clear();
    if(p.session){p.session->Disconnect();p.session->TakeActions();}
    p.sender.Stop();p.receiver.Stop();p.sendWindow.Reset(0,0);p.receipts=std::make_shared<ReceiptQueue>();p.senderRunning=p.receiverRunning=false;p.clearPending=true;
    ++p.epoch;Impl::Command command;command.kind=Impl::CommandKind::Stop;command.epoch=p.epoch;p.Queue(std::move(command),true);
    {std::lock_guard lock(p.mutex);p.notices.clear();p.actualPort=0;p.transportMetrics={};}
    p.sendBudget=std::make_shared<SendBudget>();
}
PeerVideoSnapshot PeerVideoController::Snapshot()const {
    const auto& p=*impl_;PeerVideoSnapshot result;
    {std::lock_guard lock(p.mutex);result.listen_port=p.actualPort;result.transport=p.transportMetrics;result.runtime=p.runtime;result.audio=p.audioSnapshot;}
    result.active=result.listen_port||p.startPending;result.connecting=p.connecting;result.wants_share=p.wantsShare;
    if(p.identity)result.own_fingerprint=p.identity->Public().fingerprint;if(p.peer)result.peer_fingerprint=p.peer->fingerprint;
    if(p.session) {
        const auto& state=p.session->State();result.connected=state.connected;
        result.connection=state.connection;result.session=state.token;result.outgoing_generation=state.outgoing_generation;result.incoming_generation=state.incoming_generation;
        result.sharing=state.sharing&&p.senderRunning;result.receiving=(state.receiving&&p.receiverRunning)||state.recovering;
    }
    result.awaiting_keyframe=result.receiving&&(p.session->State().recovering||p.receiver.AwaitingKeyframe());result.error=p.error;
    for(const auto* codecError:{&p.incomingError,&p.outgoingError})
        if(!codecError->empty()){if(!result.error.empty())result.error+=" | ";result.error+=*codecError;}
    if(!p.peer)result.status="Not paired - export identity and pair with your friend";
    else if(result.connected)result.status="Authenticated peer connected | "+std::string(result.receiving?(result.awaiting_keyframe?"recovering / waiting for video IDR":"receiving H.264"):"no incoming video");
    else if(result.connecting)result.status="Connecting to "+p.remoteHost+":"+std::to_string(p.remotePort);
    else if(result.listen_port)result.status="Listening on UDP "+std::to_string(result.listen_port)+" | waiting for paired friend";
    else if(p.startPending)result.status="Starting secure peer listener";
    else result.status="Paired | disconnected";
    if(!result.error.empty())result.status+=" | "+result.error;
    result.encoder=p.sender.Info().candidate.name;result.decoder=p.receiver.Info().candidate.name;
    result.encoder_gpu_frames=p.sender.Info().gpu_frames;result.encoder_cpu_frames=p.sender.Info().cpu_frames;
    result.decoder_gpu_frames=p.receiver.Info().gpu_frames;result.decoder_cpu_frames=p.receiver.Info().cpu_frames;
    result.outgoing=p.sender.Metrics();result.incoming=p.receiver.Metrics();
    result.encode_latency=p.sender.Latency();result.decode_latency=p.receiver.DecodeLatency();result.end_to_end=p.receiver.EndToEndLatency();
    result.outbound_pending=p.sendBudget->count.load();result.outbound_bytes=p.sendBudget->bytes.load();
    result.flow_recoveries=p.flowRecoveries;result.skipped_capture=p.skippedCapture;result.oldest_outbound_ms=p.sendWindow.OldestAge(GetTickCount64());
    result.max_outbound_pending=p.sendBudget->max_count.load();result.max_outbound_bytes=p.sendBudget->max_bytes.load();return result;
}
HANDLE PeerVideoController::WakeEvent()const{return impl_->wake;}
void PeerVideoController::SetEncodedObserver(EncodedObserver observer){impl_->observer=std::move(observer);}
bool PeerVideoController::WantsShare()const{return impl_->wantsShare;}
bool PeerVideoController::IsReceiving()const{return impl_->session&&((impl_->receiverRunning&&impl_->session->State().receiving)||impl_->session->State().recovering);}
bool PeerVideoController::IsActive()const {
    const auto& p=*impl_;std::lock_guard lock(p.mutex);return p.startPending||p.actualPort!=0;
}
}
