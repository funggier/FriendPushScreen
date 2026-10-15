#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace fps::graphics {

enum class RendererMode {
    Hardware,
    WarpFallback,
    Error
};

struct RendererCreationPolicy {
    bool try_hardware{true};
    bool try_warp{true};
};

RendererMode ResolveRendererMode(bool hardware_success, bool warp_success);
RendererMode ResolveRendererMode(
    const RendererCreationPolicy& policy,
    bool hardware_success,
    bool warp_success);

class D3D11Device {
public:
    bool Initialize(
        HWND hwnd,
        const RendererCreationPolicy& policy = {});
    bool Resize(UINT width, UINT height);
    bool PresentPlaceholder();

    RendererMode Mode() const noexcept { return mode_; }
    HRESULT LastError() const noexcept { return last_error_; }

private:
    HRESULT CreateDeviceForDriver(D3D_DRIVER_TYPE driver_type);
    HRESULT CreateSwapChain(HWND hwnd);
    HRESULT CreateRenderTarget();

    RendererMode mode_{RendererMode::Error};
    HRESULT last_error_{S_OK};

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;
};

}  // namespace fps::graphics
