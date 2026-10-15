#include "capture/windows_graphics_capture.h"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.graphics.directx.h>

#include <dxgi.h>

#include <cstdint>
#include <cwchar>
#include <utility>

namespace fps::capture {
namespace {

template <typename T>
HRESULT GetActivationFactory(
    const wchar_t* runtime_class,
    Microsoft::WRL::ComPtr<T>& output) {
    output.Reset();

    HSTRING_HEADER header{};
    HSTRING class_name{};

    HRESULT hr =
        WindowsCreateStringReference(
            runtime_class,
            static_cast<UINT32>(
                std::wcslen(runtime_class)),
            &header,
            &class_name);

    if (FAILED(hr)) {
        return hr;
    }

    return RoGetActivationFactory(
        class_name,
        __uuidof(T),
        reinterpret_cast<void**>(
            output.ReleaseAndGetAddressOf()));
}

void CloseInspectable(
    IUnknown* object) {
    if (object == nullptr) {
        return;
    }

    Microsoft::WRL::ComPtr<
        ABI::Windows::Foundation::IClosable>
        closable;

    if (SUCCEEDED(
            object->QueryInterface(
                IID_PPV_ARGS(&closable)))) {
        closable->Close();
    }
}

}  // namespace

WindowsGraphicsCaptureSession::
WindowsGraphicsCaptureSession(
    CaptureSourceDescriptor source,
    ID3D11Device* device)
    : source_(std::move(source)),
      device_(device) {
    if (device_ != nullptr) {
        device_->GetImmediateContext(
            &context_);
    }
}

bool WindowsGraphicsCaptureSession::
IsCaptureSupported() {
    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IGraphicsCaptureSessionStatics>
        statics;

    HRESULT hr =
        GetActivationFactory(
            RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureSession,
            statics);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    boolean supported = false;
    hr = statics->IsSupported(
        &supported);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    if (!supported) {
        last_error_ = E_NOTIMPL;
        return false;
    }

    return true;
}

bool WindowsGraphicsCaptureSession::
CreateWinRtDevice() {
    Microsoft::WRL::ComPtr<IDXGIDevice>
        dxgi_device;

    HRESULT hr =
        device_.As(&dxgi_device);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    Microsoft::WRL::ComPtr<IInspectable>
        inspectable;

    hr =
        CreateDirect3D11DeviceFromDXGIDevice(
            dxgi_device.Get(),
            inspectable.GetAddressOf());

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr = inspectable.As(
        &winrt_device_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    return true;
}

bool WindowsGraphicsCaptureSession::
CreateCaptureItem() {
    if (source_.kind !=
            CaptureSourceKind::Window ||
        source_.native_handle == 0) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    const HWND hwnd =
        reinterpret_cast<HWND>(
            source_.native_handle);

    if (IsWindow(hwnd) == FALSE) {
        last_error_ =
            HRESULT_FROM_WIN32(
                ERROR_INVALID_WINDOW_HANDLE);
        return false;
    }

    Microsoft::WRL::ComPtr<
        IGraphicsCaptureItemInterop>
        interop;

    HRESULT hr =
        GetActivationFactory(
            RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem,
            interop);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr = interop->CreateForWindow(
        hwnd,
        __uuidof(
            ABI::Windows::Graphics::
                Capture::
                    IGraphicsCaptureItem),
        reinterpret_cast<void**>(
            item_.ReleaseAndGetAddressOf()));

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    return true;
}

bool WindowsGraphicsCaptureSession::
CreateFramePoolAndSession() {
    if (item_ == nullptr ||
        winrt_device_ == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    ABI::Windows::Graphics::SizeInt32
        size{};

    HRESULT hr =
        item_->get_Size(&size);

    if (FAILED(hr) ||
        size.Width <= 0 ||
        size.Height <= 0) {
        last_error_ =
            FAILED(hr)
                ? hr
                : E_INVALIDARG;
        return false;
    }

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IDirect3D11CaptureFramePoolStatics2>
        frame_pool_statics;

    hr = GetActivationFactory(
        RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool,
        frame_pool_statics);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr =
        frame_pool_statics->
            CreateFreeThreaded(
                winrt_device_.Get(),
                ABI::Windows::Graphics::
                    DirectX::
                        DirectXPixelFormat_B8G8R8A8UIntNormalized,
                2,
                size,
                frame_pool_.
                    ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr = frame_pool_->
        CreateCaptureSession(
            item_.Get(),
            session_.
                ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    if (!SetCursorCaptureEnabled(
            show_cursor_)) {
        return false;
    }

    hr = session_->StartCapture();

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    return true;
}

bool WindowsGraphicsCaptureSession::
SetCursorCaptureEnabled(
    bool enabled) {
    show_cursor_ = enabled;

    if (session_ == nullptr) {
        return true;
    }

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IGraphicsCaptureSession2>
        session2;

    const HRESULT hr =
        session_.As(&session2);

    if (FAILED(hr)) {
        // Older runtimes implicitly capture the cursor.
        // That is acceptable only when cursor display is
        // requested.
        if (enabled &&
            hr == E_NOINTERFACE) {
            return true;
        }

        last_error_ = hr;
        return false;
    }

    const HRESULT cursor_hr =
        session2->put_IsCursorCaptureEnabled(
            enabled ? TRUE : FALSE);

    if (FAILED(cursor_hr)) {
        last_error_ = cursor_hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

bool WindowsGraphicsCaptureSession::
Start() {
    Stop();
    state_ =
        CaptureSessionState::Starting;

    if (device_ == nullptr ||
        context_ == nullptr) {
        last_error_ = E_POINTER;
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    if (!IsCaptureSupported()) {
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    if (!CreateWinRtDevice()) {
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    if (!CreateCaptureItem()) {
        state_ =
            CaptureSessionState::SourceLost;
        return false;
    }

    if (!CreateFramePoolAndSession()) {
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    sequence_ = 0;
    last_error_ = S_OK;
    return true;
}

void WindowsGraphicsCaptureSession::
Stop() {
    CloseInspectable(session_.Get());
    CloseInspectable(frame_pool_.Get());

    session_.Reset();
    frame_pool_.Reset();
    item_.Reset();
    winrt_device_.Reset();
    frame_texture_.Reset();

    sequence_ = 0;
    state_ =
        CaptureSessionState::Stopped;
    last_error_ = S_OK;
}

bool WindowsGraphicsCaptureSession::
EnsureFrameTexture(
    const D3D11_TEXTURE2D_DESC& source_desc) {
    if (frame_texture_ != nullptr) {
        D3D11_TEXTURE2D_DESC current{};
        frame_texture_->GetDesc(
            &current);

        if (current.Width ==
                source_desc.Width &&
            current.Height ==
                source_desc.Height &&
            current.Format ==
                source_desc.Format) {
            return true;
        }
    }

    frame_texture_.Reset();

    D3D11_TEXTURE2D_DESC desc =
        source_desc;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Usage =
        D3D11_USAGE_DEFAULT;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    const HRESULT hr =
        device_->CreateTexture2D(
            &desc,
            nullptr,
            &frame_texture_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    return true;
}

bool WindowsGraphicsCaptureSession::
TryAcquireFrame(
    CapturedFrame& frame) {
    frame = {};

    if (frame_pool_ == nullptr ||
        context_ == nullptr) {
        last_error_ = E_UNEXPECTED;
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    const HWND hwnd =
        reinterpret_cast<HWND>(
            source_.native_handle);

    if (IsWindow(hwnd) == FALSE) {
        last_error_ =
            HRESULT_FROM_WIN32(
                ERROR_INVALID_WINDOW_HANDLE);
        state_ =
            CaptureSessionState::SourceLost;
        return false;
    }

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::Capture::
            IDirect3D11CaptureFrame>
        capture_frame;

    HRESULT hr =
        frame_pool_->TryGetNextFrame(
            capture_frame.
                GetAddressOf());

    if (FAILED(hr)) {
        last_error_ = hr;
        state_ =
            CaptureSessionState::SourceLost;
        return false;
    }

    if (capture_frame == nullptr) {
        last_error_ = S_OK;
        return false;
    }

    ABI::Windows::Graphics::SizeInt32
        content_size{};

    hr = capture_frame->
        get_ContentSize(
            &content_size);

    if (FAILED(hr)) {
        CloseInspectable(
            capture_frame.Get());
        last_error_ = hr;
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::DirectX::
            Direct3D11::IDirect3DSurface>
        surface;

    hr = capture_frame->get_Surface(
        surface.GetAddressOf());

    if (FAILED(hr)) {
        CloseInspectable(
            capture_frame.Get());
        last_error_ = hr;
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    Microsoft::WRL::ComPtr<
        Windows::Graphics::DirectX::
            Direct3D11::
                IDirect3DDxgiInterfaceAccess>
        access;

    hr = surface.As(&access);

    if (FAILED(hr)) {
        CloseInspectable(
            capture_frame.Get());
        last_error_ = hr;
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    Microsoft::WRL::ComPtr<
        ID3D11Texture2D>
        source_texture;

    hr = access->GetInterface(
        IID_PPV_ARGS(
            &source_texture));

    if (FAILED(hr)) {
        CloseInspectable(
            capture_frame.Get());
        last_error_ = hr;
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    source_texture->GetDesc(&desc);

    if (!EnsureFrameTexture(desc)) {
        CloseInspectable(
            capture_frame.Get());
        state_ =
            CaptureSessionState::Error;
        return false;
    }

    context_->CopyResource(
        frame_texture_.Get(),
        source_texture.Get());

    ABI::Windows::Foundation::TimeSpan
        relative_time{};

    const HRESULT time_hr =
        capture_frame->
            get_SystemRelativeTime(
                &relative_time);

    CloseInspectable(
        capture_frame.Get());

    frame.sequence = ++sequence_;
    frame.timestamp_100ns =
        SUCCEEDED(time_hr)
            ? relative_time.Duration
            : 0;
    frame.width =
        static_cast<std::uint32_t>(
            content_size.Width > 0
                ? content_size.Width
                : static_cast<INT32>(
                      desc.Width));
    frame.height =
        static_cast<std::uint32_t>(
            content_size.Height > 0
                ? content_size.Height
                : static_cast<INT32>(
                      desc.Height));
    frame.row_pitch =
        desc.Width * 4;
    frame.pixel_format =
        CapturePixelFormat::Bgra8;
    frame.texture =
        frame_texture_;

    if (content_size.Width > 0 &&
        content_size.Height > 0 &&
        (static_cast<UINT>(
             content_size.Width) !=
             desc.Width ||
         static_cast<UINT>(
             content_size.Height) !=
             desc.Height)) {
        const HRESULT recreate_hr =
            frame_pool_->Recreate(
                winrt_device_.Get(),
                ABI::Windows::Graphics::
                    DirectX::
                        DirectXPixelFormat_B8G8R8A8UIntNormalized,
                2,
                content_size);

        if (FAILED(recreate_hr)) {
            last_error_ = recreate_hr;
            state_ =
                CaptureSessionState::Error;
            return false;
        }
    }

    state_ =
        CaptureSessionState::Previewing;
    last_error_ = S_OK;
    return true;
}

std::string
WindowsGraphicsCaptureSession::
StatusText() const {
    switch (state_) {
    case CaptureSessionState::Stopped:
        return "Window capture stopped";
    case CaptureSessionState::Starting:
        return "Window capture starting";
    case CaptureSessionState::Previewing:
        return "Window preview active (WGC ABI)";
    case CaptureSessionState::SourceLost:
        return "Window source lost";
    case CaptureSessionState::Error:
        return "Window capture error";
    }

    return "Window capture";
}

}  // namespace fps::capture
