#include "app/peer_video_controller.h"
#include "test_support.h"
#include "peer_controller_codec_fakes.h"
using namespace fps::app;
void RunPeerVideoControllerTests() {
    const auto root=std::filesystem::current_path()/("fps-controller-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    {
        PeerVideoController a,b;std::string error;
        const bool initialized=a.Initialize(nullptr,root/"a",error)&&b.Initialize(nullptr,root/"b",error);
        FPS_EXPECT_TRUE(initialized);if(!initialized){std::filesystem::remove_all(root);return;}
        FPS_EXPECT_TRUE(!a.Listen(0,error)); // explicit trust required before opening any listener
        FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error)&&b.AuthorizePeer(*a.Identity(),error));
        FPS_EXPECT_TRUE(!a.AuthorizePeer(*a.Identity(),error));
        FPS_EXPECT_TRUE(a.Listen(0,error)&&b.Listen(0,error));
        const auto sink=[](const fps::capture::CapturedFrame&){return true;};const auto clear=[]{};
        auto until=GetTickCount64()+5000;
        while(GetTickCount64()<until&&(!a.Snapshot().listen_port||!b.Snapshot().listen_port)){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        auto as=a.Snapshot(),bs=b.Snapshot();
        FPS_EXPECT_TRUE(as.listen_port&&bs.listen_port&&as.listen_port!=bs.listen_port);
        FPS_EXPECT_TRUE(a.Connect("127.0.0.1",bs.listen_port,0,error)&&b.Connect("127.0.0.1",as.listen_port,0,error));
        until=GetTickCount64()+5000;
        while(GetTickCount64()<until&&(!a.Snapshot().connected||!b.Snapshot().connected)){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_TRUE(a.Snapshot().connected&&b.Snapshot().connected);
        until=GetTickCount64()+100;
        while(GetTickCount64()<until){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_EQ(a.Snapshot().session,b.Snapshot().session);
        FPS_EXPECT_TRUE(a.StartShare(error));a.StopShare();FPS_EXPECT_TRUE(a.Snapshot().connected);
        FPS_EXPECT_TRUE(!a.Snapshot().wants_share&&!a.Snapshot().sharing);
        a.Disconnect();b.Disconnect();
        FPS_EXPECT_TRUE(!a.Snapshot().connected&&!a.Snapshot().wants_share);
    }
    {
        auto decoder=std::make_unique<peer_controller_test::Decoder>();auto* observed=decoder.get();
        PeerVideoController a(std::make_unique<peer_controller_test::Encoder>(),std::make_unique<peer_controller_test::Decoder>()),
            b(std::make_unique<peer_controller_test::Encoder>(),std::move(decoder));
        std::string error;FPS_EXPECT_TRUE(a.Initialize(nullptr,root/"codec-a",error)&&b.Initialize(nullptr,root/"codec-b",error));
        if(!a.Identity()||!b.Identity()){std::filesystem::remove_all(root);return;}
        FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error)&&b.AuthorizePeer(*a.Identity(),error));
        fps::media::VideoCodecConfig missing;missing.decoder_id="missing-decoder";
        b.ConfigureCodecs(missing,fps::media::VideoBackendPolicy::ForceSoftware,fps::media::VideoBackendPolicy::ForceSoftware);
        FPS_EXPECT_TRUE(b.Listen(0,error));const auto sink=[](const fps::capture::CapturedFrame&){return true;};const auto clear=[]{};
        auto until=GetTickCount64()+3000;
        while(GetTickCount64()<until&&!b.Snapshot().listen_port){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_TRUE(a.Connect("127.0.0.1",b.Snapshot().listen_port,0,error));FPS_EXPECT_TRUE(a.StartShare(error));
        fps::capture::CapturedFrame source;source.width=source.height=2;source.bgra.resize(16);
        until=GetTickCount64()+3000;
        while(GetTickCount64()<until&&b.Snapshot().error.empty()){
            a.Poll(sink,clear);b.Poll(sink,clear);++source.sequence;source.timestamp_100ns=static_cast<std::int64_t>(source.sequence)*166667;
            a.PushCapture(source);b.PushCapture(source);Sleep(2);
        }
        FPS_EXPECT_TRUE(b.Snapshot().error.find("missing decoder")!=std::string::npos);
        FPS_EXPECT_TRUE(a.Snapshot().connected&&b.Snapshot().connected&&a.WantsShare());
        until=GetTickCount64()+200;
        while(GetTickCount64()<until){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_TRUE(a.Snapshot().error.find("missing decoder")!=std::string::npos);
        FPS_EXPECT_TRUE(b.StartShare(error));
        until=GetTickCount64()+2000;
        while(GetTickCount64()<until&&a.Snapshot().incoming.delivered<2) {
            a.Poll(sink,clear);b.Poll(sink,clear);++source.sequence;
            source.timestamp_100ns=static_cast<std::int64_t>(source.sequence)*166667;
            a.PushCapture(source);b.PushCapture(source);Sleep(2);
        }
        FPS_EXPECT_TRUE(a.Snapshot().incoming.delivered>=2);
        FPS_EXPECT_TRUE(b.Snapshot().error.find("missing decoder")!=std::string::npos);
        FPS_EXPECT_TRUE(a.Snapshot().error.find("missing decoder")!=std::string::npos);
        fps::media::VideoCodecConfig valid;
        b.ConfigureCodecs(valid,fps::media::VideoBackendPolicy::ForceSoftware,fps::media::VideoBackendPolicy::ForceSoftware);
        until=GetTickCount64()+2000;
        while(GetTickCount64()<until&&b.Snapshot().incoming.delivered<2){
            a.Poll(sink,clear);b.Poll(sink,clear);++source.sequence;source.timestamp_100ns=static_cast<std::int64_t>(source.sequence)*166667;
            a.PushCapture(source);b.PushCapture(source);Sleep(2);
        }
        FPS_EXPECT_TRUE(observed->initializations>=2);
        FPS_EXPECT_TRUE(b.Snapshot().incoming.delivered>=2);
        FPS_EXPECT_TRUE(a.Snapshot().error.empty()&&b.Snapshot().error.empty());
        FPS_EXPECT_TRUE(a.WantsShare()&&a.Snapshot().connected&&b.Snapshot().connected);
        std::cout<<"AudioSourceSwitchLeavesVideoRunning\n";
        until=GetTickCount64()+100;while(GetTickCount64()<until){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        const auto originalVideo=a.Snapshot().outgoing_generation,originalSession=a.Snapshot().session;

        fps::audio::AudioPreferences audioPrefs;audioPrefs.source_id="capture:__fps_missing_endpoint__";
        a.ConfigureAudio(audioPrefs);
        until=GetTickCount64()+1000;
        while(GetTickCount64()<until&&a.Snapshot().audio.send_state!=fps::audio::AudioState::Error) {
            a.Poll(sink,clear);b.Poll(sink,clear);++source.sequence;
            Sleep(2);
        }
        FPS_EXPECT_EQ(a.Snapshot().audio.send_state,fps::audio::AudioState::Error);
        FPS_EXPECT_TRUE(!a.Snapshot().audio.error.empty());
        FPS_EXPECT_EQ(a.Snapshot().session,originalSession);
        FPS_EXPECT_EQ(a.Snapshot().outgoing_generation,originalVideo);
        FPS_EXPECT_TRUE(a.WantsShare()&&a.Snapshot().sharing&&b.Snapshot().receiving);
        audioPrefs.source_id="none";a.ConfigureAudio(audioPrefs);
        until=GetTickCount64()+100;
        while(GetTickCount64()<until){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_EQ(a.Snapshot().audio.send_state,fps::audio::AudioState::Disabled);
        FPS_EXPECT_EQ(a.Snapshot().session,originalSession);
        FPS_EXPECT_EQ(a.Snapshot().outgoing_generation,originalVideo);
        a.Disconnect();b.Disconnect();
    }
    {
        std::cout<<"InvalidHostRejectedBeforeOpeningListener\n";
        PeerVideoController a,b;std::string error;
        FPS_EXPECT_TRUE(a.Initialize(nullptr,root/"invalid-a",error)&&b.Initialize(nullptr,root/"invalid-b",error));
        if(a.Identity()&&b.Identity()){
            FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error));
            FPS_EXPECT_TRUE(!a.Connect("http://cdloot.thddns.net:8997",8997,0,error));
            FPS_EXPECT_TRUE(!a.Snapshot().active);
            a.Disconnect();
        }
    }
    {
        std::cout<<"ManualPeerListenerAcceptsLocalMachineDnsRoute\n";
        PeerVideoController a,b;std::string error;
        FPS_EXPECT_TRUE(a.Initialize(nullptr,root/"route-a",error)&&b.Initialize(nullptr,root/"route-b",error));
        if(a.Identity()&&b.Identity()){
            FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error)&&b.AuthorizePeer(*a.Identity(),error));
            FPS_EXPECT_TRUE(b.Listen(0,error));
            const auto sink=[](const fps::capture::CapturedFrame&){return true;};const auto clear=[]{};
            auto until=GetTickCount64()+3000;
            while(GetTickCount64()<until&&!b.Snapshot().listen_port){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
            char localHost[MAX_COMPUTERNAME_LENGTH+1]{};DWORD nameLength=sizeof(localHost);
            FPS_EXPECT_TRUE(GetComputerNameA(localHost,&nameLength));
            FPS_EXPECT_TRUE(a.Connect(localHost,b.Snapshot().listen_port,0,error));
            until=GetTickCount64()+5000;
            while(GetTickCount64()<until&&(!a.Snapshot().connected||!b.Snapshot().connected)){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
            if(!a.Snapshot().connected||!b.Snapshot().connected){
                const auto sa=a.Snapshot(),sb=b.Snapshot();
                std::cerr<<"route "<<localHost<<" Aport="<<sa.listen_port<<" Bport="<<sb.listen_port
                    <<" Astatus="<<sa.status<<" Aerror="<<sa.error<<" Bstatus="<<sb.status<<" Berror="<<sb.error
                    <<" Arejected="<<sa.transport.rejected_certificates<<" Brejected="<<sb.transport.rejected_certificates<<"\n";
            }
            FPS_EXPECT_TRUE(a.Snapshot().connected&&b.Snapshot().connected);
            a.Disconnect();b.Disconnect();
        }
    }
    {
        std::cout<<"RemovePairedDeviceStopsMediaAndPersistsWithoutChangingOwnIdentity\n";
        std::string ownFingerprint;
        {
            PeerVideoController a(std::make_unique<peer_controller_test::Encoder>(),std::make_unique<peer_controller_test::Decoder>()),
                b(std::make_unique<peer_controller_test::Encoder>(),std::make_unique<peer_controller_test::Decoder>());
            std::string error;FPS_EXPECT_TRUE(a.Initialize(nullptr,root/"revoke-a",error)&&b.Initialize(nullptr,root/"revoke-b",error));
            if(a.Identity()&&b.Identity()) {
                ownFingerprint=a.Identity()->fingerprint;
                FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error)&&b.AuthorizePeer(*a.Identity(),error));
                FPS_EXPECT_TRUE(b.Listen(0,error));
                unsigned rendered=0,cleared=0;
                const auto sink=[&](const fps::capture::CapturedFrame&){++rendered;return true;};const auto clear=[&]{++cleared;};
                auto until=GetTickCount64()+3000;
                while(GetTickCount64()<until&&!b.Snapshot().listen_port){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
                FPS_EXPECT_TRUE(a.Connect("localhost",b.Snapshot().listen_port,0,error));
                FPS_EXPECT_TRUE(a.StartShare(error)&&b.StartShare(error));
                fps::capture::CapturedFrame frame;frame.width=frame.height=2;frame.bgra.resize(16);
                until=GetTickCount64()+3000;
                while(GetTickCount64()<until&&(a.Snapshot().incoming.delivered<2||b.Snapshot().incoming.delivered<2)){
                    a.Poll(sink,clear);b.Poll(sink,clear);++frame.sequence;frame.timestamp_100ns=static_cast<std::int64_t>(frame.sequence)*166667;
                    a.PushCapture(frame);b.PushCapture(frame);Sleep(2);
                }
                FPS_EXPECT_TRUE(a.Snapshot().incoming.delivered>=2&&b.Snapshot().incoming.delivered>=2);
                FPS_EXPECT_TRUE(a.RemoveTrustedPeer(error));
                const auto removed=a.Snapshot();
                FPS_EXPECT_TRUE(!a.TrustedPeer()&&removed.peer_fingerprint.empty());
                FPS_EXPECT_TRUE(!removed.active&&!removed.connected&&!removed.wants_share&&!removed.sharing&&!removed.receiving&&!removed.round_trip_ms);
                FPS_EXPECT_TRUE(!std::filesystem::exists(root/"revoke-a"/"trusted.friendpair"));
                FPS_EXPECT_EQ(a.Identity()->fingerprint,ownFingerprint);
                unsigned afterRemoval=0;const auto noFrame=[&](const fps::capture::CapturedFrame&){++afterRemoval;return true;};
                cleared=0;
                until=GetTickCount64()+1000;
                while(GetTickCount64()<until){a.Poll(noFrame,clear);b.Poll(sink,[]{});Sleep(2);}
                FPS_EXPECT_EQ(cleared,1U);FPS_EXPECT_EQ(afterRemoval,0U);
                FPS_EXPECT_TRUE(!b.Snapshot().connected);
                FPS_EXPECT_TRUE(!a.Listen(0,error)&&!a.Connect("localhost",8997,0,error));
                b.Disconnect();
            }
        }
        {
            PeerVideoController restored;std::string error;
            FPS_EXPECT_TRUE(restored.Initialize(nullptr,root/"revoke-a",error));
            FPS_EXPECT_TRUE(restored.Identity()&&!restored.TrustedPeer()&&!restored.Snapshot().active);
            if(restored.Identity())FPS_EXPECT_EQ(restored.Identity()->fingerprint,ownFingerprint);
            FPS_EXPECT_TRUE(!restored.Listen(0,error));
            FPS_EXPECT_TRUE(restored.RemoveTrustedPeer(error)); // already removed is harmless
        }
    }
    {
        std::cout<<"FailedTrustFileRemovalReportsFailureAndKeepsSavedPeer\n";
        PeerVideoController a,b;std::string error;
        FPS_EXPECT_TRUE(a.Initialize(nullptr,root/"locked-a",error)&&b.Initialize(nullptr,root/"locked-b",error));
        if(a.Identity()&&b.Identity()) {
            FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error));
            const auto file=root/"locked-a"/"trusted.friendpair";
            const auto locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            FPS_EXPECT_TRUE(locked!=INVALID_HANDLE_VALUE);
            if(locked!=INVALID_HANDLE_VALUE){
                FPS_EXPECT_TRUE(!a.RemoveTrustedPeer(error)&&!error.empty());
                FPS_EXPECT_TRUE(a.TrustedPeer()&&std::filesystem::exists(file)&&!a.Snapshot().active);
                CloseHandle(locked);
            }
            FPS_EXPECT_TRUE(a.RemoveTrustedPeer(error));
            FPS_EXPECT_TRUE(!a.TrustedPeer()&&!std::filesystem::exists(file));
        }
    }
    std::filesystem::remove_all(root);
}
