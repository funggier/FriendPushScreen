#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <wincrypt.h>
#include <msquic.h>
#include "network/secure_peer_transport.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>

namespace fps::network {
namespace {
constexpr std::uint64_t kProtocolError=0x465001,kExpiredMedia=0x465002;
constexpr std::size_t kMaxConnections=4,kMaxControlPending=32,kMaxEvents=128;
constexpr std::uint8_t kAlpnBytes[]={'f','r','i','e','n','d','p','u','s','h','/','1'};
QUIC_BUFFER Alpn(){return {sizeof(kAlpnBytes),const_cast<std::uint8_t*>(kAlpnBytes)};}
std::string StatusText(const char* action,QUIC_STATUS status) {
    std::ostringstream s;s<<action<<" (0x"<<std::hex<<static_cast<unsigned long>(status)<<')';return s.str();
}
std::string Utf8(const std::filesystem::path& path) {
    const auto wide=path.wstring();
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),static_cast<int>(wide.size()),nullptr,0,nullptr,nullptr);
    if(size<=0)return {};
    std::string result(static_cast<std::size_t>(size),'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),static_cast<int>(wide.size()),result.data(),size,nullptr,nullptr);
    return result;
}
bool WriteTrust(const std::filesystem::path& path,const PublicPeerIdentity& peer,std::string& error) {
    DWORD size=0;
    if(!CryptBinaryToStringA(peer.certificate_der.data(),static_cast<DWORD>(peer.certificate_der.size()),CRYPT_STRING_BASE64HEADER,nullptr,&size)) {
        error="Peer trust PEM encoding failed";return false;
    }
    std::string pem(size,'\0');
    if(!CryptBinaryToStringA(peer.certificate_der.data(),static_cast<DWORD>(peer.certificate_der.size()),CRYPT_STRING_BASE64HEADER,pem.data(),&size))return false;
    if(!pem.empty()&&pem.back()=='\0')pem.pop_back();
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    if(ec){error="Transport data directory is not writable";return false;}
    std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(pem.data(),static_cast<std::streamsize>(pem.size()));
    if(!file){error="Public peer trust file write failed";return false;}return true;
}
std::uint32_t ReadSize(const std::uint8_t* b) {
    return (static_cast<std::uint32_t>(b[0])<<24)|(static_cast<std::uint32_t>(b[1])<<16)|
        (static_cast<std::uint32_t>(b[2])<<8)|b[3];
}
}
struct SecurePeerTransport::Impl {
    struct Connection;
    struct SendBuffer {
        std::vector<std::uint8_t> bytes;
        QUIC_BUFFER buffer{};
        bool control{false};
    };
    struct Stream {
        Connection* connection{};
        HQUIC handle{};
        bool media{},outgoing{},closed{},lost{},receiveAccounted{},fin{};
        std::uint64_t created{GetTickCount64()};
        std::vector<std::uint8_t> received;
        std::size_t sendBytes{};
    };
    struct Connection {
        Impl* owner{};
        HQUIC handle{};
        std::uint64_t id{};
        bool client{},certificateValid{},tlsConnected{},announced{},closed{},closing{};
        Stream* control{};
        std::size_t pendingControl{};
        std::deque<std::unique_ptr<Stream>> streams;
    };
    struct CallbackScope {
        Impl& owner;
        explicit CallbackScope(Impl& value):owner(value){++owner.callbacks;}
        ~CallbackScope(){--owner.callbacks;}
    };
    mutable std::recursive_mutex mutex;
    std::atomic<unsigned> callbacks{0};
    HMODULE library{};
    const QUIC_API_TABLE* api{};
    using OpenFn=QUIC_STATUS(QUIC_API*)(std::uint32_t,const void**);
    using CloseFn=void(QUIC_API*)(const void*);
    CloseFn closeApi{};
    HQUIC registration{},serverConfiguration{},clientConfiguration{},listener{};
    HANDLE portReservation{};
    bool stopping{},winsock{};
    std::uint16_t port{};
    std::uint64_t nextId{1};
    PublicPeerIdentity peer;
    TransportConfig config;
    TransportMetrics metrics;
    std::string runtime;
    std::deque<std::unique_ptr<Connection>> connections;
    std::deque<TransportEvent> events;

    void Event(TransportEventKind kind,Connection* c,std::vector<std::uint8_t> bytes={},std::string detail={}) {
        if(events.size()>=kMaxEvents) {
            if(c&&!c->closing){c->closing=true;api->ConnectionShutdown(c->handle,QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,kProtocolError);}
            return;
        }
        events.push_back({kind,c?c->id:0,std::move(bytes),std::move(detail)});
    }
    void Error(Connection* c,const char* action,QUIC_STATUS status){Event(TransportEventKind::Error,c,{},StatusText(action,status));}
    void CloseProtocol(Connection& c,const char* reason) {
        if(c.closing)return;c.closing=true;Event(TransportEventKind::Error,&c,{},reason);
        api->ConnectionShutdown(c.handle,QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,kProtocolError);
    }
    void Lost(Stream& s) {
        if(!s.lost){s.lost=true;++metrics.dropped_video;Event(TransportEventKind::MediaLost,s.connection);}
    }
    void ReleaseReceive(Stream& s) {
        if(!s.receiveAccounted)return;s.receiveAccounted=false;
        --metrics.receive_media_count;metrics.receive_media_bytes-=s.received.size();s.received.clear();
    }
    void Announce(Connection& c) {
        if(c.tlsConnected&&c.certificateValid&&c.control&&!c.announced&&!c.closing) {
            c.announced=true;Event(TransportEventKind::Connected,&c);
        }
    }
    Connection* Find(std::uint64_t id) {
        for(auto& c:connections)if(!c->closed&&!c->closing&&c->announced&&(!id||c->id==id))return c.get();
        return nullptr;
    }
    bool Send(Stream& s,std::vector<std::uint8_t> bytes,bool control,bool fin) {
        auto send=std::make_unique<SendBuffer>();send->bytes=std::move(bytes);send->control=control;
        send->buffer={static_cast<std::uint32_t>(send->bytes.size()),send->bytes.data()};
        if(control)++s.connection->pendingControl;
        const auto status=api->StreamSend(s.handle,&send->buffer,1,fin?QUIC_SEND_FLAG_FIN:QUIC_SEND_FLAG_NONE,send.get());
        if(QUIC_FAILED(status)){if(control)--s.connection->pendingControl;Error(s.connection,"StreamSend failed",status);return false;}
        send.release();return true;
    }
    static QUIC_STATUS QUIC_API ListenerCallback(HQUIC,void* context,QUIC_LISTENER_EVENT* event) {
        auto& self=*static_cast<Impl*>(context);CallbackScope scope(self);std::lock_guard lock(self.mutex);
        if(event->Type!=QUIC_LISTENER_EVENT_NEW_CONNECTION)return QUIC_STATUS_SUCCESS;
        if(self.stopping || std::count_if(self.connections.begin(),self.connections.end(),[](auto& c){return !c->closed;})>=kMaxConnections)
            return QUIC_STATUS_CONNECTION_REFUSED;
        auto c=std::make_unique<Connection>();c->owner=&self;c->handle=event->NEW_CONNECTION.Connection;c->id=self.nextId++;
        auto* raw=c.get();self.connections.push_back(std::move(c));
        self.api->SetCallbackHandler(raw->handle,reinterpret_cast<void*>(ConnectionCallback),raw);
        const auto status=self.api->ConnectionSetConfiguration(raw->handle,self.serverConfiguration);
        if(QUIC_FAILED(status)){raw->closed=true;raw->handle=nullptr;} // A rejected listener callback leaves the handle with MsQuic.
        return status;
    }
    static QUIC_STATUS QUIC_API ConnectionCallback(HQUIC handle,void* context,QUIC_CONNECTION_EVENT* event) {
        auto& c=*static_cast<Connection*>(context);auto& self=*c.owner;
        CallbackScope scope(self);std::lock_guard lock(self.mutex);
        switch(event->Type) {
        case QUIC_CONNECTION_EVENT_PEER_CERTIFICATE_RECEIVED: {
            const auto* der=static_cast<const QUIC_BUFFER*>(event->PEER_CERTIFICATE_RECEIVED.Certificate);
            c.certificateValid=der&&der->Length&&der->Length<=kMaxPairingBytes&&
                Sha256({der->Buffer,der->Length})==self.peer.fingerprint;
            if(!c.certificateValid){++self.metrics.rejected_certificates;self.Event(TransportEventKind::Error,&c,{},"Peer certificate fingerprint mismatch");return QUIC_STATUS_BAD_CERTIFICATE;}
            break;
        }
        case QUIC_CONNECTION_EVENT_CONNECTED:
            if(!c.certificateValid){self.CloseProtocol(c,"Peer TLS identity was not verified");break;}
            c.tlsConnected=true;
            if(c.client) {
                auto stream=std::make_unique<Stream>();stream->connection=&c;stream->outgoing=true;
                const auto status=self.api->StreamOpen(handle,QUIC_STREAM_OPEN_FLAG_NONE,StreamCallback,stream.get(),&stream->handle);
                if(QUIC_FAILED(status)){self.Error(&c,"Control stream open failed",status);self.CloseProtocol(c,"Control stream unavailable");break;}
                c.control=stream.get();c.streams.push_back(std::move(stream));
                const auto start=self.api->StreamStart(c.control->handle,QUIC_STREAM_START_FLAG_IMMEDIATE);
                if(QUIC_FAILED(start)){self.CloseProtocol(c,"Control stream start failed");break;}
                if(!self.Send(*c.control,{0,0,0,0},true,false)){self.CloseProtocol(c,"Control stream announcement failed");break;}
            }
            self.Announce(c);break;
        case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
            const bool media=(event->PEER_STREAM_STARTED.Flags&QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL)!=0;
            if(!media&&(c.client||c.control)){self.CloseProtocol(c,"Unexpected duplicate control stream");break;}
            auto stream=std::make_unique<Stream>();stream->connection=&c;stream->handle=event->PEER_STREAM_STARTED.Stream;stream->media=media;
            auto* raw=stream.get();c.streams.push_back(std::move(stream));
            self.api->SetCallbackHandler(raw->handle,reinterpret_cast<void*>(StreamCallback),raw);
            if(media) {
                if(self.metrics.receive_media_count>=kMaxMediaInFlight) {
                    raw->lost=true;++self.metrics.dropped_video;self.Event(TransportEventKind::MediaLost,&c);
                    self.api->StreamShutdown(raw->handle,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,kExpiredMedia);
                } else {
                    raw->receiveAccounted=true;++self.metrics.receive_media_count;
                    self.metrics.max_receive_media_count=std::max(self.metrics.max_receive_media_count,self.metrics.receive_media_count);
                }
            } else {c.control=raw;self.Announce(c);}
            break;
        }
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
            if(!c.closing)self.Error(&c,"QUIC connection closed",event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
            c.closing=true;break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:c.closing=true;break;
        case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
            c.closing=true;c.closed=true;c.tlsConnected=false;
            self.Event(TransportEventKind::Disconnected,&c);
            self.api->ConnectionClose(handle);c.handle=nullptr;break;
        default:break;
        }
        return QUIC_STATUS_SUCCESS;
    }
    bool ReceiveControl(Stream& s,const QUIC_BUFFER& buffer) {
        auto& c=*s.connection;
        std::size_t offset=0;
        // Consume one bounded length-prefixed message at a time, including coalesced receives.
        while(offset<buffer.Length) {
            const std::size_t target=s.received.size()<4?4:4ULL+ReadSize(s.received.data());
            if(target>kMaxControlBytes+4){CloseProtocol(c,"Oversized control message");return false;}
            const auto amount=std::min<std::size_t>(target-s.received.size(),buffer.Length-offset);
            s.received.insert(s.received.end(),buffer.Buffer+offset,buffer.Buffer+offset+amount);offset+=amount;
            if(s.received.size()==target&&s.received.size()>=4) {
                const auto size=ReadSize(s.received.data());
                if(size>kMaxControlBytes){CloseProtocol(c,"Oversized control message");return false;}
                if(s.received.size()==size+4ULL) {
                    if(size){++metrics.received_control;metrics.received_bytes+=size;
                        Event(TransportEventKind::Control,&c,{s.received.begin()+4,s.received.end()});}
                    s.received.clear();
                }
            }
        }
        return true;
    }
    static QUIC_STATUS QUIC_API StreamCallback(HQUIC handle,void* context,QUIC_STREAM_EVENT* event) {
        auto& s=*static_cast<Stream*>(context);auto& c=*s.connection;auto& self=*c.owner;
        CallbackScope scope(self);std::lock_guard lock(self.mutex);
        switch(event->Type) {
        case QUIC_STREAM_EVENT_RECEIVE:
            if(s.closed||s.lost)break;
            for(std::uint32_t i=0;i<event->RECEIVE.BufferCount;++i) {
                const auto& buffer=event->RECEIVE.Buffers[i];
                if(!s.media){if(!self.ReceiveControl(s,buffer))break;continue;}
                if(!s.receiveAccounted || buffer.Length>kMaxMediaBytes-s.received.size()) {
                    self.Lost(s);self.ReleaseReceive(s);self.api->StreamShutdown(handle,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,kProtocolError);break;
                }
                s.received.insert(s.received.end(),buffer.Buffer,buffer.Buffer+buffer.Length);
                self.metrics.receive_media_bytes+=buffer.Length;
                self.metrics.max_receive_media_bytes=std::max(self.metrics.max_receive_media_bytes,self.metrics.receive_media_bytes);
            }
            break;
        case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            s.fin=true;
            if(s.media&&!s.outgoing&&!s.lost&&s.receiveAccounted) {
                if(s.received.empty()||GetTickCount64()-s.created>self.config.media_deadline_ms){self.Lost(s);self.ReleaseReceive(s);}
                else {
                    ++self.metrics.received_video;self.metrics.received_bytes+=s.received.size();
                    // Transfer receive accounting to the queued event until PollEvents consumes it.
                    s.receiveAccounted=false;self.Event(TransportEventKind::Video,&c,std::move(s.received));
                }
            } else if(!s.media&&!c.closing)self.CloseProtocol(c,"Control stream ended");
            break;
        case QUIC_STREAM_EVENT_SEND_COMPLETE: {
            auto send=std::unique_ptr<SendBuffer>{static_cast<SendBuffer*>(event->SEND_COMPLETE.ClientContext)};
            if(send&&send->control&&c.pendingControl)--c.pendingControl;
            break;
        }
        case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
        case QUIC_STREAM_EVENT_PEER_RECEIVE_ABORTED:
            if(s.media){self.Lost(s);self.ReleaseReceive(s);}
            else if(!c.closing)self.CloseProtocol(c,"Control stream aborted");
            self.api->StreamShutdown(handle,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,kExpiredMedia);break;
        case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
            if(s.media) {
                if(!s.outgoing&&!s.fin&&!s.lost)self.Lost(s);
                self.ReleaseReceive(s);
                if(s.outgoing&&s.sendBytes){--self.metrics.send_media_count;self.metrics.send_media_bytes-=s.sendBytes;s.sendBytes=0;}
            }
            s.closed=true;self.api->StreamClose(handle);s.handle=nullptr;break;
        default:break;
        }
        return QUIC_STATUS_SUCCESS;
    }
    bool Load(std::string& error) {
        std::array<wchar_t,32768> name{};const auto size=GetModuleFileNameW(nullptr,name.data(),static_cast<DWORD>(name.size()));
        if(!size||size==name.size()){error="Executable path unavailable";return false;}
        const auto path=std::filesystem::path(std::wstring(name.data(),size)).parent_path()/L"msquic.dll";
        library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library){error=StatusText("Pinned QUIC runtime load failed",HRESULT_FROM_WIN32(GetLastError()));return false;}
        const auto openSymbol=GetProcAddress(library,"MsQuicOpenVersion"),closeSymbol=GetProcAddress(library,"MsQuicClose");
        OpenFn open{};static_assert(sizeof(open)==sizeof(openSymbol));std::memcpy(&open,&openSymbol,sizeof(open));
        std::memcpy(&closeApi,&closeSymbol,sizeof(closeApi));
        if(!open||!closeApi){error="QUIC runtime exports unavailable";return false;}
        const auto status=open(2,reinterpret_cast<const void**>(&api));
        if(QUIC_FAILED(status)){error=StatusText("MsQuicOpenVersion failed",status);return false;}
        QUIC_TLS_PROVIDER provider{};std::uint32_t length=sizeof(provider);
        if(QUIC_FAILED(api->GetParam(nullptr,QUIC_PARAM_GLOBAL_TLS_PROVIDER,&length,&provider))||provider!=QUIC_TLS_PROVIDER_OPENSSL) {
            error="The pinned OpenSSL TLS provider is required";return false;
        }
        std::array<std::uint32_t,4> version{};length=sizeof(version);
        if(QUIC_FAILED(api->GetParam(nullptr,QUIC_PARAM_GLOBAL_LIBRARY_VERSION,&length,version.data()))||
            version[0]!=2||version[1]!=6||version[2]!=3){error="Unexpected QUIC runtime version";return false;}
        runtime="MsQuic "+std::to_string(version[0])+"."+std::to_string(version[1])+"."+std::to_string(version[2])+
            "."+std::to_string(version[3])+" / OpenSSL / TLS 1.3 / mutual DER SHA-256 pin";
        QUIC_REGISTRATION_CONFIG registrationConfig{"FriendPushScreen",QUIC_EXECUTION_PROFILE_LOW_LATENCY};
        const auto registrationStatus=api->RegistrationOpen(&registrationConfig,&registration);
        if(QUIC_FAILED(registrationStatus)){error=StatusText("QUIC registration failed",registrationStatus);return false;}
        return true;
    }
    bool Configuration(const PeerIdentity& identity,bool client,HQUIC& result,const std::string& trustPath,std::string& error) {
        QUIC_SETTINGS settings{};
        settings.IdleTimeoutMs=10000;settings.IsSet.IdleTimeoutMs=TRUE;
        settings.HandshakeIdleTimeoutMs=4000;settings.IsSet.HandshakeIdleTimeoutMs=TRUE;
        settings.PeerBidiStreamCount=client?0:1;settings.IsSet.PeerBidiStreamCount=TRUE;
        settings.PeerUnidiStreamCount=4;settings.IsSet.PeerUnidiStreamCount=TRUE;
        settings.SendBufferingEnabled=FALSE;settings.IsSet.SendBufferingEnabled=TRUE;
        settings.KeepAliveIntervalMs=2000;settings.IsSet.KeepAliveIntervalMs=TRUE;
        settings.ServerResumptionLevel=QUIC_SERVER_NO_RESUME;settings.IsSet.ServerResumptionLevel=TRUE;
        auto alpn=Alpn();auto status=api->ConfigurationOpen(registration,&alpn,1,&settings,sizeof(settings),nullptr,&result);
        if(QUIC_FAILED(status)){error=StatusText("QUIC configuration failed",status);return false;}
        QUIC_CERTIFICATE_PKCS12 certificate{identity.PfxBytes().data(),static_cast<std::uint32_t>(identity.PfxBytes().size()),""};
        QUIC_CREDENTIAL_CONFIG credentials{};credentials.Type=QUIC_CREDENTIAL_TYPE_CERTIFICATE_PKCS12;
        credentials.CertificatePkcs12=&certificate;credentials.CaCertificateFile=trustPath.c_str();
        credentials.Flags=QUIC_CREDENTIAL_FLAG_USE_TLS_BUILTIN_CERTIFICATE_VALIDATION|
            QUIC_CREDENTIAL_FLAG_SET_CA_CERTIFICATE_FILE|QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED|
            QUIC_CREDENTIAL_FLAG_USE_PORTABLE_CERTIFICATES|
            (client?QUIC_CREDENTIAL_FLAG_CLIENT:QUIC_CREDENTIAL_FLAG_REQUIRE_CLIENT_AUTHENTICATION);
        status=api->ConfigurationLoadCredential(result,&credentials);
        if(QUIC_FAILED(status)){error=StatusText("Mutual TLS credential load failed",status);return false;}
        return true;
    }
    void Maintenance() {
        const auto now=GetTickCount64();
        for(auto& c:connections)for(auto& s:c->streams)
            if(s->media&&!s->closed&&!s->lost&&now-s->created>config.media_deadline_ms) {
                Lost(*s);ReleaseReceive(*s);api->StreamShutdown(s->handle,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,kExpiredMedia);
            }
        // Closed contexts stay alive until no callback can still be unwinding.
        if(callbacks.load()==0) {
            for(auto& c:connections)std::erase_if(c->streams,[&](auto& s){return s->closed&&c->control!=s.get();});
            std::erase_if(connections,[](auto& c){return c->closed;});
        }
    }
};
SecurePeerTransport::SecurePeerTransport():impl_(std::make_unique<Impl>()){}
SecurePeerTransport::~SecurePeerTransport(){Stop();}
bool SecurePeerTransport::Start(const PeerIdentity& identity,const PublicPeerIdentity& peer,TransportConfig config,std::string& error) {
    Stop();impl_=std::make_unique<Impl>();auto& self=*impl_;self.peer=peer;self.config=std::move(config);
    if(!ValidateTrustedPeer(identity.Public(),peer,error))return false;
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0){error="Windows sockets startup failed";return false;}self.winsock=true;
    if(!self.Load(error)){Stop();return false;}
    const auto trustFile=self.config.data_directory/"transport-peer-ca.pem";
    if(!WriteTrust(trustFile,peer,error)){Stop();return false;}const auto trustPath=Utf8(trustFile);
    if(trustPath.empty()||!self.Configuration(identity,false,self.serverConfiguration,trustPath,error)||
       !self.Configuration(identity,true,self.clientConfiguration,trustPath,error)){Stop();return false;}
    if(self.config.listen_port) {
        const auto mutexName=L"Local\\FriendPushScreen.UDP."+std::to_wstring(self.config.listen_port);
        self.portReservation=CreateMutexW(nullptr,FALSE,mutexName.c_str());
        if(!self.portReservation||GetLastError()==ERROR_ALREADY_EXISTS){error="This share port is already used by another FriendPushScreen instance";Stop();return false;}
    }
    auto status=self.api->ListenerOpen(self.registration,Impl::ListenerCallback,&self,&self.listener);
    if(QUIC_FAILED(status)){error=StatusText("QUIC listener open failed",status);Stop();return false;}
    QUIC_ADDR address{};QuicAddrSetFamily(&address,QUIC_ADDRESS_FAMILY_INET);
    QuicAddrSetPort(&address,self.config.listen_port);
    if(self.config.loopback_only)address.Ipv4.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    auto alpn=Alpn();status=self.api->ListenerStart(self.listener,&alpn,1,&address);
    if(QUIC_FAILED(status)){error=StatusText("QUIC listener bind failed",status);Stop();return false;}
    std::uint32_t length=sizeof(address);
    status=self.api->GetParam(self.listener,QUIC_PARAM_LISTENER_LOCAL_ADDRESS,&length,&address);
    if(QUIC_FAILED(status)){error=StatusText("QUIC listener address unavailable",status);Stop();return false;}
    self.port=QuicAddrGetPort(&address);return true;
}
bool SecurePeerTransport::Connect(const std::string& host,std::uint16_t port,std::string& error) {
    auto& self=*impl_;std::lock_guard lock(self.mutex);
    if(!self.registration||self.stopping||!port||host.empty()||host.size()>253){error="Invalid peer endpoint or inactive transport";return false;}
    self.Maintenance();
    if(self.connections.size()>=kMaxConnections){error="Peer connection limit reached";return false;}
    addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_DGRAM;addrinfo* resolved=nullptr;
    const auto service=std::to_string(port);const auto lookup=getaddrinfo(host.c_str(),service.c_str(),&hints,&resolved);
    if(lookup||!resolved){error="Peer host resolution failed";return false;}
    QUIC_ADDR address{};std::memcpy(&address,resolved->ai_addr,sizeof(sockaddr_in));freeaddrinfo(resolved);
    if(self.config.loopback_only&&(ntohl(address.Ipv4.sin_addr.s_addr)>>24)!=127){error="This transport milestone is restricted to loopback";return false;}
    if(port==self.port&&(ntohl(address.Ipv4.sin_addr.s_addr)>>24)==127){error="Cannot connect to this instance's own share port";return false;}
    auto c=std::make_unique<Impl::Connection>();c->owner=&self;c->id=self.nextId++;c->client=true;
    auto status=self.api->ConnectionOpen(self.registration,Impl::ConnectionCallback,c.get(),&c->handle);
    if(QUIC_FAILED(status)){error=StatusText("QUIC connection open failed",status);return false;}
    auto* raw=c.get();self.connections.push_back(std::move(c));
    status=self.api->SetParam(raw->handle,QUIC_PARAM_CONN_REMOTE_ADDRESS,sizeof(address),&address);
    if(QUIC_SUCCEEDED(status))status=self.api->ConnectionStart(raw->handle,self.clientConfiguration,QUIC_ADDRESS_FAMILY_INET,self.peer.stable_name.c_str(),port);
    if(QUIC_FAILED(status)){raw->closed=true;self.api->ConnectionClose(raw->handle);raw->handle=nullptr;error=StatusText("QUIC connection start failed",status);return false;}
    return true;
}
TransportSendResult SecurePeerTransport::SendControl(std::span<const std::uint8_t> bytes,std::uint64_t id) {
    if(bytes.empty()||bytes.size()>kMaxControlBytes)return TransportSendResult::Invalid;
    auto& self=*impl_;std::lock_guard lock(self.mutex);auto* c=self.Find(id);
    if(!c||!c->control||c->control->closed)return TransportSendResult::NotConnected;
    if(c->pendingControl>=kMaxControlPending)return TransportSendResult::Backpressure;
    std::vector<std::uint8_t> framed;framed.reserve(bytes.size()+4);
    const auto size=static_cast<std::uint32_t>(bytes.size());
    for(int shift=24;shift>=0;shift-=8)framed.push_back(static_cast<std::uint8_t>(size>>shift));
    framed.insert(framed.end(),bytes.begin(),bytes.end());
    if(!self.Send(*c->control,std::move(framed),true,false))return TransportSendResult::NotConnected;
    ++self.metrics.sent_control;self.metrics.sent_bytes+=bytes.size();return TransportSendResult::Sent;
}
TransportSendResult SecurePeerTransport::SendVideo(std::vector<std::uint8_t> bytes,std::uint64_t id) {
    if(bytes.empty()||bytes.size()>kMaxMediaBytes)return TransportSendResult::Invalid;
    auto& self=*impl_;std::lock_guard lock(self.mutex);auto* c=self.Find(id);
    if(!c)return TransportSendResult::NotConnected;
    if(self.metrics.send_media_count>=kMaxMediaInFlight){++self.metrics.dropped_video;return TransportSendResult::Backpressure;}
    auto stream=std::make_unique<Impl::Stream>();stream->connection=c;stream->media=true;stream->outgoing=true;
    auto status=self.api->StreamOpen(c->handle,QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,Impl::StreamCallback,stream.get(),&stream->handle);
    if(QUIC_FAILED(status))return TransportSendResult::Backpressure;
    auto* raw=stream.get();c->streams.push_back(std::move(stream));raw->sendBytes=bytes.size();
    ++self.metrics.send_media_count;self.metrics.send_media_bytes+=bytes.size();
    self.metrics.max_send_media_count=std::max(self.metrics.max_send_media_count,self.metrics.send_media_count);
    self.metrics.max_send_media_bytes=std::max(self.metrics.max_send_media_bytes,self.metrics.send_media_bytes);
    status=self.api->StreamStart(raw->handle,QUIC_STREAM_START_FLAG_IMMEDIATE);
    if(QUIC_FAILED(status)||!self.Send(*raw,std::move(bytes),false,true)) {
        self.api->StreamShutdown(raw->handle,QUIC_STREAM_SHUTDOWN_FLAG_ABORT,kExpiredMedia);return TransportSendResult::Backpressure;
    }
    ++self.metrics.sent_video;self.metrics.sent_bytes+=raw->sendBytes;return TransportSendResult::Sent;
}
std::vector<TransportEvent> SecurePeerTransport::PollEvents() {
    auto& self=*impl_;std::lock_guard lock(self.mutex);
    if(self.api)self.Maintenance();
    std::vector<TransportEvent> result;result.reserve(self.events.size());
    while(!self.events.empty()) {
        auto& event=self.events.front();
        if(event.kind==TransportEventKind::Video){--self.metrics.receive_media_count;self.metrics.receive_media_bytes-=event.bytes.size();}
        result.push_back(std::move(event));self.events.pop_front();
    }
    return result;
}
TransportMetrics SecurePeerTransport::Metrics()const {std::lock_guard lock(impl_->mutex);return impl_->metrics;}
std::uint16_t SecurePeerTransport::ListenPort()const{return impl_->port;}
std::string SecurePeerTransport::RuntimeInfo()const{return impl_->runtime;}
void SecurePeerTransport::Disconnect(std::uint64_t id) {
    auto& self=*impl_;std::lock_guard lock(self.mutex);
    if(!self.api)return;
    for(auto& c:self.connections)if(!c->closed&&!c->closing&&(!id||c->id==id)){c->closing=true;self.api->ConnectionShutdown(c->handle,QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,0);}
}
void SecurePeerTransport::Stop() {
    auto& self=*impl_;
    {std::lock_guard lock(self.mutex);self.stopping=true;}
    // Closing the listener and registration may wait for callbacks: never hold their callback mutex.
    if(self.listener){self.api->ListenerClose(self.listener);self.listener=nullptr;}
    Disconnect();
    if(self.serverConfiguration){self.api->ConfigurationClose(self.serverConfiguration);self.serverConfiguration=nullptr;}
    if(self.clientConfiguration){self.api->ConfigurationClose(self.clientConfiguration);self.clientConfiguration=nullptr;}
    if(self.registration){self.api->RegistrationClose(self.registration);self.registration=nullptr;}
    self.connections.clear();self.events.clear();self.metrics.send_media_count=0;self.metrics.receive_media_count=0;
    self.metrics.send_media_bytes=0;self.metrics.receive_media_bytes=0;
    if(self.api&&self.closeApi){self.closeApi(self.api);self.api=nullptr;}
    if(self.library){FreeLibrary(self.library);self.library=nullptr;}
    if(self.portReservation){CloseHandle(self.portReservation);self.portReservation=nullptr;}
    if(self.winsock){WSACleanup();self.winsock=false;}self.port=0;
}
}
