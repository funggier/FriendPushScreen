#pragma once
#include <msquic.h>
#include <wincrypt.h>
#include <atomic>
#include <cstring>
namespace {
struct NativeTlsFailure {
    bool setup{},finished{},server_accepted{};
    QUIC_STATUS status{};
};
struct NativeTlsClient {
    const QUIC_API_TABLE* api{};
    std::atomic<bool> done{false};
    QUIC_STATUS status{};
    static QUIC_STATUS QUIC_API Callback(HQUIC handle,void* context,QUIC_CONNECTION_EVENT* event) {
        auto& self=*static_cast<NativeTlsClient*>(context);
        if(event->Type==QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT)self.status=event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status;
        if(event->Type==QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE) {
            self.api->ConnectionClose(handle);self.done.store(true,std::memory_order_release);
        }
        return QUIC_STATUS_SUCCESS;
    }
};
NativeTlsFailure RunNativeTlsNegative(fps::network::SecurePeerTransport& server,
    const fps::network::PublicPeerIdentity& server_identity,const fps::network::PeerIdentity* client_identity,
    const std::string& expected_name,const std::filesystem::path& trust_file) {
    NativeTlsFailure result;
    DWORD count=0;
    if(!CryptBinaryToStringA(server_identity.certificate_der.data(),static_cast<DWORD>(server_identity.certificate_der.size()),
        CRYPT_STRING_BASE64HEADER,nullptr,&count))return result;
    std::string pem(count,'\0');
    if(!CryptBinaryToStringA(server_identity.certificate_der.data(),static_cast<DWORD>(server_identity.certificate_der.size()),
        CRYPT_STRING_BASE64HEADER,pem.data(),&count))return result;
    if(!pem.empty()&&pem.back()=='\0')pem.pop_back();
    {std::ofstream file(trust_file,std::ios::binary);file.write(pem.data(),static_cast<std::streamsize>(pem.size()));if(!file)return result;}
    const auto path=trust_file.u8string();const std::string ca(reinterpret_cast<const char*>(path.data()),path.size());
    const auto library=GetModuleHandleW(L"msquic.dll");if(!library)return result;
    using Open=QUIC_STATUS(QUIC_API*)(std::uint32_t,const void**);using Close=void(QUIC_API*)(const void*);
    Open open{};Close close{};const auto open_symbol=GetProcAddress(library,"MsQuicOpenVersion"),close_symbol=GetProcAddress(library,"MsQuicClose");
    std::memcpy(&open,&open_symbol,sizeof(open));std::memcpy(&close,&close_symbol,sizeof(close));
    const QUIC_API_TABLE* api=nullptr;
    if(!open||!close||QUIC_FAILED(open(2,reinterpret_cast<const void**>(&api))))return result;
    HQUIC registration=nullptr,configuration=nullptr,connection=nullptr;
    NativeTlsClient client;client.api=api;
    QUIC_REGISTRATION_CONFIG registration_config{"FriendPushScreen credential rejection fixture",QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    auto status=api->RegistrationOpen(&registration_config,&registration);
    const std::uint8_t bytes[]={'f','r','i','e','n','d','p','u','s','h','/','1'};
    QUIC_BUFFER alpn{sizeof(bytes),const_cast<std::uint8_t*>(bytes)};
    QUIC_SETTINGS settings{};settings.HandshakeIdleTimeoutMs=2000;settings.IsSet.HandshakeIdleTimeoutMs=TRUE;
    if(QUIC_SUCCEEDED(status))status=api->ConfigurationOpen(registration,&alpn,1,&settings,sizeof(settings),nullptr,&configuration);
    QUIC_CREDENTIAL_CONFIG credential{};
    credential.Flags=QUIC_CREDENTIAL_FLAG_CLIENT|QUIC_CREDENTIAL_FLAG_USE_TLS_BUILTIN_CERTIFICATE_VALIDATION|QUIC_CREDENTIAL_FLAG_SET_CA_CERTIFICATE_FILE;
    credential.CaCertificateFile=ca.c_str();credential.Type=QUIC_CREDENTIAL_TYPE_NONE;
    QUIC_CERTIFICATE_PKCS12 pfx{};
    if(client_identity) {
        pfx={client_identity->PfxBytes().data(),static_cast<std::uint32_t>(client_identity->PfxBytes().size()),""};
        credential.Type=QUIC_CREDENTIAL_TYPE_CERTIFICATE_PKCS12;credential.CertificatePkcs12=&pfx;
    }
    if(QUIC_SUCCEEDED(status))status=api->ConfigurationLoadCredential(configuration,&credential);
    if(QUIC_SUCCEEDED(status))status=api->ConnectionOpen(registration,NativeTlsClient::Callback,&client,&connection);
    QUIC_ADDR address{};QuicAddrSetFamily(&address,QUIC_ADDRESS_FAMILY_INET);QuicAddrSetPort(&address,server.ListenPort());
    address.Ipv4.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(QUIC_SUCCEEDED(status))status=api->SetParam(connection,QUIC_PARAM_CONN_REMOTE_ADDRESS,sizeof(address),&address);
    if(QUIC_SUCCEEDED(status))status=api->ConnectionStart(connection,configuration,QUIC_ADDRESS_FAMILY_INET,expected_name.c_str(),server.ListenPort());
    result.setup=QUIC_SUCCEEDED(status);
    const auto until=GetTickCount64()+4000;
    while(result.setup&&!client.done.load(std::memory_order_acquire)&&GetTickCount64()<until) {
        for(const auto& event:server.PollEvents())
            result.server_accepted|=event.kind==fps::network::TransportEventKind::Connected||
                event.kind==fps::network::TransportEventKind::Control||event.kind==fps::network::TransportEventKind::Video;
        Sleep(2);
    }
    if(connection&&!client.done.load(std::memory_order_acquire)) {
        api->ConnectionShutdown(connection,QUIC_CONNECTION_SHUTDOWN_FLAG_NONE,0);
        const auto stopped=GetTickCount64()+5000;
        while(!client.done.load(std::memory_order_acquire)&&GetTickCount64()<stopped)Sleep(2);
        if(!client.done.load(std::memory_order_acquire))std::abort(); // Fail the owned probe without freeing a live callback context.
    }
    for(const auto& event:server.PollEvents())
        result.server_accepted|=event.kind==fps::network::TransportEventKind::Connected||
            event.kind==fps::network::TransportEventKind::Control||event.kind==fps::network::TransportEventKind::Video;
    result.finished=client.done.load(std::memory_order_acquire);result.status=client.status;
    if(configuration)api->ConfigurationClose(configuration);
    if(registration)api->RegistrationClose(registration);
    close(api);return result;
}
}
