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
        a.Disconnect();b.Disconnect();
    }
    std::filesystem::remove_all(root);
}
