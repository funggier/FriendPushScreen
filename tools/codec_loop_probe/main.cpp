#include "media/mf_video_codec.h"
#include "media/video_frame_clock.h"
#include "media/encoded_access_unit_queue.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <thread>
using Microsoft::WRL::ComPtr;
using namespace fps;
int main(int argc,char** argv) {
    bool software_encode=false,software_decode=false; unsigned frames=120,width=1920,height=1080;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg=="--software") software_encode=software_decode=true;
        else if(arg=="--software-encode") software_encode=true;
        else if(arg=="--software-decode") software_decode=true;
        else if(arg=="--frames" && i+1<argc) frames=static_cast<unsigned>(std::stoul(argv[++i]));
        else if(arg=="--width" && i+1<argc) width=static_cast<unsigned>(std::stoul(argv[++i]));
        else if(arg=="--height" && i+1<argc) height=static_cast<unsigned>(std::stoul(argv[++i]));
        else { std::cerr<<"Unknown argument "<<arg<<"\n"; return 2; }
    }
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_VIDEO_SUPPORT,nullptr,0,
        D3D11_SDK_VERSION,&device,nullptr,&context))) return 2;
    media::MftVideoEncoder encoder; media::MftVideoDecoder decoder;
    media::VideoCodecConfig config; config.width=width; config.height=height;
    config.keyframe_interval_frames=60;
    if(!encoder.Initialize(device.Get(),config,software_encode ? media::VideoBackendPolicy::ForceSoftware :
        media::VideoBackendPolicy::RequireHardware)) { std::cerr<<"ENCODER "<<encoder.Error()<<"\n"; return 1; }
    if(!decoder.Initialize(device.Get(),config,software_decode)) { std::cerr<<"DECODER "<<decoder.Error()<<"\n"; return 1; }
    std::cout<<"encoder="<<encoder.Info().candidate.name<<" async="<<encoder.Info().asynchronous
        <<" surface_io="<<encoder.Info().surface_io<<" low_latency="<<encoder.Info().low_latency_applied<<"\n";
    std::cout<<"decoder="<<decoder.Info().candidate.name<<" catalog_class=software"
        <<" surface_io="<<decoder.Info().surface_io<<" low_latency="<<decoder.Info().low_latency_applied<<"\n";
    capture::CapturedFrame source; source.width=width; source.height=height;
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=width; desc.Height=height;
    desc.MipLevels=1; desc.ArraySize=1; desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM; desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11RenderTargetView> target;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&source.texture)) ||
       FAILED(device->CreateRenderTargetView(source.texture.Get(),nullptr,&target))) return 2;
    desc.Width=1;desc.Height=1;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> pixel;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&pixel))) return 2;
    media::VideoFrameClock clock(60,1); media::BoundedEncodedQueue queue(2);
    unsigned encoded=0,decoded=0,keyframes=0,quality_fail=0; std::uint64_t total_bytes=0;
    LONGLONG last_pts=-1; bool forced_key=false;
    auto consume=[&](std::vector<capture::CapturedFrame>& output) {
        for(auto& frame:output) {
            ++decoded;
            if(frame.width!=width || frame.height!=height || frame.sequence!=decoded ||
                frame.timestamp_100ns<=last_pts) { std::cerr<<"METADATA FAIL seq="<<frame.sequence<<" expected="<<decoded<<"\n"; return false; }
            last_pts=frame.timestamp_100ns;
            D3D11_BOX box{width/2,height/2,0,width/2+1,height/2+1,1};
            context->CopySubresourceRegion(pixel.Get(),0,0,0,0,frame.texture.Get(),0,&box);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(context->Map(pixel.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
            const auto* p=static_cast<const unsigned char*>(mapped.pData);
            bool red=(frame.sequence%2)==1;
            if(std::abs(static_cast<int>(p[2])-(red?255:0))>20 ||
               std::abs(static_cast<int>(p[0])-(red?0:255))>20 || p[1]>20) ++quality_fail;
            context->Unmap(pixel.Get(),0);
        }
        return true;
    };
    auto transmit=[&](std::vector<media::EncodedAccessUnit>& units) {
        for(auto& unit:units) {
            ++encoded; total_bytes+=unit.bytes.size();
            if(unit.keyframe) { ++keyframes; if(unit.sequence>=61) forced_key=true; }
            if(unit.source_sequence!=unit.sequence) return false;
            queue.Push(std::move(unit));
            while(auto next=queue.Pop()) {
                std::vector<capture::CapturedFrame> output;
                if(!decoder.Decode(*next,output)) { std::cerr<<"DECODE "<<decoder.Error()<<"\n"; return false; }
                if(!consume(output)) return false;
            }
        }
        return true;
    };
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=1;i<=frames;++i) {
        if(i==61 && !encoder.RequestKeyframe()) {std::cerr<<encoder.Error()<<"\n";return 1;}
        const float red[4]{1,0,0,1},blue[4]{0,0,1,1};
        context->ClearRenderTargetView(target.Get(),i%2 ? red:blue);
        source.sequence=i;source.timestamp_100ns=static_cast<LONGLONG>(i)*166667;
        std::vector<media::EncodedAccessUnit> units;
        if(!encoder.Encode(source,clock.Normalize(i,source.timestamp_100ns),units)) {
            std::cerr<<"ENCODE frame="<<i<<" "<<encoder.Error()<<"\n";return 1;
        }
        if(!transmit(units)) return 1;
        std::this_thread::sleep_until(start+std::chrono::microseconds(static_cast<long long>(i)*1000000/60));
    }
    std::vector<media::EncodedAccessUnit> tail;
    if(!encoder.Drain(tail)) {std::cerr<<"ENCODER DRAIN "<<encoder.Error()<<"\n";return 1;}
    if(!transmit(tail))return 1;
    std::vector<capture::CapturedFrame> decoded_tail;
    if(!decoder.Drain(decoded_tail)) {std::cerr<<"DECODER DRAIN "<<decoder.Error()<<"\n";return 1;}
    if(!consume(decoded_tail))return 1;
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"RESULT input="<<frames<<" encoded="<<encoded<<" decoded="<<decoded<<" keyframes="<<keyframes
        <<" quality_fail="<<quality_fail<<" queue_max="<<queue.Stats().max_depth
        <<" drops="<<queue.Stats().dropped<<" seconds="<<seconds<<" fps="<<decoded/seconds
        <<" bytes="<<total_bytes<<" encoder_gpu="<<encoder.Info().gpu_frames<<" decoder_gpu="<<decoder.Info().gpu_frames
        <<" encoder_cpu="<<encoder.Info().cpu_frames<<" decoder_cpu="<<decoder.Info().cpu_frames<<"\n";
    bool pass=encoded==frames && decoded==frames && quality_fail==0 && keyframes>0 &&
        (frames<61 || forced_key) && queue.Stats().dropped==0 &&
        (software_encode ? encoder.Info().gpu_frames==0 : encoder.Info().gpu_frames==frames) &&
        (software_decode ? decoder.Info().gpu_frames==0 : decoder.Info().gpu_frames==frames);
    std::cout<<(pass?"SYNTHETIC_LOOP_PASS":"SYNTHETIC_LOOP_FAIL")<<"\n";
    return pass?0:1;
}
