#include "media/video_peer_stream.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
using namespace fps;
int main() {
    std::cout<<std::unitbuf;
    Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 2;
    network::VideoFormat format{1920,1080,1920,1080};media::VideoCodecConfig config;config.width=1920;config.height=1080;
    media::VideoPeerSender sender;media::VideoPeerReceiver receiver;
    if(!sender.Initialize(device.Get(),format,config,media::VideoBackendPolicy::RequireHardware,123,1)){std::cerr<<sender.Error();return 2;}
    if(!receiver.Configure(device.Get(),format,config,media::VideoBackendPolicy::RequireHardware,123,1)){std::cerr<<receiver.Error();return 2;}
    D3D11_TEXTURE2D_DESC d{};d.Width=1920;d.Height=1080;d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    capture::CapturedFrame input;input.width=1920;input.height=1080;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
    if(FAILED(device->CreateTexture2D(&d,nullptr,&input.texture))||FAILED(device->CreateRenderTargetView(input.texture.Get(),nullptr,&target)))return 2;
    d.Width=d.Height=1;d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pixel;if(FAILED(device->CreateTexture2D(&d,nullptr,&pixel)))return 2;
    unsigned quality=0;std::uint64_t last=0;
    auto sink=[&](const capture::CapturedFrame& frame) {
        const D3D11_BOX box{960,540,0,961,541,1};context->CopySubresourceRegion(pixel.Get(),0,0,0,0,frame.texture.Get(),0,&box);
        D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(context->Map(pixel.Get(),0,D3D11_MAP_READ,0,&map)))return false;
        const auto* p=static_cast<const unsigned char*>(map.pData);const bool red=frame.sequence%2==1;
        if(std::abs(static_cast<int>(p[2])-(red?255:0))>20||std::abs(static_cast<int>(p[0])-(red?0:255))>20||p[1]>20)++quality;
        context->Unmap(pixel.Get(),0);last=frame.sequence;return true;
    };
    auto transmit=[&](network::VideoWireFrame&& wire) {
        if(wire.sequence<4)std::cout<<"unit="<<wire.sequence<<" key="<<wire.keyframe<<" parameter_sets="<<network::HasRandomAccessParameterSets(wire.bytes)<<" size="<<wire.bytes.size()<<'\n';
        return receiver.Push(std::move(wire),sink);
    };
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=1;i<=120;++i) {
        const float red[4]{1,0,0,1},blue[4]{0,0,1,1};context->ClearRenderTargetView(target.Get(),i%2?red:blue);
        input.sequence=i;input.timestamp_100ns=static_cast<std::int64_t>(i)*166667;
        if(!sender.Push(input,transmit)||!sender.Poll(transmit)||!receiver.Poll(sink)){std::cerr<<sender.Error()<<" / "<<receiver.Error()<<'\n';return 1;}
        std::this_thread::sleep_until(start+std::chrono::microseconds(static_cast<long long>(i)*1000000/60));
    }
    const auto until=GetTickCount64()+2000;
    while(GetTickCount64()<until&&receiver.Metrics().delivered<119){sender.Poll(transmit);receiver.Poll(sink);Sleep(1);}
    const auto& s=sender.Metrics();const auto& r=receiver.Metrics();
    std::cout<<"encoded="<<s.encoded<<" decoded="<<r.decoded<<" rendered="<<r.delivered<<" drops="<<r.dropped
        <<" pending_peak="<<r.max_pending<<" last="<<last<<" quality_fail="<<quality<<'\n';
    const bool ok=r.delivered>=119&&r.dropped==0&&quality==0;std::cout<<(ok?"PEER_MEDIA_RUNTIME_PASS":"PEER_MEDIA_RUNTIME_FAIL")<<'\n';return ok?0:1;
}
