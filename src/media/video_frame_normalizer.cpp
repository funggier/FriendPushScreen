#include "media/video_frame_normalizer.h"
#include <cstring>
namespace fps::media {
void VideoFrameNormalizer::Configure(ID3D11Device* device,std::uint32_t width,std::uint32_t height) {
    Stop();device_=device;width_=width;height_=height;if(device)device->GetImmediateContext(&context_);
}
void VideoFrameNormalizer::Stop(){pad_.Reset();context_.Reset();device_.Reset();width_=height_=0;}
bool VideoFrameNormalizer::Normalize(const capture::CapturedFrame& source,capture::CapturedFrame& frame,std::string& error) {
    if(!source.IsValid()||!width_||!height_||width_>4096||height_>4096||
        static_cast<std::uint64_t>(source.width)*source.height>3840ULL*2160||
        source.width>width_||source.height>height_||width_-source.width>1||height_-source.height>1) {
        error="Capture size changed or exceeds negotiated geometry";return false;
    }
    if(source.width==width_&&source.height==height_){frame=source;return true;}
    frame=source;frame.width=width_;frame.height=height_;frame.texture.Reset();frame.bgra.clear();frame.row_pitch=0;
    if(!source.HasGpuTexture()) {
        const auto pitch=source.row_pitch?source.row_pitch:source.width*4;
        if(pitch<source.width*4||static_cast<std::uint64_t>(pitch)*source.height>source.bgra.size()){error="Invalid padding CPU source";return false;}
        frame.row_pitch=width_*4;frame.bgra.assign(static_cast<std::size_t>(width_)*height_*4,0);
        for(std::uint32_t y=0;y<source.height;++y)std::memcpy(frame.bgra.data()+static_cast<std::size_t>(y)*frame.row_pitch,
            source.bgra.data()+static_cast<std::size_t>(y)*pitch,source.width*4);
        return true;
    }
    if(!device_||!context_){error="Padding requires D3D11 device";return false;}
    Microsoft::WRL::ComPtr<ID3D11Device> owner;source.texture->GetDevice(&owner);
    D3D11_TEXTURE2D_DESC desc{};source.texture->GetDesc(&desc);
    if(owner.Get()!=device_.Get()||desc.Width<source.width||desc.Height<source.height||
        desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||desc.SampleDesc.Count!=1){error="Invalid padding GPU source";return false;}
    if(!pad_) {
        desc={};desc.Width=width_;desc.Height=height_;desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc.Count=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        std::vector<std::uint8_t> black(static_cast<std::size_t>(width_)*height_*4,0);
        D3D11_SUBRESOURCE_DATA initial{black.data(),width_*4,0};
        if(FAILED(device_->CreateTexture2D(&desc,&initial,&pad_))){error="Padding texture failed";return false;}
    }
    const D3D11_BOX box{0,0,0,source.width,source.height,1};
    context_->CopySubresourceRegion(pad_.Get(),0,0,0,0,source.texture.Get(),0,&box);frame.texture=pad_;return true;
}
bool VideoFrameNormalizer::RestoreVisible(capture::CapturedFrame& frame,std::uint32_t width,std::uint32_t height,std::string& error) {
    if(!width||!height||frame.width<width||frame.height<height||frame.width-width>1||frame.height-height>1){error="Decoded visible size invalid";return false;}
    if(frame.width==width&&frame.height==height)return true;
    if(frame.HasGpuTexture()) {
        if(!device_||!context_){error="Visible crop requires D3D11";return false;}
        D3D11_TEXTURE2D_DESC desc{};frame.texture->GetDesc(&desc);desc.Width=width;desc.Height=height;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> cropped;
        if(FAILED(device_->CreateTexture2D(&desc,nullptr,&cropped))){error="Visible texture failed";return false;}
        const D3D11_BOX box{0,0,0,width,height,1};context_->CopySubresourceRegion(cropped.Get(),0,0,0,0,frame.texture.Get(),0,&box);
        frame.texture=cropped;
    } else {
        const auto pitch=frame.row_pitch?frame.row_pitch:frame.width*4;
        if(pitch<frame.width*4||static_cast<std::uint64_t>(pitch)*frame.height>frame.bgra.size()){error="Invalid decoded CPU layout";return false;}
        std::vector<std::uint8_t> cropped(static_cast<std::size_t>(width)*height*4);
        for(std::uint32_t y=0;y<height;++y)std::memcpy(cropped.data()+static_cast<std::size_t>(y)*width*4,
            frame.bgra.data()+static_cast<std::size_t>(y)*pitch,width*4);
        frame.bgra=std::move(cropped);frame.row_pitch=width*4;
    }
    frame.width=width;frame.height=height;return true;
}
}
