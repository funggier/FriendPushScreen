#include <d3d11.h>
#include <dxgi1_2.h>
#include <initguid.h>
#include <dxva.h>
#include <wrl/client.h>
#include <iostream>
#include <iomanip>
namespace {
const char* ProfileName(const GUID& g) {
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_H264_VLD_NOFGT)) return "H264 VLD";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_H264_VLD_FGT)) return "H264 VLD FGT";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_H264_VLD_WITHFMOASO_NOFGT)) return "H264 VLD FMOASO";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_HEVC_VLD_MAIN)) return "HEVC Main";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10)) return "HEVC Main10";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0)) return "VP9 Profile0";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2)) return "VP9 Profile2 10bit";
    if(IsEqualGUID(g,DXVA_ModeAV1_VLD_Profile0)) return "AV1 Profile0";
    if(IsEqualGUID(g,DXVA_ModeAV1_VLD_Profile1)) return "AV1 Profile1";
    if(IsEqualGUID(g,DXVA_ModeAV1_VLD_Profile2)) return "AV1 Profile2";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_MPEG2_VLD)) return "MPEG2 VLD";
    if(IsEqualGUID(g,D3D11_DECODER_PROFILE_VC1_D2010)) return "VC1 D2010";
    return "Other profile";
}
}
void ProbeD3D11DecoderInventory() {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT|D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr);
    if(FAILED(hr)){std::cout<<"D3D11 inventory unavailable hr="<<std::hex<<hr<<std::dec<<"\n";return;}
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter;
    if(SUCCEEDED(device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter))) {
        DXGI_ADAPTER_DESC desc{};adapter->GetDesc(&desc);
        char name[512]{};WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name,512,nullptr,nullptr);
        std::cout<<"D3D11 hardware adapter: "<<name<<" vendor=0x"<<std::hex<<desc.VendorId
            <<" device=0x"<<desc.DeviceId<<std::dec<<"\n";
    }
    ComPtr<ID3D11VideoDevice> video;
    if(FAILED(device.As(&video)))return;
    std::cout<<"D3D11 decoder profiles="<<video->GetVideoDecoderProfileCount()
        <<" (configuration/creation evidence; not a decoded-bitstream test)\n";
    for(UINT i=0;i<video->GetVideoDecoderProfileCount();++i) {
        GUID profile{};if(FAILED(video->GetVideoDecoderProfile(i,&profile)))continue;
        wchar_t wide[64]{};StringFromGUID2(profile,wide,64);
        char guid[128]{};WideCharToMultiByte(CP_UTF8,0,wide,-1,guid,128,nullptr,nullptr);
        std::cout<<"  "<<ProfileName(profile)<<" "<<guid;
        for(auto format:{DXGI_FORMAT_NV12,DXGI_FORMAT_P010}) {
            BOOL supported=FALSE;
            if(FAILED(video->CheckVideoDecoderFormat(&profile,format,&supported))||!supported)continue;
            D3D11_VIDEO_DECODER_DESC desc{profile,1920,1080,format};
            UINT count=0;video->GetVideoDecoderConfigCount(&desc,&count);
            bool created=false;
            for(UINT c=0;c<count;++c) {
                D3D11_VIDEO_DECODER_CONFIG config{};
                if(FAILED(video->GetVideoDecoderConfig(&desc,c,&config)))continue;
                ComPtr<ID3D11VideoDecoder> decoder;
                if(SUCCEEDED(video->CreateVideoDecoder(&desc,&config,&decoder))) {created=true;break;}
            }
            std::cout<<" "<<(format==DXGI_FORMAT_NV12?"NV12":"P010")<<" configs="<<count
                <<" create1080p="<<(created?"PASS":"FAIL");
        }
        std::cout<<"\n";
    }
}
