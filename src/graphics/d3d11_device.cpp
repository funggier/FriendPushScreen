#include "graphics/d3d11_device.h"

#include <array>

namespace fps::graphics {

RendererMode ResolveRendererMode(bool hardware_success, bool warp_success) {
    return ResolveRendererMode(
        RendererCreationPolicy{},
        hardware_success,
        warp_success);
}

RendererMode ResolveRendererMode(
    const RendererCreationPolicy& policy,
    bool hardware_success,
    bool warp_success) {
    if (policy.try_hardware && hardware_success) {
        return RendererMode::Hardware;
    }
    if (policy.try_warp && warp_success) {
        return RendererMode::WarpFallback;
    }
    return RendererMode::Error;
}

HRESULT D3D11Device::CreateDeviceForDriver(D3D_DRIVER_TYPE driver_type) {
    device_.Reset();
    context_.Reset();

    constexpr std::array<D3D_FEATURE_LEVEL, 4> feature_levels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL created_level{};
    return D3D11CreateDevice(
        nullptr,
        driver_type,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels.data(),
        static_cast<UINT>(feature_levels.size()),
        D3D11_SDK_VERSION,
        &device_,
        &created_level,
        &context_);
}

HRESULT D3D11Device::CreateSwapChain(HWND hwnd) {
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = device_.As(&dxgi_device);
    if (FAILED(hr)) {
        return hr;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgi_device->GetAdapter(&adapter);
    if (FAILED(hr)) {
        return hr;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return hr;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 0;
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = factory->CreateSwapChainForHwnd(
        device_.Get(),
        hwnd,
        &desc,
        nullptr,
        nullptr,
        &swap_chain_);
    if (FAILED(hr)) {
        return hr;
    }

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    return S_OK;
}

HRESULT D3D11Device::CreateRenderTarget() {
    render_target_.Reset();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(
        0,
        IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) {
        return hr;
    }

    hr = device_->CreateRenderTargetView(
        back_buffer.Get(),
        nullptr,
        &render_target_);
    if (FAILED(hr)) {
        return hr;
    }

    ID3D11RenderTargetView* target = render_target_.Get();
    context_->OMSetRenderTargets(1, &target, nullptr);
    return S_OK;
}

bool D3D11Device::Initialize(
    HWND hwnd,
    const RendererCreationPolicy& policy) {
    if (hwnd == nullptr) {
        mode_ = RendererMode::Error;
        last_error_ = E_INVALIDARG;
        return false;
    }

    HRESULT hardware_hr = E_FAIL;
    HRESULT warp_hr = E_FAIL;

    if (policy.try_hardware) {
        hardware_hr = CreateDeviceForDriver(D3D_DRIVER_TYPE_HARDWARE);
    }

    if (FAILED(hardware_hr) && policy.try_warp) {
        warp_hr = CreateDeviceForDriver(D3D_DRIVER_TYPE_WARP);
    }

    mode_ = ResolveRendererMode(
        policy,
        SUCCEEDED(hardware_hr),
        SUCCEEDED(warp_hr));

    if (mode_ == RendererMode::Error) {
        last_error_ =
            policy.try_warp ? warp_hr : hardware_hr;
        return false;
    }

    HRESULT hr = CreateSwapChain(hwnd);
    if (FAILED(hr)) {
        mode_ = RendererMode::Error;
        last_error_ = hr;
        return false;
    }

    hr = CreateRenderTarget();
    if (FAILED(hr)) {
        mode_ = RendererMode::Error;
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool D3D11Device::Resize(UINT width, UINT height) {
    if (swap_chain_ == nullptr || context_ == nullptr) {
        return false;
    }

    if (width == 0 || height == 0) {
        return true;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    render_target_.Reset();

    const HRESULT hr = swap_chain_->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    const HRESULT target_hr = CreateRenderTarget();
    if (FAILED(target_hr)) {
        last_error_ = target_hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool D3D11Device::PresentPlaceholder() {
    if (context_ == nullptr ||
        render_target_ == nullptr ||
        swap_chain_ == nullptr) {
        return false;
    }

    constexpr float clear_color[4]{
        0.08F,
        0.12F,
        0.16F,
        1.0F
    };
    context_->ClearRenderTargetView(
        render_target_.Get(),
        clear_color);

    const HRESULT hr = swap_chain_->Present(1, 0);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

}  // namespace fps::graphics
