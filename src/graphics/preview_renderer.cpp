#include "graphics/preview_renderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace fps::graphics {
namespace {

constexpr const char* kVertexShaderSource = R"(
struct VSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut main(uint vertex_id : SV_VertexID) {
    VSOut output;

    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 uvs[3] = {
        float2(0.0, 1.0),
        float2(0.0, -1.0),
        float2(2.0, 1.0)
    };

    output.position =
        float4(positions[vertex_id], 0.0, 1.0);
    output.uv = uvs[vertex_id];
    return output;
}
)";

constexpr const char* kPixelShaderSource = R"(
Texture2D preview_texture : register(t0);
SamplerState preview_sampler : register(s0);

float4 main(
    float4 position : SV_Position,
    float2 uv : TEXCOORD0) : SV_Target {
    return preview_texture.Sample(
        preview_sampler,
        uv);
}
)";

bool CompileShader(
    const char* source,
    const char* target,
    Microsoft::WRL::ComPtr<ID3DBlob>& blob,
    HRESULT& error) {
    Microsoft::WRL::ComPtr<ID3DBlob> messages;

    error = D3DCompile(
        source,
        std::char_traits<char>::length(source),
        nullptr,
        nullptr,
        nullptr,
        "main",
        target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &blob,
        &messages);

    return SUCCEEDED(error);
}

}  // namespace

PreviewFit CalculateAspectFit(
    std::uint32_t view_width,
    std::uint32_t view_height,
    std::uint32_t frame_width,
    std::uint32_t frame_height) {
    if (view_width == 0 ||
        view_height == 0 ||
        frame_width == 0 ||
        frame_height == 0) {
        return {};
    }

    const double scale_x =
        static_cast<double>(view_width) /
        static_cast<double>(frame_width);
    const double scale_y =
        static_cast<double>(view_height) /
        static_cast<double>(frame_height);
    const double scale =
        std::min(scale_x, scale_y);

    const float width =
        static_cast<float>(
            static_cast<double>(
                frame_width) *
            scale);
    const float height =
        static_cast<float>(
            static_cast<double>(
                frame_height) *
            scale);

    return PreviewFit{
        (static_cast<float>(view_width) -
         width) /
            2.0F,
        (static_cast<float>(view_height) -
         height) /
            2.0F,
        width,
        height,
        scale
    };
}

bool PreviewRenderer::Initialize(
    HWND hwnd,
    const RendererCreationPolicy& policy) {
    if (!device_.Initialize(hwnd, policy)) {
        last_error_ = device_.LastError();
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::Resize(
    UINT width,
    UINT height) {
    view_width_ = width;
    view_height_ = height;

    if (!device_.Resize(width, height)) {
        last_error_ = device_.LastError();
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::RenderPlaceholder() {
    if (!device_.PresentPlaceholder()) {
        last_error_ = device_.LastError();
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::EnsurePipeline() {
    if (vertex_shader_ != nullptr &&
        pixel_shader_ != nullptr &&
        sampler_ != nullptr &&
        rasterizer_ != nullptr &&
        cursor_blend_ != nullptr) {
        return true;
    }

    ID3D11Device* device = device_.Device();
    if (device == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob>
        vertex_blob;
    Microsoft::WRL::ComPtr<ID3DBlob>
        pixel_blob;

    HRESULT hr = S_OK;
    if (!CompileShader(
            kVertexShaderSource,
            "vs_4_0",
            vertex_blob,
            hr)) {
        last_error_ = hr;
        return false;
    }

    if (!CompileShader(
            kPixelShaderSource,
            "ps_4_0",
            pixel_blob,
            hr)) {
        last_error_ = hr;
        return false;
    }

    hr = device->CreateVertexShader(
        vertex_blob->GetBufferPointer(),
        vertex_blob->GetBufferSize(),
        nullptr,
        &vertex_shader_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr = device->CreatePixelShader(
        pixel_blob->GetBufferPointer(),
        pixel_blob->GetBufferSize(),
        nullptr,
        &pixel_shader_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter =
        D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU =
        D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV =
        D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW =
        D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MinLOD = 0.0F;
    sampler_desc.MaxLOD =
        D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(
        &sampler_desc,
        &sampler_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer_desc{};
    rasterizer_desc.FillMode =
        D3D11_FILL_SOLID;
    rasterizer_desc.CullMode =
        D3D11_CULL_NONE;
    rasterizer_desc.FrontCounterClockwise =
        FALSE;
    rasterizer_desc.DepthClipEnable =
        TRUE;

    hr = device->CreateRasterizerState(
        &rasterizer_desc,
        &rasterizer_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend =
        D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend =
        D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp =
        D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha =
        D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha =
        D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOpAlpha =
        D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(
        &blend_desc,
        &cursor_blend_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::EnsureFrameTexture(
    std::uint32_t width,
    std::uint32_t height) {
    if (frame_texture_ != nullptr &&
        frame_width_ == width &&
        frame_height_ == height) {
        return true;
    }

    ID3D11Device* device = device_.Device();
    if (device == nullptr ||
        width == 0 ||
        height == 0) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    frame_srv_.Reset();
    frame_texture_.Reset();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage =
        D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr =
        device->CreateTexture2D(
            &desc,
            nullptr,
            &frame_texture_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(
        frame_texture_.Get(),
        &srv_desc,
        &frame_srv_);
    if (FAILED(hr)) {
        frame_texture_.Reset();
        last_error_ = hr;
        return false;
    }

    frame_width_ = width;
    frame_height_ = height;
    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::EnsureCursorTexture(
    std::uint32_t width,
    std::uint32_t height) {
    if (cursor_texture_ != nullptr &&
        cursor_width_ == width &&
        cursor_height_ == height) {
        return true;
    }

    ID3D11Device* device = device_.Device();
    if (device == nullptr ||
        width == 0 ||
        height == 0) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    cursor_srv_.Reset();
    cursor_texture_.Reset();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage =
        D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr =
        device->CreateTexture2D(
            &desc,
            nullptr,
            &cursor_texture_);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(
        cursor_texture_.Get(),
        &srv_desc,
        &cursor_srv_);
    if (FAILED(hr)) {
        cursor_texture_.Reset();
        last_error_ = hr;
        return false;
    }

    cursor_width_ = width;
    cursor_height_ = height;
    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::UploadFrame(
    const capture::CapturedFrame& frame) {
    if (!frame.IsValid() ||
        frame.pixel_format !=
            capture::CapturePixelFormat::Bgra8) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    if (!EnsureFrameTexture(
            frame.width,
            frame.height)) {
        return false;
    }

    ID3D11DeviceContext* context =
        device_.Context();
    if (context == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    if (frame.HasGpuTexture()) {
        ID3D11Device* source_device = nullptr;
        frame.texture->GetDevice(
            &source_device);

        const bool same_device =
            source_device ==
            device_.Device();

        if (source_device != nullptr) {
            source_device->Release();
        }

        if (!same_device) {
            last_error_ =
                DXGI_ERROR_INVALID_CALL;
            return false;
        }

        context->CopyResource(
            frame_texture_.Get(),
            frame.texture.Get());
        return true;
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

    if (frame.bgra.size() < required) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    context->UpdateSubresource(
        frame_texture_.Get(),
        0,
        nullptr,
        frame.bgra.data(),
        row_pitch,
        0);

    return true;
}

bool PreviewRenderer::UploadCursor(
    const capture::CapturedCursor& cursor) {
    if (!cursor.IsValid()) {
        return true;
    }

    if (!EnsureCursorTexture(
            cursor.width,
            cursor.height)) {
        return false;
    }

    ID3D11DeviceContext* context =
        device_.Context();
    if (context == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    const std::uint32_t row_pitch =
        cursor.row_pitch != 0
            ? cursor.row_pitch
            : cursor.width * 4;

    const std::size_t required =
        static_cast<std::size_t>(
            row_pitch) *
        static_cast<std::size_t>(
            cursor.height);

    if (cursor.bgra.size() < required) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    context->UpdateSubresource(
        cursor_texture_.Get(),
        0,
        nullptr,
        cursor.bgra.data(),
        row_pitch,
        0);

    return true;
}

bool PreviewRenderer::DrawFrame(
    const capture::CapturedFrame& frame) {
    if (!EnsurePipeline() ||
        frame_srv_ == nullptr ||
        view_width_ == 0 ||
        view_height_ == 0 ||
        frame.width == 0 ||
        frame.height == 0) {
        return false;
    }

    if (frame.cursor.IsValid() &&
        !UploadCursor(frame.cursor)) {
        return false;
    }

    ID3D11DeviceContext* context =
        device_.Context();
    ID3D11RenderTargetView* target =
        device_.RenderTarget();
    IDXGISwapChain1* swap_chain =
        device_.SwapChain();

    if (context == nullptr ||
        target == nullptr ||
        swap_chain == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    constexpr std::array<float, 4> clear{
        0.03F,
        0.04F,
        0.05F,
        1.0F};

    ID3D11RenderTargetView* targets[] = {
        target
    };
    context->OMSetRenderTargets(
        1,
        targets,
        nullptr);

    context->OMSetBlendState(
        nullptr,
        nullptr,
        0xFFFFFFFFu);

    context->RSSetState(
        rasterizer_.Get());

    context->ClearRenderTargetView(
        target,
        clear.data());

    const PreviewFit fit =
        CalculateAspectFit(
            view_width_,
            view_height_,
            frame.width,
            frame.height);
    const double scale = fit.scale;

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = fit.x;
    viewport.TopLeftY = fit.y;
    viewport.Width =
        std::max(
            1.0F,
            fit.width);
    viewport.Height =
        std::max(
            1.0F,
            fit.height);
    viewport.MinDepth = 0.0F;
    viewport.MaxDepth = 1.0F;

    context->RSSetViewports(
        1,
        &viewport);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(
        vertex_shader_.Get(),
        nullptr,
        0);
    context->PSSetShader(
        pixel_shader_.Get(),
        nullptr,
        0);

    ID3D11SamplerState* sampler =
        sampler_.Get();
    context->PSSetSamplers(
        0,
        1,
        &sampler);

    ID3D11ShaderResourceView* srv =
        frame_srv_.Get();
    context->PSSetShaderResources(
        0,
        1,
        &srv);
    context->Draw(3, 0);

    ID3D11ShaderResourceView* null_srv =
        nullptr;
    context->PSSetShaderResources(
        0,
        1,
        &null_srv);

    if (frame.cursor.IsValid() &&
        cursor_srv_ != nullptr) {
        const float cursor_left =
            viewport.TopLeftX +
            static_cast<float>(
                static_cast<double>(
                    frame.cursor.x) *
                scale);
        const float cursor_top =
            viewport.TopLeftY +
            static_cast<float>(
                static_cast<double>(
                    frame.cursor.y) *
                scale);
        const float cursor_width =
            std::max(
                1.0F,
                static_cast<float>(
                    static_cast<double>(
                        frame.cursor.width) *
                    scale));
        const float cursor_height =
            std::max(
                1.0F,
                static_cast<float>(
                    static_cast<double>(
                        frame.cursor.height) *
                    scale));

        const bool outside =
            cursor_left + cursor_width <= 0.0F ||
            cursor_top + cursor_height <= 0.0F ||
            cursor_left >=
                static_cast<float>(view_width_) ||
            cursor_top >=
                static_cast<float>(view_height_);

        if (!outside) {
            D3D11_VIEWPORT cursor_viewport{};
            cursor_viewport.TopLeftX = cursor_left;
            cursor_viewport.TopLeftY = cursor_top;
            cursor_viewport.Width = cursor_width;
            cursor_viewport.Height = cursor_height;
            cursor_viewport.MinDepth = 0.0F;
            cursor_viewport.MaxDepth = 1.0F;

            context->RSSetViewports(
                1,
                &cursor_viewport);

            const float blend_factor[4] = {
                0.0F, 0.0F, 0.0F, 0.0F};
            context->OMSetBlendState(
                cursor_blend_.Get(),
                blend_factor,
                0xFFFFFFFFu);

            srv = cursor_srv_.Get();
            context->PSSetShaderResources(
                0,
                1,
                &srv);
            context->Draw(3, 0);

            context->PSSetShaderResources(
                0,
                1,
                &null_srv);
            context->OMSetBlendState(
                nullptr,
                nullptr,
                0xFFFFFFFFu);
        }
    }

    const HRESULT hr =
        swap_chain->Present(1, 0);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool PreviewRenderer::RenderFrame(
    const capture::CapturedFrame& frame) {
    if (!UploadFrame(frame)) {
        return false;
    }

    return DrawFrame(frame);
}

}  // namespace fps::graphics
