#include "graphics/d3d11_video_converter.h"
#include "test_support.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cmath>
#include <cstdint>
#include <iostream>
using Microsoft::WRL::ComPtr;
int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(hr)) { std::cerr << "D3D11 unavailable 0x" << std::hex << hr; return 2; }
    fps::graphics::D3D11VideoConverter converter;
    FPS_EXPECT_TRUE(converter.Initialize(device.Get()));
    constexpr UINT width=128, height=72;
    fps::capture::CapturedFrame frame{};
    frame.width=width; frame.height=height; frame.row_pitch=width*4;
    frame.bgra.resize(width*height*4);
    for (size_t i=0; i<frame.bgra.size(); i+=4) { frame.bgra[i+2]=255; frame.bgra[i+3]=255; }
    ComPtr<ID3D11Texture2D> nv12, bgra;
    FPS_EXPECT_TRUE(converter.CreateNv12Texture(width,height,nv12));
    FPS_EXPECT_TRUE(converter.CreateBgraTexture(width,height,bgra));
    FPS_EXPECT_TRUE(converter.ConvertBgraFrameToNv12(frame,nv12.Get()));
    D3D11_TEXTURE2D_DESC desc{}; nv12->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    FPS_EXPECT_TRUE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)));
    context->CopyResource(readback.Get(),nv12.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    FPS_EXPECT_TRUE(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)));
    const int luma=static_cast<unsigned char*>(mapped.pData)[36*mapped.RowPitch+64];
    context->Unmap(readback.Get(),0);
    std::cout << "BT709 red luma=" << luma << " expected=63\n";
    FPS_EXPECT_TRUE(std::abs(luma-63)<=2);
    FPS_EXPECT_TRUE(converter.ConvertNv12TextureToBgra(nv12.Get(),0,width,height,bgra.Get()));
    frame.row_pitch=width*4-1;
    FPS_EXPECT_TRUE(!converter.ConvertBgraFrameToNv12(frame,nv12.Get()));
    frame.row_pitch=width*4;
    // Decoders may expose macroblock-aligned arrays; the requested visible rectangle is smaller.
    nv12->GetDesc(&desc); desc.Height=80; desc.ArraySize=2;
    ComPtr<ID3D11Texture2D> padded;
    FPS_EXPECT_TRUE(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&padded)));
    FPS_EXPECT_TRUE(converter.ConvertNv12TextureToBgra(padded.Get(),1,width,height,bgra.Get()));
    FPS_EXPECT_TRUE(!converter.ConvertNv12TextureToBgra(padded.Get(),2,width,height,bgra.Get()));
    std::cout << "CONVERSION_FAILURES=" << fps::test::FailureCount() << "\n";
    return fps::test::FailureCount()==0 ? 0 : 1;
}
