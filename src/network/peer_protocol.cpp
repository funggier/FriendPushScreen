#include "network/peer_protocol.h"
#include <algorithm>
#include <bit>
#include <limits>
namespace fps::network {
namespace {
struct Writer {
    std::vector<std::uint8_t> bytes;
    void U8(std::uint8_t n){bytes.push_back(n);}
    void U16(std::uint16_t n){U8(static_cast<std::uint8_t>(n>>8));U8(static_cast<std::uint8_t>(n));}
    void U32(std::uint32_t n){for(int i=24;i>=0;i-=8)U8(static_cast<std::uint8_t>(n>>i));}
    void U64(std::uint64_t n){for(int i=56;i>=0;i-=8)U8(static_cast<std::uint8_t>(n>>i));}
    void Format(const VideoFormat& f){U32(f.coded_width);U32(f.coded_height);U32(f.visible_width);U32(f.visible_height);
        U32(f.fps_numerator);U32(f.fps_denominator);U32(f.bitrate);U32(f.gop);U32(f.low_latency?1U:0U);}
    void Header(char kind,std::uint16_t flags,std::uint32_t size){U8('F');U8('P');U8(static_cast<std::uint8_t>(kind));U8('1');U16(1);U16(flags);U32(size);}
};
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t offset{};bool valid{true};
    std::uint8_t U8(){if(offset>=bytes.size()){valid=false;return 0;}return bytes[offset++];}
    std::uint16_t U16(){std::uint16_t n=U8();return static_cast<std::uint16_t>((n<<8)|U8());}
    std::uint32_t U32(){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n=(n<<8)|U8();return n;}
    std::uint64_t U64(){std::uint64_t n=0;for(unsigned i=0;i<8;++i)n=(n<<8)|U8();return n;}
    VideoFormat Format(){VideoFormat f;f.coded_width=U32();f.coded_height=U32();f.visible_width=U32();f.visible_height=U32();
        f.fps_numerator=U32();f.fps_denominator=U32();f.bitrate=U32();f.gop=U32();const auto flags=U32();
        f.low_latency=(flags&1)!=0;if(flags>1)valid=false;return f;}
    bool Header(char kind,std::uint16_t& flags,std::size_t maximum) {
        if(bytes.size()<12||bytes.size()>maximum)return false;
        const bool magic=U8()=='F'&&U8()=='P'&&U8()==kind&&U8()=='1';
        const auto version=U16();flags=U16();const auto size=U32();
        return magic&&version==1&&size==bytes.size()&&valid;
    }
};
bool TextValid(const std::string& text){return text.size()<=1024&&std::all_of(text.begin(),text.end(),[](unsigned char c){return c>=32&&c<127;});}
bool CursorValid(const CursorRecord& c) {
    if(!c.visible)return !c.x&&!c.y&&!c.width&&!c.height&&!c.row_pitch&&c.bgra.empty();
    return c.width&&c.height&&c.width<=256&&c.height<=256&&c.row_pitch==c.width*4&&
        c.bgra.size()==static_cast<std::uint64_t>(c.row_pitch)*c.height&&
        c.x>=-4096&&c.x<=4096&&c.y>=-4096&&c.y<=4096;
}
bool FrameValid(const VideoWireFrame& f) {
    return f.session&&f.stream==1&&f.generation&&f.sequence&&f.timestamp_100ns>=0&&f.capture_time_100ns>=0&&
        f.duration_100ns>0&&f.duration_100ns<=10'000'000&&ValidateVideoFormat(f.format)&&CursorValid(f.cursor)&&
        !f.bytes.empty()&&f.bytes.size()<=kMaxMediaBytes-136&&f.cursor.bgra.size()<=kMaxMediaBytes-136-f.bytes.size();
}
}
bool ValidateVideoFormat(const VideoFormat& f) {
    return f.coded_width&&f.coded_height&&f.coded_width<=4096&&f.coded_height<=4096&&
        f.coded_width%2==0&&f.coded_height%2==0&&f.visible_width&&f.visible_height&&
        f.visible_width<=f.coded_width&&f.visible_height<=f.coded_height&&
        f.coded_width-f.visible_width<=1&&f.coded_height-f.visible_height<=1&&
        static_cast<std::uint64_t>(f.visible_width)*f.visible_height<=3840ULL*2160&&
        f.fps_numerator&&f.fps_numerator<=120000&&f.fps_denominator&&f.fps_denominator<=1001&&
        static_cast<std::uint64_t>(f.fps_numerator)<=120ULL*f.fps_denominator&&
        f.bitrate>=256000&&f.bitrate<=200000000&&f.gop&&f.gop<=600;
}
std::vector<std::uint8_t> EncodeControl(const ControlMessage& m) {
    const auto type=static_cast<std::uint8_t>(m.type);
    if(type<1||type>9||!m.session||m.stream!=1)return {};
    std::size_t payload=0;
    if(m.type==ControlType::Hello){if(m.capabilities&~3U)return {};payload=8;}
    if(m.type==ControlType::VideoFormat){if(!ValidateVideoFormat(m.format))return {};payload=36;}
    if(m.type==ControlType::Error){if(!TextValid(m.text))return {};payload=m.text.size();}
    Writer w;w.Header('C',static_cast<std::uint16_t>(type<<8),static_cast<std::uint32_t>(40+payload));
    w.U64(m.session);w.U64(m.generation);w.U64(m.value);w.U32(m.stream);
    if(m.type==ControlType::Hello){w.U32(m.capabilities);w.U16(m.listen_port);w.U16(0);}
    if(m.type==ControlType::VideoFormat)w.Format(m.format);
    if(m.type==ControlType::Error)w.bytes.insert(w.bytes.end(),m.text.begin(),m.text.end());
    return std::move(w.bytes);
}
std::optional<ControlMessage> DecodeControl(std::span<const std::uint8_t> bytes,std::string& error) {
    error="Invalid control protocol record";Reader r{bytes};std::uint16_t flags=0;
    if(!r.Header('C',flags,kMaxControlBytes)||(flags&255)!=0||(flags>>8)<1||(flags>>8)>9||bytes.size()<40)return std::nullopt;
    ControlMessage m;m.type=static_cast<ControlType>(flags>>8);m.session=r.U64();m.generation=r.U64();m.value=r.U64();m.stream=r.U32();
    if(m.type==ControlType::Hello){m.capabilities=r.U32();m.listen_port=r.U16();if(r.U16()!=0)r.valid=false;}
    if(m.type==ControlType::VideoFormat)m.format=r.Format();
    if(m.type==ControlType::Error){m.text.assign(reinterpret_cast<const char*>(bytes.data()+r.offset),bytes.size()-r.offset);r.offset=bytes.size();}
    if(!r.valid||r.offset!=bytes.size()||EncodeControl(m)!=std::vector<std::uint8_t>(bytes.begin(),bytes.end()))return std::nullopt;
    error.clear();return m;
}
std::vector<std::uint8_t> EncodeVideo(const VideoWireFrame& f) {
    if(!FrameValid(f))return {};
    Writer w;w.Header('V',static_cast<std::uint16_t>((f.keyframe?1:0)|(f.cursor.visible?2:0)),
        static_cast<std::uint32_t>(136+f.bytes.size()+f.cursor.bgra.size()));
    w.U64(f.session);w.U32(f.stream);w.U64(f.generation);w.U64(f.sequence);w.U64(f.source_sequence);
    w.U64(static_cast<std::uint64_t>(f.timestamp_100ns));w.U64(static_cast<std::uint64_t>(f.duration_100ns));
    w.U64(static_cast<std::uint64_t>(f.capture_time_100ns));w.Format(f.format);
    w.U32(std::bit_cast<std::uint32_t>(f.cursor.x));w.U32(std::bit_cast<std::uint32_t>(f.cursor.y));
    w.U32(f.cursor.width);w.U32(f.cursor.height);w.U32(f.cursor.row_pitch);w.U32(static_cast<std::uint32_t>(f.cursor.bgra.size()));
    w.U32(static_cast<std::uint32_t>(f.bytes.size()));
    w.bytes.insert(w.bytes.end(),f.cursor.bgra.begin(),f.cursor.bgra.end());w.bytes.insert(w.bytes.end(),f.bytes.begin(),f.bytes.end());
    return std::move(w.bytes);
}
std::optional<VideoWireFrame> DecodeVideo(std::span<const std::uint8_t> bytes,std::string& error) {
    error="Invalid video protocol record";Reader r{bytes};std::uint16_t flags=0;
    if(!r.Header('V',flags,kMaxMediaBytes)||(flags&~3U)!=0||bytes.size()<136)return std::nullopt;
    VideoWireFrame f;f.keyframe=(flags&1)!=0;f.cursor.visible=(flags&2)!=0;
    f.session=r.U64();f.stream=r.U32();f.generation=r.U64();f.sequence=r.U64();f.source_sequence=r.U64();
    const auto timestamp=r.U64(),duration=r.U64(),captured=r.U64();
    if(timestamp>INT64_MAX||duration>INT64_MAX||captured>INT64_MAX)return std::nullopt;
    f.timestamp_100ns=static_cast<std::int64_t>(timestamp);f.duration_100ns=static_cast<std::int64_t>(duration);
    f.capture_time_100ns=static_cast<std::int64_t>(captured);f.format=r.Format();
    f.cursor.x=std::bit_cast<std::int32_t>(r.U32());f.cursor.y=std::bit_cast<std::int32_t>(r.U32());
    f.cursor.width=r.U32();f.cursor.height=r.U32();f.cursor.row_pitch=r.U32();const auto cursorBytes=r.U32(),encodedBytes=r.U32();
    if(!r.valid||r.offset!=136||!ValidateVideoFormat(f.format)||cursorBytes>256*256*4||
        !encodedBytes||static_cast<std::uint64_t>(cursorBytes)+encodedBytes!=bytes.size()-136)return std::nullopt;
    if(f.cursor.visible) {
        if(!f.cursor.width||!f.cursor.height||f.cursor.width>256||f.cursor.height>256||f.cursor.row_pitch!=f.cursor.width*4||
            cursorBytes!=static_cast<std::uint64_t>(f.cursor.row_pitch)*f.cursor.height)return std::nullopt;
    } else if(cursorBytes||f.cursor.width||f.cursor.height||f.cursor.row_pitch||f.cursor.x||f.cursor.y)return std::nullopt;
    f.cursor.bgra.assign(bytes.begin()+136,bytes.begin()+136+cursorBytes);f.bytes.assign(bytes.begin()+136+cursorBytes,bytes.end());
    if(!FrameValid(f))return std::nullopt;error.clear();return f;
}
bool HasRandomAccessParameterSets(std::span<const std::uint8_t> bytes) {
    bool sps=false,pps=false,idr=false;
    for(std::size_t i=0;i+3<bytes.size();++i) {
        std::size_t nal=0;
        if(bytes[i]==0&&bytes[i+1]==0&&bytes[i+2]==1)nal=i+3;
        else if(i+4<bytes.size()&&bytes[i]==0&&bytes[i+1]==0&&bytes[i+2]==0&&bytes[i+3]==1)nal=i+4;
        if(nal&&nal<bytes.size()){const auto type=bytes[nal]&31U;sps|=type==7;pps|=type==8;idr|=type==5;i=nal;}
    }return sps&&pps&&idr;
}
}
