#include "graphics/d3d11_video_converter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace fps::graphics {
namespace {

bool SameDevice(
    ID3D11Texture2D* texture,
    ID3D11Device* expected) {
    if (texture == nullptr ||
        expected == nullptr) {
        return false;
    }

    ID3D11Device* raw = nullptr;
    texture->GetDevice(&raw);

    const bool same =
        raw == expected;

    if (raw != nullptr) {
        raw->Release();
    }

    return same;
}

bool TextureMatches(
    ID3D11Texture2D* texture,
    std::uint32_t width,
    std::uint32_t height,
    DXGI_FORMAT format) {
    if (texture == nullptr) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);

    return desc.Width == width &&
        desc.Height == height &&
        desc.Format == format &&
        desc.ArraySize == 1 &&
        desc.MipLevels == 1;
}

}  // namespace

bool D3D11VideoConverter::Initialize(
    ID3D11Device* device) {
    if (device == nullptr) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    device_ = device;
    device_->GetImmediateContext(
        &context_);

    if (context_ == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    HRESULT hr =
        device_.As(
            &video_device_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr =
        context_.As(
            &video_context_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool D3D11VideoConverter::CreateNv12Texture(
    std::uint32_t width,
    std::uint32_t height,
    Microsoft::WRL::ComPtr<
        ID3D11Texture2D>& texture) {
    const VideoConversionSize size{
        width,
        height};

    if (!size.IsValid() ||
        device_ == nullptr) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_RENDER_TARGET |
        D3D11_BIND_SHADER_RESOURCE;

    texture.Reset();

    const HRESULT hr =
        device_->CreateTexture2D(
            &desc,
            nullptr,
            &texture);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool D3D11VideoConverter::CreateBgraTexture(
    std::uint32_t width,
    std::uint32_t height,
    Microsoft::WRL::ComPtr<
        ID3D11Texture2D>& texture) {
    if (width == 0 ||
        height == 0 ||
        device_ == nullptr) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_RENDER_TARGET |
        D3D11_BIND_SHADER_RESOURCE;

    texture.Reset();

    const HRESULT hr =
        device_->CreateTexture2D(
            &desc,
            nullptr,
            &texture);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool D3D11VideoConverter::
EnsureBgraInputScratch(
    std::uint32_t width,
    std::uint32_t height) {
    if (bgra_input_scratch_ != nullptr &&
        bgra_scratch_width_ == width &&
        bgra_scratch_height_ == height) {
        return true;
    }

    if (!CreateBgraTexture(
            width,
            height,
            bgra_input_scratch_)) {
        return false;
    }

    bgra_scratch_width_ = width;
    bgra_scratch_height_ = height;
    return true;
}

bool D3D11VideoConverter::
EnsureNv12InputScratch(
    std::uint32_t width,
    std::uint32_t height) {
    if (nv12_input_scratch_ != nullptr &&
        nv12_scratch_width_ == width &&
        nv12_scratch_height_ == height) {
        return true;
    }

    if (!CreateNv12Texture(
            width,
            height,
            nv12_input_scratch_)) {
        return false;
    }

    nv12_scratch_width_ = width;
    nv12_scratch_height_ = height;
    return true;
}

bool D3D11VideoConverter::
CopyCapturedFrameToBgraScratch(
    const capture::CapturedFrame& frame) {
    if (!frame.IsValid() ||
        frame.pixel_format !=
            capture::CapturePixelFormat::Bgra8 ||
        !EnsureBgraInputScratch(
            frame.width,
            frame.height)) {
        if (last_error_ == S_OK) {
            last_error_ = E_INVALIDARG;
        }
        return false;
    }

    if (frame.HasGpuTexture()) {
        if (!SameDevice(
                frame.texture.Get(),
                device_.Get())) {
            last_error_ =
                DXGI_ERROR_INVALID_CALL;
            return false;
        }

        D3D11_TEXTURE2D_DESC desc{};
        frame.texture->GetDesc(&desc);

        if (desc.Width != frame.width ||
            desc.Height != frame.height ||
            desc.Format !=
                DXGI_FORMAT_B8G8R8A8_UNORM) {
            last_error_ = E_INVALIDARG;
            return false;
        }

        context_->CopySubresourceRegion(
            bgra_input_scratch_.Get(),
            0,
            0,
            0,
            0,
            frame.texture.Get(),
            0,
            nullptr);

        last_error_ = S_OK;
        return true;
    }

    if (!frame.HasCpuPixels()) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    const std::uint32_t row_pitch =
        frame.row_pitch != 0
            ? frame.row_pitch
            : frame.width * 4;

    const std::size_t required =
        static_cast<std::size_t>(
            row_pitch) *
        static_cast<std::size_t>(
            frame.height);

    if (row_pitch < static_cast<std::uint64_t>(frame.width) * 4 ||
        frame.bgra.size() < required) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    context_->UpdateSubresource(
        bgra_input_scratch_.Get(),
        0,
        nullptr,
        frame.bgra.data(),
        row_pitch,
        0);

    last_error_ = S_OK;
    return true;
}

bool D3D11VideoConverter::EnsureProcessor(
    std::uint32_t width,
    std::uint32_t height,
    DXGI_FORMAT input_format,
    DXGI_FORMAT output_format) {
    if (video_device_ == nullptr ||
        video_context_ == nullptr ||
        width == 0 ||
        height == 0) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    if (processor_ != nullptr &&
        processor_enumerator_ != nullptr &&
        processor_width_ == width &&
        processor_height_ == height &&
        processor_input_format_ ==
            input_format &&
        processor_output_format_ ==
            output_format) {
        return true;
    }

    processor_.Reset();
    processor_enumerator_.Reset();

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC
        content{};
    content.InputFrameFormat =
        D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content.InputFrameRate.Numerator = 60;
    content.InputFrameRate.Denominator = 1;
    content.InputWidth = width;
    content.InputHeight = height;
    content.OutputFrameRate.Numerator = 60;
    content.OutputFrameRate.Denominator = 1;
    content.OutputWidth = width;
    content.OutputHeight = height;
    content.Usage =
        D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr =
        video_device_->
            CreateVideoProcessorEnumerator(
                &content,
                &processor_enumerator_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    UINT input_support = 0;
    hr =
        processor_enumerator_->
            CheckVideoProcessorFormat(
                input_format,
                &input_support);

    if (FAILED(hr) ||
        (input_support &
         D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) ==
            0) {
        last_error_ =
            FAILED(hr)
                ? hr
                : DXGI_ERROR_UNSUPPORTED;
        return false;
    }

    UINT output_support = 0;
    hr =
        processor_enumerator_->
            CheckVideoProcessorFormat(
                output_format,
                &output_support);

    if (FAILED(hr) ||
        (output_support &
         D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) ==
            0) {
        last_error_ =
            FAILED(hr)
                ? hr
                : DXGI_ERROR_UNSUPPORTED;
        return false;
    }

    hr =
        video_device_->
            CreateVideoProcessor(
                processor_enumerator_.Get(),
                0,
                &processor_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    video_context_->
        VideoProcessorSetStreamFrameFormat(
            processor_.Get(),
            0,
            D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

    RECT rect{
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)};

    video_context_->
        VideoProcessorSetStreamSourceRect(
            processor_.Get(),
            0,
            TRUE,
            &rect);

    video_context_->
        VideoProcessorSetStreamDestRect(
            processor_.Get(),
            0,
            TRUE,
            &rect);

    video_context_->
        VideoProcessorSetOutputTargetRect(
            processor_.Get(),
            TRUE,
            &rect);

    // SDR BT.709: full-range desktop RGB <-> studio-range NV12.
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb{};
    rgb.YCbCr_Matrix = 1;
    rgb.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE yuv{};
    yuv.YCbCr_Matrix = 1;
    yuv.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
    video_context_->VideoProcessorSetStreamColorSpace(
        processor_.Get(), 0,
        input_format == DXGI_FORMAT_NV12 ? &yuv : &rgb);
    video_context_->VideoProcessorSetOutputColorSpace(
        processor_.Get(),
        output_format == DXGI_FORMAT_NV12 ? &yuv : &rgb);
    video_context_->VideoProcessorSetStreamAutoProcessingMode(
        processor_.Get(), 0, FALSE);
    processor_width_ = width;
    processor_height_ = height;
    processor_input_format_ =
        input_format;
    processor_output_format_ =
        output_format;
    last_error_ = S_OK;
    return true;
}

bool D3D11VideoConverter::Blit(
    ID3D11Texture2D* input,
    ID3D11Texture2D* output) {
    if (input == nullptr ||
        output == nullptr ||
        processor_ == nullptr ||
        processor_enumerator_ == nullptr) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC
        input_desc{};
    input_desc.FourCC = 0;
    input_desc.ViewDimension =
        D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_desc.Texture2D.MipSlice = 0;
    input_desc.Texture2D.ArraySlice = 0;

    Microsoft::WRL::ComPtr<
        ID3D11VideoProcessorInputView>
        input_view;

    HRESULT hr =
        video_device_->
            CreateVideoProcessorInputView(
                input,
                processor_enumerator_.Get(),
                &input_desc,
                &input_view);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC
        output_desc{};
    output_desc.ViewDimension =
        D3D11_VPOV_DIMENSION_TEXTURE2D;
    output_desc.Texture2D.MipSlice = 0;

    Microsoft::WRL::ComPtr<
        ID3D11VideoProcessorOutputView>
        output_view;

    hr =
        video_device_->
            CreateVideoProcessorOutputView(
                output,
                processor_enumerator_.Get(),
                &output_desc,
                &output_view);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface =
        input_view.Get();

    hr =
        video_context_->
            VideoProcessorBlt(
                processor_.Get(),
                output_view.Get(),
                0,
                1,
                &stream);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool D3D11VideoConverter::
ConvertBgraFrameToNv12(
    const capture::CapturedFrame& frame,
    ID3D11Texture2D* output_nv12) {
    if (!frame.IsValid() ||
        output_nv12 == nullptr ||
        !TextureMatches(
            output_nv12,
            frame.width,
            frame.height,
            DXGI_FORMAT_NV12) ||
        !SameDevice(
            output_nv12,
            device_.Get())) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    const VideoConversionSize size{
        frame.width,
        frame.height};

    if (!size.IsValid() ||
        !CopyCapturedFrameToBgraScratch(
            frame) ||
        !EnsureProcessor(
            frame.width,
            frame.height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_NV12)) {
        return false;
    }

    return Blit(
        bgra_input_scratch_.Get(),
        output_nv12);
}

bool D3D11VideoConverter::
ConvertNv12TextureToBgra(
    ID3D11Texture2D* input_nv12,
    UINT input_subresource,
    std::uint32_t width,
    std::uint32_t height,
    ID3D11Texture2D* output_bgra) {
    if (input_nv12 == nullptr ||
        output_bgra == nullptr ||
        !VideoConversionSize{
            width,
            height}.IsValid() ||
        !TextureMatches(
            output_bgra,
            width,
            height,
            DXGI_FORMAT_B8G8R8A8_UNORM) ||
        !SameDevice(
            input_nv12,
            device_.Get()) ||
        !SameDevice(
            output_bgra,
            device_.Get()) ||
        !EnsureNv12InputScratch(
            width,
            height)) {
        if (last_error_ == S_OK) {
            last_error_ = E_INVALIDARG;
        }
        return false;
    }

    D3D11_TEXTURE2D_DESC input_desc{};
    input_nv12->GetDesc(&input_desc);

    if (input_desc.Width < width ||
        input_desc.Height < height ||
        input_desc.MipLevels != 1 ||
        input_desc.SampleDesc.Count != 1 ||
        input_subresource >= input_desc.ArraySize ||
        input_desc.Format != DXGI_FORMAT_NV12) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    const D3D11_BOX visible{0, 0, 0, width, height, 1};
    context_->CopySubresourceRegion(
        nv12_input_scratch_.Get(),
        0,
        0,
        0,
        0,
        input_nv12,
        input_subresource,
        &visible);

    if (!EnsureProcessor(
            width,
            height,
            DXGI_FORMAT_NV12,
            DXGI_FORMAT_B8G8R8A8_UNORM)) {
        return false;
    }

    return Blit(
        nv12_input_scratch_.Get(),
        output_bgra);
}

}  // namespace fps::graphics
