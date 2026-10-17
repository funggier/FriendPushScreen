#include "network/peer_endpoint.h"
#include "test_support.h"
void RunPeerEndpointTests() {
    using namespace fps::network;
    std::cout<<"PeerHostSyntaxSeparatesRoutesFromCertificateIdentity\n";
    PeerHost host;std::string error;
    FPS_EXPECT_TRUE(ParsePeerHost("192.168.1.165",host,error));
    FPS_EXPECT_TRUE(host.is_ipv4);
    FPS_EXPECT_EQ(host.ipv4[0],192);FPS_EXPECT_EQ(host.ipv4[3],165);
    FPS_EXPECT_TRUE(ParsePeerHost("127.0.0.2",host,error));
    FPS_EXPECT_TRUE(ParsePeerHost("100.73.92.109",host,error));
    FPS_EXPECT_TRUE(ParsePeerHost("CDLOOT.THDDNS.NET.",host,error));
    FPS_EXPECT_TRUE(!host.is_ipv4);
    FPS_EXPECT_EQ(host.dns_name,std::string("cdloot.thddns.net."));
    FPS_EXPECT_TRUE(ParsePeerHost("localhost",host,error));
    FPS_EXPECT_TRUE(ParsePeerHost("workstation-1",host,error));
    for(const std::string value:{"","http://cdloot.thddns.net:8996","cdloot.thddns.net:8996",
        "cdloot.thddns.net/path","a b","a\\b","a@b","a..b","-a.example","a-.example",
        "0.0.0.0","0.1.2.3","224.0.0.1","255.255.255.255","999.1.2.3","127.01.0.1","127.0.0","::1"}) {
        FPS_EXPECT_TRUE(!ParsePeerHost(value,host,error));
        FPS_EXPECT_TRUE(!error.empty());
    }
    FPS_EXPECT_TRUE(!ParsePeerHost(std::string("a\0.example",10),host,error));
    FPS_EXPECT_TRUE(!ParsePeerHost(std::string(64,'a')+".example",host,error));
    FPS_EXPECT_TRUE(!ParsePeerHost(std::string(254,'a'),host,error));
}
