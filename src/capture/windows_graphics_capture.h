#pragma once

#include "capture/capture_session.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.h>

#include <cstdint>
#include <string>

namespace fps::capture {

class WindowsGraphicsCaptureSession final
    : public ICaptureSession {
public:
    WindowsGraphicsCaptureSession(
        CaptureSourceDescriptor source,
        ID3D11Device* device);

    bool Start() override;
    void Stop() override;

    bool TryAcquireFrame(
        CapturedFrame& frame) override;

    bool SetCursorCaptureEnabled(
        bool enabled) override;

    CaptureSessionState State()
        const noexcept override {
        return state_;
    }

    const CaptureSourceDescriptor&
    Source() const noexcept override {
        return source_;
    }

    long LastError()
        const noexcept override {
        return last_error_;
    }

    std::string StatusText()
        const override;

private:
    bool IsCaptureSupported();
    bool CreateWinRtDevice();
    bool CreateCaptureItem();
    bool CreateFramePoolAndSession();

    bool EnsureFrameTexture(
        const D3D11_TEXTURE2D_DESC& desc);

    CaptureSourceDescriptor source_;

    Microsoft::WRL::ComPtr<ID3D11Device>
        device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>
        context_;

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::DirectX::
            Direct3D11::IDirect3DDevice>
        winrt_device_;

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IGraphicsCaptureItem>
        item_;

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IDirect3D11CaptureFramePool>
        frame_pool_;

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IGraphicsCaptureSession>
        session_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        frame_texture_;

    std::uint64_t sequence_{0};
    bool show_cursor_{true};
    CaptureSessionState state_{
        CaptureSessionState::Stopped};
    HRESULT last_error_{S_OK};
};

}  // namespace fps::capture
