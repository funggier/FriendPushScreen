#include "test_support.h"
#include "network/peer_identity.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iterator>
void RunPeerIdentityTests() {
    using namespace fps::network;
    const auto root=std::filesystem::current_path()/("fps-peer-identity-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    std::filesystem::create_directories(root);
    std::string error;
    auto a=PeerIdentity::LoadOrCreate(root/"a",error);
    FPS_EXPECT_TRUE(a != nullptr);
    if(!a) { std::cerr<<"IDENTITY_CREATE_RED: "<<error<<'\n';std::filesystem::remove_all(root);return; }
    auto b=PeerIdentity::LoadOrCreate(root/"b",error);
    if(!b)std::cerr<<"SECOND_IDENTITY_FAILED: "<<error<<'\n';
    auto again=PeerIdentity::LoadOrCreate(root/"a",error);
    FPS_EXPECT_TRUE(b != nullptr);
    FPS_EXPECT_TRUE(again != nullptr);
    if(!again)std::cerr<<"RELOAD_IDENTITY_FAILED: "<<error<<'\n';
    if(!b || !again) {std::filesystem::remove_all(root);return;}
    FPS_EXPECT_EQ(a->Public().fingerprint,again->Public().fingerprint);
    FPS_EXPECT_TRUE(a->Public().fingerprint != b->Public().fingerprint);
    FPS_EXPECT_EQ(a->Public().fingerprint.size(),std::size_t{64});
    FPS_EXPECT_TRUE(!a->PfxBytes().empty());
    auto bytes=EncodePeerBundle(a->Public());
    FPS_EXPECT_TRUE(bytes.size() <= kMaxPairingBytes);
    auto parsed=ParsePeerBundle(bytes,error);
    FPS_EXPECT_TRUE(parsed.has_value());
    if(parsed) {
        FPS_EXPECT_EQ(parsed->certificate_der,a->Public().certificate_der);
        FPS_EXPECT_EQ(parsed->stable_name,a->Public().stable_name);
        FPS_EXPECT_EQ(parsed->fingerprint,a->Public().fingerprint);
    }
    FPS_EXPECT_TRUE(!ValidateTrustedPeer(a->Public(),a->Public(),error));
    FPS_EXPECT_TRUE(ValidateTrustedPeer(a->Public(),b->Public(),error));
    FPS_EXPECT_TRUE(WritePublicBundle(root/"a.friendpair",a->Public(),error));
    auto fromFile=ReadPublicBundle(root/"a.friendpair",error);
    FPS_EXPECT_TRUE(fromFile && fromFile->fingerprint == a->Public().fingerprint);
    auto damaged=bytes;damaged[0]^=0xFF;
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    damaged=bytes;damaged.push_back(0);
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    damaged=bytes;damaged.resize(10);
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    damaged.assign(kMaxPairingBytes+1,0);
    FPS_EXPECT_TRUE(!ParsePeerBundle(damaged,error));
    const auto stored=root/"a"/"identity.dpapi";
    std::ifstream input(stored,std::ios::binary);
    std::vector<std::uint8_t> protectedBytes((std::istreambuf_iterator<char>(input)),{});
    input.close();
    FPS_EXPECT_TRUE(!protectedBytes.empty());
    FPS_EXPECT_TRUE(protectedBytes != std::vector<std::uint8_t>(a->PfxBytes().begin(),a->PfxBytes().end()));
    std::ofstream(stored,std::ios::binary|std::ios::trunc)<<"damaged persisted identity";
    FPS_EXPECT_TRUE(!PeerIdentity::LoadOrCreate(root/"a",error));
    std::ifstream after(stored,std::ios::binary);std::string unchanged((std::istreambuf_iterator<char>(after)),{});after.close();
    FPS_EXPECT_EQ(unchanged,std::string{"damaged persisted identity"});
    std::filesystem::remove_all(root);
}
