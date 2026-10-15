#include "capture/desktop_duplication_capture.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fps::capture {
namespace {

std::wstring Utf8ToWide(
    const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

    if (required <= 0) {
        return {};
    }

    std::wstring output(
        static_cast<std::size_t>(
            required),
        L'\0');

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            output.data(),
            required) != required) {
        return {};
    }

    return output;
}

std::int64_t QpcTo100ns(
    LARGE_INTEGER value) {
    LARGE_INTEGER frequency{};
    if (QueryPerformanceFrequency(
            &frequency) == FALSE ||
        frequency.QuadPart <= 0) {
        return 0;
    }

    const long double seconds =
        static_cast<long double>(
            value.QuadPart) /
        static_cast<long double>(
            frequency.QuadPart);

    return static_cast<std::int64_t>(
        seconds * 10000000.0L);
}

bool RenderCursorOnBackground(
    HCURSOR cursor,
    int width,
    int height,
    std::uint8_t background,
    std::vector<std::uint8_t>& pixels) {
    pixels.clear();

    if (cursor == nullptr ||
        width <= 0 ||
        height <= 0) {
        return false;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib =
        CreateDIBSection(
            nullptr,
            &info,
            DIB_RGB_COLORS,
            &bits,
            nullptr,
            0);

    if (dib == nullptr || bits == nullptr) {
        if (dib != nullptr) {
            DeleteObject(dib);
        }
        return false;
    }

    HDC dc = CreateCompatibleDC(nullptr);
    if (dc == nullptr) {
        DeleteObject(dib);
        return false;
    }

    HGDIOBJ previous =
        SelectObject(dc, dib);

    const std::size_t byte_count =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) * 4;

    auto* raw =
        static_cast<std::uint8_t*>(bits);

    for (std::size_t offset = 0;
         offset < byte_count;
         offset += 4) {
        raw[offset + 0] = background;
        raw[offset + 1] = background;
        raw[offset + 2] = background;
        raw[offset + 3] = 255;
    }

    const BOOL drawn =
        DrawIconEx(
            dc,
            0,
            0,
            cursor,
            width,
            height,
            0,
            nullptr,
            DI_NORMAL);

    if (drawn != FALSE) {
        pixels.assign(
            raw,
            raw + byte_count);
    }

    SelectObject(dc, previous);
    DeleteDC(dc);
    DeleteObject(dib);

    return drawn != FALSE;
}

bool CaptureCursorPixels(
    HCURSOR cursor,
    std::uint32_t& hotspot_x,
    std::uint32_t& hotspot_y,
    std::uint32_t& width,
    std::uint32_t& height,
    std::vector<std::uint8_t>& bgra) {
    hotspot_x = 0;
    hotspot_y = 0;
    width = 0;
    height = 0;
    bgra.clear();

    ICONINFO icon{};
    if (GetIconInfo(cursor, &icon) == FALSE) {
        return false;
    }

    BITMAP bitmap{};
    bool have_bitmap = false;

    if (icon.hbmColor != nullptr &&
        GetObjectW(
            icon.hbmColor,
            sizeof(bitmap),
            &bitmap) == sizeof(bitmap)) {
        width = static_cast<std::uint32_t>(
            std::max<LONG>(1, bitmap.bmWidth));
        height = static_cast<std::uint32_t>(
            std::max<LONG>(1, bitmap.bmHeight));
        have_bitmap = true;
    } else if (icon.hbmMask != nullptr &&
               GetObjectW(
                   icon.hbmMask,
                   sizeof(bitmap),
                   &bitmap) == sizeof(bitmap)) {
        width = static_cast<std::uint32_t>(
            std::max<LONG>(1, bitmap.bmWidth));
        height = static_cast<std::uint32_t>(
            std::max<LONG>(
                1,
                bitmap.bmHeight / 2));
        have_bitmap = true;
    }

    hotspot_x = icon.xHotspot;
    hotspot_y = icon.yHotspot;

    if (icon.hbmColor != nullptr) {
        DeleteObject(icon.hbmColor);
    }
    if (icon.hbmMask != nullptr) {
        DeleteObject(icon.hbmMask);
    }

    if (!have_bitmap ||
        width > 512 ||
        height > 512) {
        return false;
    }

    std::vector<std::uint8_t> black;
    std::vector<std::uint8_t> white;

    if (!RenderCursorOnBackground(
            cursor,
            static_cast<int>(width),
            static_cast<int>(height),
            0,
            black) ||
        !RenderCursorOnBackground(
            cursor,
            static_cast<int>(width),
            static_cast<int>(height),
            255,
            white) ||
        black.size() != white.size()) {
        return false;
    }

    bgra.resize(black.size(), 0);

    for (std::size_t offset = 0;
         offset + 3 < black.size();
         offset += 4) {
        const int diff_b =
            std::clamp(
                static_cast<int>(white[offset + 0]) -
                    static_cast<int>(black[offset + 0]),
                0,
                255);
        const int diff_g =
            std::clamp(
                static_cast<int>(white[offset + 1]) -
                    static_cast<int>(black[offset + 1]),
                0,
                255);
        const int diff_r =
            std::clamp(
                static_cast<int>(white[offset + 2]) -
                    static_cast<int>(black[offset + 2]),
                0,
                255);

        const int background_share =
            (diff_b + diff_g + diff_r) / 3;
        const int alpha =
            255 - background_share;

        if (alpha <= 0) {
            continue;
        }

        bgra[offset + 0] =
            static_cast<std::uint8_t>(
                std::clamp(
                    (static_cast<int>(
                         black[offset + 0]) *
                     255) / alpha,
                    0,
                    255));
        bgra[offset + 1] =
            static_cast<std::uint8_t>(
                std::clamp(
                    (static_cast<int>(
                         black[offset + 1]) *
                     255) / alpha,
                    0,
                    255));
        bgra[offset + 2] =
            static_cast<std::uint8_t>(
                std::clamp(
                    (static_cast<int>(
                         black[offset + 2]) *
                     255) / alpha,
                    0,
                    255));
        bgra[offset + 3] =
            static_cast<std::uint8_t>(alpha);
    }

    return true;
}

}  // namespace

DesktopDuplicationCaptureSession::
DesktopDuplicationCaptureSession(
    CaptureSourceDescriptor source,
    ID3D11Device* device)
    : source_(std::move(source)),
      device_(device) {
    if (device_ != nullptr) {
        device_->GetImmediateContext(
            &context_);
    }
}

bool DesktopDuplicationCaptureSession::
FindOutput() {
    output_.Reset();
    output_rect_ = {};

    if (device_ == nullptr) {
        last_error_ = E_POINTER;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice>
        dxgi_device;
    HRESULT hr =
        device_.As(&dxgi_device);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter>
        adapter;
    hr = dxgi_device->GetAdapter(
        &adapter);
    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    const std::wstring target =
        Utf8ToWide(source_.native_id);

    for (UINT index = 0;; ++index) {
        Microsoft::WRL::ComPtr<IDXGIOutput>
            output;

        hr = adapter->EnumOutputs(
            index,
            &output);

        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        if (FAILED(hr)) {
            last_error_ = hr;
            return false;
        }

        DXGI_OUTPUT_DESC desc{};
        hr = output->GetDesc(&desc);
        if (FAILED(hr)) {
            continue;
        }

        if (!target.empty() &&
            _wcsicmp(
                desc.DeviceName,
                target.c_str()) != 0) {
            continue;
        }

        hr = output.As(&output_);
        if (FAILED(hr)) {
            last_error_ = hr;
            return false;
        }
        output_rect_ = desc.DesktopCoordinates;

        last_error_ = S_OK;
        return true;
    }

    last_error_ =
        DXGI_ERROR_NOT_FOUND;
    return false;
}

bool DesktopDuplicationCaptureSession::
Start() {
    Stop();
    state_ = CaptureSessionState::Starting;

    if (source_.kind !=
            CaptureSourceKind::Monitor ||
        device_ == nullptr ||
        context_ == nullptr) {
        last_error_ = E_INVALIDARG;
        state_ = CaptureSessionState::Error;
        return false;
    }

    if (!FindOutput()) {
        state_ =
            CaptureSessionState::SourceLost;
        return false;
    }

    HRESULT hr =
        output_->DuplicateOutput(
            device_.Get(),
            &duplication_);

    if (FAILED(hr)) {
        last_error_ = hr;
        state_ = CaptureSessionState::Error;
        return false;
    }

    D3D11_QUERY_DESC query_desc{};
    query_desc.Query = D3D11_QUERY_EVENT;

    hr = device_->CreateQuery(
        &query_desc,
        &copy_complete_query_);
    if (FAILED(hr)) {
        last_error_ = hr;
        state_ = CaptureSessionState::Error;
        duplication_.Reset();
        return false;
    }

    sequence_ = 0;
    last_error_ = S_OK;
    return true;
}

void DesktopDuplicationCaptureSession::
Stop() {
    copy_complete_query_.Reset();
    frame_texture_.Reset();
    duplication_.Reset();
    output_.Reset();
    sequence_ = 0;
    state_ = CaptureSessionState::Stopped;
    last_error_ = S_OK;
}

void DesktopDuplicationCaptureSession::
CaptureCursor(
    CapturedCursor& cursor) const {
    cursor = {};

    if (!show_cursor_) {
        return;
    }

    CURSORINFO info{};
    info.cbSize = sizeof(info);

    if (GetCursorInfo(&info) == FALSE ||
        (info.flags & CURSOR_SHOWING) == 0 ||
        info.hCursor == nullptr) {
        return;
    }

    if (info.ptScreenPos.x < output_rect_.left ||
        info.ptScreenPos.x >= output_rect_.right ||
        info.ptScreenPos.y < output_rect_.top ||
        info.ptScreenPos.y >= output_rect_.bottom) {
        return;
    }

    std::uint32_t hotspot_x = 0;
    std::uint32_t hotspot_y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> pixels;

    if (!CaptureCursorPixels(
            info.hCursor,
            hotspot_x,
            hotspot_y,
            width,
            height,
            pixels)) {
        return;
    }

    cursor.visible = true;
    cursor.x =
        info.ptScreenPos.x -
        output_rect_.left -
        static_cast<LONG>(hotspot_x);
    cursor.y =
        info.ptScreenPos.y -
        output_rect_.top -
        static_cast<LONG>(hotspot_y);
    cursor.width = width;
    cursor.height = height;
    cursor.row_pitch = width * 4;
    cursor.bgra = std::move(pixels);
}

bool DesktopDuplicationCaptureSession::
EnsureFrameTexture(
    const D3D11_TEXTURE2D_DESC& source_desc) {
    if (frame_texture_ != nullptr) {
        D3D11_TEXTURE2D_DESC current{};
        frame_texture_->GetDesc(&current);

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
    desc.Usage = D3D11_USAGE_DEFAULT;
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

bool DesktopDuplicationCaptureSession::
TryAcquireFrame(
    CapturedFrame& frame) {
    frame = {};

    if (duplication_ == nullptr ||
        context_ == nullptr) {
        last_error_ = E_UNEXPECTED;
        state_ = CaptureSessionState::Error;
        return false;
    }

    DXGI_OUTDUPL_FRAME_INFO info{};
    Microsoft::WRL::ComPtr<IDXGIResource>
        resource;

    HRESULT hr =
        duplication_->AcquireNextFrame(
            0,
            &info,
            &resource);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        last_error_ = S_OK;
        return false;
    }

    if (hr == DXGI_ERROR_ACCESS_LOST ||
        hr == DXGI_ERROR_SESSION_DISCONNECTED) {
        last_error_ = hr;
        state_ =
            CaptureSessionState::SourceLost;
        return false;
    }

    if (FAILED(hr)) {
        last_error_ = hr;
        state_ = CaptureSessionState::Error;
        return false;
    }

    bool release_required = true;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        acquired_texture;

    hr = resource.As(&acquired_texture);
    if (FAILED(hr)) {
        last_error_ = hr;
        duplication_->ReleaseFrame();
        state_ = CaptureSessionState::Error;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    acquired_texture->GetDesc(&desc);

    if (desc.Format !=
            DXGI_FORMAT_B8G8R8A8_UNORM &&
        desc.Format !=
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        last_error_ =
            DXGI_ERROR_UNSUPPORTED;
        duplication_->ReleaseFrame();
        state_ = CaptureSessionState::Error;
        return false;
    }

    if (!EnsureFrameTexture(desc)) {
        duplication_->ReleaseFrame();
        state_ = CaptureSessionState::Error;
        return false;
    }

    context_->CopyResource(
        frame_texture_.Get(),
        acquired_texture.Get());

    if (copy_complete_query_ == nullptr) {
        last_error_ = E_UNEXPECTED;
        duplication_->ReleaseFrame();
        state_ = CaptureSessionState::Error;
        return false;
    }

    context_->End(
        copy_complete_query_.Get());
    context_->Flush();

    const ULONGLONG wait_started =
        GetTickCount64();

    for (;;) {
        const HRESULT copy_hr =
            context_->GetData(
                copy_complete_query_.Get(),
                nullptr,
                0,
                0);

        if (copy_hr == S_OK) {
            break;
        }

        if (FAILED(copy_hr)) {
            last_error_ = copy_hr;
            duplication_->ReleaseFrame();
            state_ = CaptureSessionState::Error;
            return false;
        }

        if (GetTickCount64() -
                wait_started >
            250) {
            last_error_ =
                DXGI_ERROR_WAIT_TIMEOUT;
            duplication_->ReleaseFrame();
            state_ = CaptureSessionState::Error;
            return false;
        }

        Sleep(0);
    }

    const HRESULT release_hr =
        duplication_->ReleaseFrame();
    release_required = false;

    if (FAILED(release_hr)) {
        last_error_ = release_hr;
        state_ =
            CaptureSessionState::SourceLost;
        return false;
    }

    frame.sequence = ++sequence_;
    frame.timestamp_100ns =
        QpcTo100ns(
            info.LastPresentTime);
    frame.width = desc.Width;
    frame.height = desc.Height;
    frame.row_pitch = desc.Width * 4;
    frame.pixel_format =
        CapturePixelFormat::Bgra8;
    frame.texture = frame_texture_;
    CaptureCursor(frame.cursor);

    state_ = CaptureSessionState::Previewing;
    last_error_ = S_OK;

    (void)release_required;
    return true;
}

std::string
DesktopDuplicationCaptureSession::
StatusText() const {
    switch (state_) {
    case CaptureSessionState::Stopped:
        return "Monitor capture stopped";
    case CaptureSessionState::Starting:
        return "Monitor capture starting";
    case CaptureSessionState::Previewing:
        return "Monitor preview active (DDA)";
    case CaptureSessionState::SourceLost:
        return "Monitor source lost";
    case CaptureSessionState::Error:
        return "Monitor capture error";
    }

    return "Monitor capture";
}

}  // namespace fps::capture
