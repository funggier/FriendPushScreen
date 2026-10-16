#include "media/video_peer_stream.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <array>
#include <charconv>
using namespace fps;
namespace {
bool Number(const char* text,std::uint32_t& value) {
    const std::string_view input{text};const auto [end,error]=std::from_chars(input.data(),input.data()+input.size(),value);
    return error==std::errc{}&&end==input.data()+input.size();
}
}
int main(int argc,char** argv) {
    std::cout<<std::unitbuf;
    std::uint32_t width=1920,height=1080,frame_count=120;
    bool cpu_encoder=false,cpu_decoder=false,low_latency=true;
    for(int i=1;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--buffered-encoder"){low_latency=false;continue;}
        if(i+1>=argc){std::cerr<<"Missing option value\n";return 2;}
        const char* value=argv[++i];
        if(option=="--encoder"||option=="--decoder") {
            const std::string_view mode=value;if(mode!="cpu"&&mode!="gpu"){std::cerr<<"Use cpu or gpu\n";return 2;}
            (option=="--encoder"?cpu_encoder:cpu_decoder)=mode=="cpu";
        }else if(option=="--width"){if(!Number(value,width))return 2;}
        else if(option=="--height"){if(!Number(value,height))return 2;}
        else if(option=="--frames"){if(!Number(value,frame_count))return 2;}
        else {std::cerr<<"Unknown option\n";return 2;}
    }
    if(width<32||height<32||width>4096||height>4096||frame_count<16||frame_count>600)return 2;
    network::VideoFormat format{(width+1)&~1U,(height+1)&~1U,width,height};format.low_latency=low_latency;
    if(!network::ValidateVideoFormat(format))return 2;
    Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 2;
    media::VideoCodecConfig config;config.width=format.coded_width;config.height=format.coded_height;config.low_latency=low_latency;
    media::VideoPeerSender sender;media::VideoPeerReceiver receiver;
    if(!sender.Initialize(device.Get(),format,config,cpu_encoder?media::VideoBackendPolicy::ForceSoftware:media::VideoBackendPolicy::RequireHardware,123,1)){
        std::cerr<<sender.Error();return 2;
    }
    if(!receiver.Configure(device.Get(),format,config,cpu_decoder?media::VideoBackendPolicy::ForceSoftware:media::VideoBackendPolicy::RequireHardware,123,1)){
        std::cerr<<receiver.Error();return 2;
    }
    D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    capture::CapturedFrame input;input.width=width;input.height=height;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
    if(FAILED(device->CreateTexture2D(&d,nullptr,&input.texture))||FAILED(device->CreateRenderTargetView(input.texture.Get(),nullptr,&target)))return 2;
    d.Width=d.Height=1;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pixel;if(FAILED(device->CreateTexture2D(&d,nullptr,&pixel)))return 2;
    unsigned quality=0,geometry=0;std::uint64_t last=0;
    const std::array<std::array<std::uint32_t,2>,5> points{{{width/2,height/2},{2,2},{width-3,2},{2,height-3},{width-3,height-3}}};
    auto sink=[&](const capture::CapturedFrame& frame) {
        if(!frame.texture||frame.width!=width||frame.height!=height||frame.sequence<=last){++geometry;return false;}
        D3D11_TEXTURE2D_DESC actual{};frame.texture->GetDesc(&actual);
        if(actual.Width!=width||actual.Height!=height){++geometry;return false;}
        for(const auto& point:points) {
            const D3D11_BOX box{point[0],point[1],0,point[0]+1,point[1]+1,1};
            context->CopySubresourceRegion(pixel.Get(),0,0,0,0,frame.texture.Get(),0,&box);
            D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context->Map(pixel.Get(),0,D3D11_MAP_READ,0,&map)))return false;
            const auto* p=static_cast<const unsigned char*>(map.pData);const bool red=frame.sequence%2==1;
            if(std::abs(static_cast<int>(p[2])-(red?255:0))>25||std::abs(static_cast<int>(p[0])-(red?0:255))>25||p[1]>25)++quality;
            context->Unmap(pixel.Get(),0);
        }
        last=frame.sequence;return true;
    };
    auto transmit=[&](network::VideoWireFrame&& wire) {
        if(wire.sequence<4)std::cout<<"unit="<<wire.sequence<<" key="<<wire.keyframe<<" parameter_sets="<<network::HasRandomAccessParameterSets(wire.bytes)<<" size="<<wire.bytes.size()<<'\n';
        return receiver.Push(std::move(wire),sink);
    };
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=1;i<=frame_count;++i) {
        const float red[4]{1,0,0,1},blue[4]{0,0,1,1};context->ClearRenderTargetView(target.Get(),i%2?red:blue);
        input.sequence=i;input.timestamp_100ns=static_cast<std::int64_t>(i)*10000000/60;
        if(!sender.Push(input,transmit)||!sender.Poll(transmit)||!receiver.Poll(sink)){std::cerr<<sender.Error()<<" / "<<receiver.Error()<<'\n';return 1;}
        std::this_thread::sleep_until(start+std::chrono::microseconds(static_cast<long long>(i)*1000000/60));
    }
    const auto until=GetTickCount64()+2000;
    while(GetTickCount64()<until&&receiver.Metrics().delivered+1<frame_count) {
        if(!sender.Poll(transmit)||!receiver.Poll(sink)){std::cerr<<sender.Error()<<" / "<<receiver.Error()<<'\n';return 1;}Sleep(1);
    }
    const auto& s=sender.Metrics();const auto& r=receiver.Metrics();const auto& encoder=sender.Info();const auto& decoder=receiver.Info();
    const bool backend=(cpu_encoder?encoder.cpu_frames>0&&encoder.gpu_frames==0:encoder.gpu_frames>0&&encoder.cpu_frames==0)&&
        (cpu_decoder?decoder.cpu_frames>0&&decoder.gpu_frames==0:decoder.gpu_frames>0&&decoder.cpu_frames==0);
    std::cout<<"visible="<<width<<'x'<<height<<" coded="<<format.coded_width<<'x'<<format.coded_height
        <<" encoder_low_latency="<<low_latency<<" decoder_realtime="<<decoder.low_latency_applied
        <<" encoder_gpu="<<encoder.gpu_frames<<" encoder_cpu="<<encoder.cpu_frames
        <<" decoder_gpu="<<decoder.gpu_frames<<" decoder_cpu="<<decoder.cpu_frames
        <<" encoded="<<s.encoded<<" decoded="<<r.decoded<<" rendered="<<r.delivered<<" drops="<<r.dropped
        <<" pending_peak="<<r.max_pending<<" last="<<last<<" quality_fail="<<quality<<" geometry_fail="<<geometry
        <<" pending_tail="<<(s.captured-s.encoded)<<'\n';
    // Native encoders may retain an un-drained tail; this functional probe reports it.
    const bool ok=r.delivered+8>=frame_count&&r.dropped==0&&quality==0&&geometry==0&&backend&&decoder.low_latency_applied&&r.max_pending<=2;
    std::cout<<(ok?"PEER_MEDIA_RUNTIME_PASS":"PEER_MEDIA_RUNTIME_FAIL")<<'\n';return ok?0:1;
}
