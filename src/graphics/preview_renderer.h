#pragma once

#include "capture/capture_frame.h"
#include "graphics/d3d11_device.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace fps::graphics {

struct PreviewFit {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
    double scale{0.0};
};

PreviewFit CalculateAspectFit(
    std::uint32_t view_width,
    std::uint32_t view_height,
    std::uint32_t frame_width,
    std::uint32_t frame_height);

class PreviewRenderer {
public:
    bool Initialize(
        HWND hwnd,
        const RendererCreationPolicy& policy = {});

    bool Resize(
        UINT width,
        UINT height);

    bool RenderPlaceholder();

    bool RenderFrame(
        const capture::CapturedFrame& frame);

    RendererMode Mode() const noexcept {
        return device_.Mode();
    }

    HRESULT LastError() const noexcept {
        return last_error_;
    }

    ID3D11Device* Device() const noexcept {
        return device_.Device();
    }

private:
    bool EnsurePipeline();
    bool EnsureFrameTexture(
        std::uint32_t width,
        std::uint32_t height);

    bool EnsureCursorTexture(
        std::uint32_t width,
        std::uint32_t height);

    bool UploadFrame(
        const capture::CapturedFrame& frame);

    bool UploadCursor(
        const capture::CapturedCursor& cursor);

    bool DrawFrame(
        const capture::CapturedFrame& frame);

    D3D11Device device_;

    Microsoft::WRL::ComPtr<ID3D11VertexShader>
        vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>
        pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>
        sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>
        rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11BlendState>
        cursor_blend_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        frame_texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
        frame_srv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        cursor_texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
        cursor_srv_;

    std::uint32_t frame_width_{0};
    std::uint32_t frame_height_{0};
    std::uint32_t cursor_width_{0};
    std::uint32_t cursor_height_{0};
    UINT view_width_{0};
    UINT view_height_{0};

    HRESULT last_error_{S_OK};
};

}  // namespace fps::graphics
