#include "network/peer_endpoint.h"
#include <algorithm>
namespace fps::network {
bool ParsePeerHost(std::string_view input,PeerHost& result,std::string& error) {
    result={};
    error="Enter an IPv4 address or DNS hostname; use Remote Port separately, without http:// or a path";
    if(input.empty()||input.size()>253)return false;
    for(unsigned char c:input)if(c<=32||c>=127)return false;
    const bool numeric=std::all_of(input.begin(),input.end(),[](char c){return (c>='0'&&c<='9')||c=='.';});
    if(numeric) {
        std::size_t start=0;
        for(std::size_t i=0;i<4;++i) {
            const auto end=input.find('.',start);
            if((i<3&&end==std::string_view::npos)||(i==3&&end!=std::string_view::npos))return false;
            const auto part=input.substr(start,end==std::string_view::npos?end:end-start);
            if(part.empty()||part.size()>3||(part.size()>1&&part.front()=='0'))return false;
            unsigned value=0;for(char c:part)value=value*10+static_cast<unsigned>(c-'0');
            if(value>255)return false;
            result.ipv4[i]=static_cast<std::uint8_t>(value);
            start=end==std::string_view::npos?input.size():end+1;
        }
        if(result.ipv4[0]==0||result.ipv4[0]>=224)return false;
        result.is_ipv4=true;
    } else {
        const auto length=input.size()-(input.back()=='.'?1:0);
        if(!length||input[length-1]=='.')return false;
        std::size_t start=0;
        while(start<length) {
            auto end=input.find('.',start);if(end==std::string_view::npos||end>length)end=length;
            const auto label=input.substr(start,end-start);
            if(label.empty()||label.size()>63||label.front()=='-'||label.back()=='-')return false;
            for(char c:label)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return false;
            start=end+1;
        }
        result.dns_name.assign(input);
        std::transform(result.dns_name.begin(),result.dns_name.end(),result.dns_name.begin(),
            [](unsigned char c){return static_cast<char>(c>='A'&&c<='Z'?c-'A'+'a':c);});
    }
    error.clear();return true;
}
}
