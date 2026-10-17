#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "network/secure_peer_transport.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <thread>
#include "tls_negative.h"
using namespace fps::network;
namespace {
bool Check(bool condition,const char* label) {
    std::cout<<(condition?"PASS ":"FAIL ")<<label<<'\n';return condition;
}
bool SelfTest(const std::filesystem::path& root) {
    std::string error;auto a=PeerIdentity::LoadOrCreate(root/"a",error),b=PeerIdentity::LoadOrCreate(root/"b",error);
    if(!Check(a&&b,"independent native identities")){std::cerr<<error<<'\n';return false;}
    SecurePeerTransport sender,receiver;
    if(!Check(sender.Start(*a,b->Public(),{root/"a",0,true,250},error),"A secure listener")){std::cerr<<error<<'\n';return false;}
    if(!Check(receiver.Start(*b,a->Public(),{root/"b",0,true,250},error),"B secure listener")){std::cerr<<error<<'\n';return false;}
    std::cout<<sender.RuntimeInfo()<<"\nA_PORT="<<sender.ListenPort()<<"\nB_PORT="<<receiver.ListenPort()<<'\n';
    if(!Check(sender.ListenPort()!=receiver.ListenPort(),"distinct UDP ports"))return false;
    if(!Check(sender.Connect("127.0.0.1",receiver.ListenPort(),error),"connect start")){std::cerr<<error<<'\n';return false;}
    bool aConnected=false,bConnected=false,control=false,video=false,reply=false;
    std::vector<std::uint8_t> payload(32768);for(std::size_t i=0;i<payload.size();++i)payload[i]=static_cast<std::uint8_t>(i*17U);
    const std::vector<std::uint8_t> hello{'h','e','l','l','o'};
    bool sent=false,replied=false;
    auto until=GetTickCount64()+6000;
    while(GetTickCount64()<until) {
        for(auto& e:sender.PollEvents()){
            if(e.kind==TransportEventKind::Connected)aConnected=true;
            if(e.kind==TransportEventKind::Control)reply=(e.bytes==hello);
            if(e.kind==TransportEventKind::Error)std::cerr<<"A "<<e.detail<<'\n';
        }
        for(auto& e:receiver.PollEvents()){
            if(e.kind==TransportEventKind::Connected)bConnected=true;
            if(e.kind==TransportEventKind::Control)control=(e.bytes==hello);
            if(e.kind==TransportEventKind::Video)video=(e.bytes==payload);
            if(e.kind==TransportEventKind::Error)std::cerr<<"B "<<e.detail<<'\n';
        }
        if(aConnected&&bConnected&&!sent){
            sent=sender.SendControl(hello)==TransportSendResult::Sent;
            sent=sender.SendVideo(payload)==TransportSendResult::Sent&&sent;
        }
        if(control&&!replied)replied=receiver.SendControl(hello)==TransportSendResult::Sent;
        if(control&&video&&reply)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    bool ok=true;
    ok=Check(aConnected&&bConnected,"mutual authenticated connection")&&ok;
    ok=Check(control&&reply,"ordered control both directions")&&ok;
    ok=Check(video,"actual encrypted media payload integrity")&&ok;
    const auto metrics=receiver.Metrics();
    ok=Check(metrics.received_video==1 && metrics.received_bytes>=payload.size(),"native receive counters")&&ok;
    sender.Stop();receiver.Stop();
    return Check(ok,"secure native self-test")&&ok;
}
bool WaitConnected(SecurePeerTransport& a,SecurePeerTransport& b,std::uint32_t timeout=5000) {
    bool ac=false,bc=false;const auto until=GetTickCount64()+timeout;
    while(GetTickCount64()<until) {
        for(auto& e:a.PollEvents())if(e.kind==TransportEventKind::Connected)ac=true;
        for(auto& e:b.PollEvents())if(e.kind==TransportEventKind::Connected)bc=true;
        if(ac&&bc)return true;Sleep(2);
    }return false;
}
bool NegativeTests(const std::filesystem::path& root) {
    std::string error;auto a=PeerIdentity::LoadOrCreate(root/"a",error),b=PeerIdentity::LoadOrCreate(root/"b",error),
        stranger=PeerIdentity::LoadOrCreate(root/"stranger",error);
    if(!Check(a&&b&&stranger,"security test identities"))return false;
    bool ok=true;
    {
        SecurePeerTransport wrongServer,client;
        ok=Check(wrongServer.Start(*b,stranger->Public(),{root/"b",0,true,250},error)&&
            client.Start(*a,b->Public(),{root/"a",0,true,250},error),"unknown client test listeners")&&ok;
        if(!ok)return false;
        ok=Check(client.Connect("127.0.0.1",wrongServer.ListenPort(),error),"unknown client connect attempted")&&ok;
        bool accepted=false,disconnected=false;const auto until=GetTickCount64()+5000;
        while(GetTickCount64()<until&&!disconnected) {
            for(auto& e:wrongServer.PollEvents()) {
                accepted|=e.kind==TransportEventKind::Connected||e.kind==TransportEventKind::Video||e.kind==TransportEventKind::Control;
                disconnected|=e.kind==TransportEventKind::Disconnected;
            }
            client.PollEvents();Sleep(2);
        }
        ok=Check(!accepted&&disconnected,"unknown client rejected before payload delivery")&&ok;
    }
    for(bool missing_client:{true,false}) {
        SecurePeerTransport server;
        if(!server.Start(*b,a->Public(),{root/"b",0,true,250},error))return Check(false,"TLS rejection fixture server");
        const auto expected=missing_client?b->Public().stable_name:std::string("fps-00000000000000000000000000000000.invalid");
        const auto result=RunNativeTlsNegative(server,b->Public(),missing_client?nullptr:a.get(),expected,root/"negative-server-ca.pem");
        ok=Check(result.setup,"native TLS negative connection actually attempted")&&ok;
        ok=Check(result.finished&&!result.server_accepted,missing_client?"missing client credential rejected before payload":"normal TLS SAN mismatch rejected before payload")&&ok;
        std::cout<<"TLS_REJECTION_STATUS="<<static_cast<unsigned long>(result.status)<<'\n';
    }
    {
        SecurePeerTransport first,duplicate;
        ok=Check(first.Start(*a,b->Public(),{root/"a",0,true,250},error),"port conflict listener")&&ok;
        ok=Check(!duplicate.Start(*b,a->Public(),{root/"b",first.ListenPort(),true,250},error),"occupied UDP port rejected")&&ok;
        ok=Check(!first.Connect("127.0.0.1",first.ListenPort(),error),"self endpoint rejected")&&ok;
        ok=Check(!first.Connect("192.0.2.1",12345,error),"loopback qualification does not expose non-loopback")&&ok;
        const auto lookup_started=GetTickCount64();
        ok=Check(!first.Connect("not-resolved.invalid",12345,error)&&error.find("IPv4")!=std::string::npos,
            "evaluation rejects DNS names explicitly without an unbounded resolver")&&ok;
        ok=Check(GetTickCount64()-lookup_started<500,"endpoint validation is promptly bounded")&&ok;
    }
    {
        SecurePeerTransport sender,receiver;
        if(!sender.Start(*a,b->Public(),{root/"a",0,true,250},error)||
           !receiver.Start(*b,a->Public(),{root/"b",0,true,250},error)||
           !sender.Connect("127.0.0.1",receiver.ListenPort(),error)||!WaitConnected(sender,receiver))
            return Check(false,"bounded queue connection");
        ok=Check(sender.SendControl(std::vector<std::uint8_t>(kMaxControlBytes+1))==TransportSendResult::Invalid&&
            sender.SendVideo(std::vector<std::uint8_t>(kMaxMediaBytes+1))==TransportSendResult::Invalid,
            "oversized outgoing messages rejected")&&ok;
        // No PollEvents on the receiver: completed and partial media must share the same two-slot budget.
        for(unsigned i=0;i<20;++i){sender.SendVideo(std::vector<std::uint8_t>(32768,static_cast<std::uint8_t>(i)));sender.PollEvents();Sleep(10);}
        Sleep(50);
        const auto m=receiver.Metrics(),s=sender.Metrics();
        ok=Check(m.receive_media_count<=2&&m.max_receive_media_count<=2&&m.max_receive_media_bytes<=2*kMaxMediaBytes&&
            s.max_send_media_count<=2&&s.max_send_media_bytes<=2*kMaxMediaBytes,"all partial/in-flight queues bounded")&&ok;
        ok=Check(m.dropped_video>0&&m.receive_media_count==2,"stalled receiver drops excess media")&&ok;
        std::cout<<"AudioDatagramBudgetIndependent\n";
        for(unsigned i=0;i<20;++i){sender.SendAudio(std::vector<std::uint8_t>(512,static_cast<std::uint8_t>(i)));sender.PollEvents();Sleep(3);}
        Sleep(30);
        const auto am=receiver.Metrics(),asmtr=sender.Metrics();
        ok=Check(am.receive_audio_count==6&&am.max_receive_audio_count==6&&am.receive_media_count==2&&
            asmtr.max_send_audio_count<=6,"independent six-datagram audio budget with full video credit")&&ok;
        auto audioHeld=receiver.PollEvents();
        std::vector<TransportEvent> keepVideo;
        for(auto it=audioHeld.begin();it!=audioHeld.end();)if(it->kind==TransportEventKind::Video){keepVideo.push_back(std::move(*it));it=audioHeld.erase(it);}else ++it;
        ok=Check(receiver.Metrics().receive_audio_count==6,"consumer-held audio remains accounted")&&ok;
        audioHeld.clear();
        ok=Check(receiver.Metrics().receive_audio_count==0,"audio credits returned after consumer release")&&ok;
        auto held=std::move(keepVideo);
        ok=Check(receiver.Metrics().receive_media_count==2,"consumer-held media remains in receive budget")&&ok;
        held.clear();sender.PollEvents();
        bool recovered=false;
        for(unsigned i=0;i<100&&!recovered;++i) {
            sender.SendVideo({0x12,0x34,0x56});sender.PollEvents();Sleep(5);
            for(auto& e:receiver.PollEvents())if(e.kind==TransportEventKind::Video&&e.bytes==std::vector<std::uint8_t>{0x12,0x34,0x56})recovered=true;
        }
        ok=Check(recovered,"transport resumes after bounded receive stall")&&ok;
        for(unsigned round=0;round<3;++round) {
            sender.Disconnect();const auto until=GetTickCount64()+2000;
            while(GetTickCount64()<until){sender.PollEvents();receiver.PollEvents();Sleep(2);}
            if(!sender.Connect("127.0.0.1",receiver.ListenPort(),error)||!WaitConnected(sender,receiver)){ok=Check(false,"repeated disconnect/reconnect")&&ok;break;}
        }
        ok=Check(ok,"repeated disconnect/reconnect without stale handles")&&ok;
    }
    return ok;
}
bool DnsRouteTests(const std::filesystem::path& root) {
    std::string error;auto a=PeerIdentity::LoadOrCreate(root/"dns-a",error),b=PeerIdentity::LoadOrCreate(root/"dns-b",error);
    if(!a||!b)return Check(false,"DNS route identities");
    SecurePeerTransport client,server;
    if(!client.Start(*a,b->Public(),{root/"dns-a",0,false,250},error)||
       !server.Start(*b,a->Public(),{root/"dns-b",0,false,250},error))return Check(false,"unrestricted pinned route listeners");
    auto start=GetTickCount64();
    bool ok=Check(client.Connect("localhost",server.ListenPort(),error),"DNS connection is accepted asynchronously");
    ok=Check(GetTickCount64()-start<1000,"DNS initiation does not block the caller")&&ok;
    client.Disconnect();
    bool stale=false;auto until=GetTickCount64()+100;
    while(GetTickCount64()<until){
        for(auto& event:client.PollEvents())stale|=event.kind==TransportEventKind::Connected;
        server.PollEvents();Sleep(1);
    }
    ok=Check(!stale,"Disconnect invalidates completed or pending DNS")&&ok;
    ok=Check(client.Connect("localhost",server.ListenPort(),error)&&WaitConnected(client,server),
        "native DNS route retains mutual certificate and stable SAN validation")&&ok;
    const std::vector<std::uint8_t> payload{'d','n','s','-','p','i','n','n','e','d'};
    ok=Check(client.SendControl(payload)==TransportSendResult::Sent,"DNS route control send")&&ok;
    bool delivered=false;until=GetTickCount64()+1000;
    while(GetTickCount64()<until&&!delivered){
        client.PollEvents();for(auto& event:server.PollEvents())if(event.kind==TransportEventKind::Control&&event.bytes==payload)delivered=true;
        Sleep(1);
    }
    ok=Check(delivered,"DNS route delivers authenticated payload")&&ok;
    client.Stop();
    for(unsigned round=0;round<4;++round){
        SecurePeerTransport pending;
        if(!pending.Start(*a,b->Public(),{root/"dns-a",0,false,250},error))return Check(false,"DNS stop fixture");
        start=GetTickCount64();pending.Connect("localhost",server.ListenPort(),error);pending.Stop();
        ok=Check(GetTickCount64()-start<2000,"DNS context survives prompt stop without waiting for provider")&&ok;
    }
    return ok;
}
bool RouteProbe(const std::filesystem::path& root,const std::string& host,std::uint16_t localPort,std::uint16_t peerPort) {
    std::string error;auto a=PeerIdentity::LoadOrCreate(root/"a",error),b=PeerIdentity::LoadOrCreate(root/"b",error);
    SecurePeerTransport client,server;
    if(!a||!b||!client.Start(*a,b->Public(),{root/"a",localPort,false,250},error)||
       !server.Start(*b,a->Public(),{root/"b",peerPort,false,250},error)){
        std::cerr<<"route startup: "<<error<<"\n";return false;
    }
    std::cout<<"route host="<<host<<" Aport="<<client.ListenPort()<<" Bport="<<server.ListenPort()<<"\n";
    if(!client.Connect(host,server.ListenPort(),error)){std::cerr<<error<<"\n";return false;}
    bool ca=false,cb=false,delivered=false,sent=false;const auto until=GetTickCount64()+7000;
    const std::vector<std::uint8_t> bytes{'r','o','u','t','e'};
    while(GetTickCount64()<until&&!delivered){
        for(auto& event:client.PollEvents()){
            if(event.kind==TransportEventKind::Connected){ca=true;std::cout<<"route A authenticated\n";}
            if(event.kind==TransportEventKind::Error)std::cout<<"route A error "<<event.detail<<"\n";
        }
        for(auto& event:server.PollEvents()){
            if(event.kind==TransportEventKind::Connected){cb=true;std::cout<<"route B authenticated\n";}
            if(event.kind==TransportEventKind::Error)std::cout<<"route B error "<<event.detail<<"\n";
            if(event.kind==TransportEventKind::Control&&event.bytes==bytes)delivered=true;
        }
        if(ca&&cb&&!sent){client.SendControl(bytes);sent=true;}Sleep(1);
    }
    std::cout<<"route rejected A="<<client.Metrics().rejected_certificates<<" B="<<server.Metrics().rejected_certificates<<"\n";
    return Check(ca&&cb&&delivered,"route mutual authentication and payload");
}
bool Prepare(const std::filesystem::path& root) {
    std::string error;auto a=PeerIdentity::LoadOrCreate(root/"a",error),b=PeerIdentity::LoadOrCreate(root/"b",error);
    return a&&b&&WritePublicBundle(root/"a.friendpair",a->Public(),error)&&WritePublicBundle(root/"b.friendpair",b->Public(),error);
}
bool ProcessProbe(const std::filesystem::path& data,const std::filesystem::path& trust,std::uint16_t localPort,
    std::uint16_t remotePort,std::uint32_t duration,const std::filesystem::path& report) {
    std::string error;auto identity=PeerIdentity::LoadOrCreate(data,error);auto peer=ReadPublicBundle(trust,error);
    SecurePeerTransport transport;
    if(!identity||!peer||!transport.Start(*identity,*peer,{data,localPort,true,250},error)){
        std::cerr<<error<<'\n';return false;
    }
    std::cout<<"READY pid="<<GetCurrentProcessId()<<" port="<<transport.ListenPort()<<'\n'<<transport.RuntimeInfo()<<'\n';
    if(remotePort&&!transport.Connect("127.0.0.1",remotePort,error)){std::cerr<<error<<'\n';return false;}
    const std::vector<std::uint8_t> control{'p','i','n','g'};
    std::vector<std::uint8_t> payload(65536);for(std::size_t i=0;i<payload.size();++i)payload[i]=static_cast<std::uint8_t>(i*29U+7U);
    bool connected=false,integrity=true;std::uint64_t controls=0,videos=0,nextSend=0;
    const auto until=GetTickCount64()+duration;
    while(GetTickCount64()<until) {
        for(auto& e:transport.PollEvents()) {
            if(e.kind==TransportEventKind::Connected)connected=true;
            if(e.kind==TransportEventKind::Disconnected)connected=false;
            if(e.kind==TransportEventKind::Control){++controls;integrity&=e.bytes==control;if(!remotePort)transport.SendControl(e.bytes);}
            if(e.kind==TransportEventKind::Video){++videos;integrity&=e.bytes==payload;if(!remotePort)transport.SendVideo(std::move(e.bytes));}
            if(e.kind==TransportEventKind::Error)std::cerr<<e.detail<<'\n';
        }
        if(remotePort&&connected&&GetTickCount64()>=nextSend) {
            transport.SendControl(control);transport.SendVideo(payload);nextSend=GetTickCount64()+16;
        }
        Sleep(1);
    }
    const auto m=transport.Metrics();const bool ok=controls>=10&&videos>=10&&integrity;
    std::ofstream file(report);
    file<<"result="<<(ok?"PASS":"FAIL")<<"\npid="<<GetCurrentProcessId()<<"\nport="<<localPort
        <<"\npeer="<<peer->fingerprint<<"\ncontrols="<<controls<<"\nvideos="<<videos<<"\nsha256="<<Sha256(payload)
        <<"\nsend_peak_count="<<m.max_send_media_count<<"\nreceive_peak_count="<<m.max_receive_media_count
        <<"\nsend_peak_bytes="<<m.max_send_media_bytes<<"\nreceive_peak_bytes="<<m.max_receive_media_bytes
        <<"\nreceived_bytes="<<m.received_bytes<<"\nsent_bytes="<<m.sent_bytes<<"\ndropped="<<m.dropped_video<<"\n";
    transport.Stop();return Check(ok,"independent process encrypted exchange");
}

}
int wmain(int argc,wchar_t** argv) {
    std::cout << std::unitbuf; std::cerr << std::unitbuf;
    std::filesystem::path root=std::filesystem::current_path()/("peer-probe-test-"+std::to_string(GetCurrentProcessId()));
    if(argc>2 && std::wstring_view(argv[1])==L"--self-test")root=argv[2];
    if(argc>3&&std::wstring_view(argv[1])==L"--route-self-test"){
        const std::wstring wide(argv[3]);std::string host;
        for(wchar_t ch:wide){if(ch<=0||ch>=127){std::cerr<<"Use an ASCII/punycode peer host\n";return 2;}host.push_back(static_cast<char>(ch));}
        return RouteProbe(argv[2],host,argc>4?static_cast<std::uint16_t>(_wtoi(argv[4])):0,
            argc>5?static_cast<std::uint16_t>(_wtoi(argv[5])):0)?0:1;
    }
    if(argc>2&&std::wstring_view(argv[1])==L"--prepare")return Prepare(argv[2])?0:1;
    if(argc>7&&std::wstring_view(argv[1])==L"--process")return ProcessProbe(argv[2],argv[3],
        static_cast<std::uint16_t>(_wtoi(argv[4])),static_cast<std::uint16_t>(_wtoi(argv[5])),
        static_cast<std::uint32_t>(_wtoi(argv[6])),argv[7])?0:1;
    const bool ok=SelfTest(root)&&NegativeTests(root)&&DnsRouteTests(root);
    return ok?0:1;
}
