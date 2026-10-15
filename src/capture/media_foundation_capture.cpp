#include "capture/media_foundation_capture.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <objbase.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace fps::capture {
namespace {

std::string WideToUtf8(
    const wchar_t* value) {
    if (value == nullptr ||
        *value == L'\0') {
        return {};
    }

    const int length =
        static_cast<int>(
            wcslen(value));

    const int required =
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            length,
            nullptr,
            0,
            nullptr,
            nullptr);

    if (required <= 0) {
        return {};
    }

    std::string output(
        static_cast<std::size_t>(
            required),
        '\0');

    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            length,
            output.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }

    return output;
}

std::string ReadAllocatedString(
    IMFActivate* activate,
    REFGUID key) {
    wchar_t* value = nullptr;
    UINT32 length = 0;

    const HRESULT hr =
        activate->GetAllocatedString(
            key,
            &value,
            &length);

    if (FAILED(hr) ||
        value == nullptr) {
        CoTaskMemFree(value);
        return {};
    }

    std::string result =
        WideToUtf8(value);

    CoTaskMemFree(value);
    return result;
}

Microsoft::WRL::ComPtr<IMFActivate>
FindVideoDeviceActivation(
    std::string_view symbolic_link) {
    Microsoft::WRL::ComPtr<
        IMFAttributes>
        attributes;

    HRESULT hr =
        MFCreateAttributes(
            &attributes,
            1);

    if (FAILED(hr)) {
        return {};
    }

    hr = attributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)) {
        return {};
    }

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    hr = MFEnumDeviceSources(
        attributes.Get(),
        &activates,
        &count);

    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<
        IMFActivate>
        result;

    for (UINT32 index = 0;
         index < count;
         ++index) {
        IMFActivate* activate =
            activates[index];

        if (activate == nullptr) {
            continue;
        }

        const std::string link =
            ReadAllocatedString(
                activate,
                MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);

        if (link == symbolic_link) {
            result = activate;
        }

        activate->Release();

        if (result != nullptr) {
            for (UINT32 rest =
                     index + 1;
                 rest < count;
                 ++rest) {
                if (activates[rest] !=
                    nullptr) {
                    activates[rest]->
                        Release();
                }
            }
            break;
        }
    }

    CoTaskMemFree(activates);
    return result;
}

std::string SubtypeName(
    REFGUID subtype) {
    if (subtype == MFVideoFormat_MJPG) {
        return "MJPG";
    }
    if (subtype == MFVideoFormat_YUY2) {
        return "YUY2";
    }
    if (subtype == MFVideoFormat_NV12) {
        return "NV12";
    }
    if (subtype == MFVideoFormat_RGB32) {
        return "RGB32";
    }
    if (subtype == MFVideoFormat_ARGB32) {
        return "ARGB32";
    }
    if (subtype == MFVideoFormat_UYVY) {
        return "UYVY";
    }

    return "Other";
}

int ModeScore(
    std::uint32_t width,
    std::uint32_t height,
    double fps,
    REFGUID subtype) {
    constexpr std::int64_t
        target_pixels =
            1920LL * 1080LL;

    const std::int64_t pixels =
        static_cast<std::int64_t>(
            width) *
        static_cast<std::int64_t>(
            height);

    const std::int64_t distance =
        std::llabs(
            pixels - target_pixels);

    int score =
        100000 -
        static_cast<int>(
            std::min<std::int64_t>(
                80000,
                distance / 64));

    if (width == 1920 &&
        height == 1080) {
        score += 50000;
    }

    if (fps >= 59.0) {
        score += 20000;
    } else if (fps >= 29.0) {
        score += 8000;
    }

    score += static_cast<int>(
        std::min(120.0, fps) *
        100.0);

    if (subtype == MFVideoFormat_MJPG) {
        score += 5000;
    } else if (
        subtype ==
        MFVideoFormat_NV12) {
        score += 3000;
    } else if (
        subtype ==
        MFVideoFormat_YUY2) {
        score += 1000;
    }

    return score;
}

bool IsDeviceLossError(
    HRESULT hr) {
#ifdef MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED
    if (hr ==
        MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED) {
        return true;
    }
#endif

    return hr == MF_E_SHUTDOWN ||
        hr ==
            HRESULT_FROM_WIN32(
                ERROR_DEVICE_NOT_CONNECTED) ||
        hr ==
            HRESULT_FROM_WIN32(
                ERROR_DEV_NOT_EXIST);
}

}  // namespace

MediaFoundationCaptureSession::
MediaFoundationCaptureSession(
    CaptureSourceDescriptor source)
    : source_(std::move(source)) {}

MediaFoundationCaptureSession::
~MediaFoundationCaptureSession() {
    Stop();
}

bool MediaFoundationCaptureSession::
CreateReader() {
    auto activate =
        FindVideoDeviceActivation(
            source_.native_id);

    if (activate == nullptr) {
        last_error_.store(
            HRESULT_FROM_WIN32(
                ERROR_DEVICE_NOT_CONNECTED));
        return false;
    }

    HRESULT hr =
        activate->ActivateObject(
            IID_PPV_ARGS(
                &media_source_));

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    Microsoft::WRL::ComPtr<
        IMFAttributes>
        attributes;

    hr = MFCreateAttributes(
        &attributes,
        2);

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    attributes->SetUINT32(
        MF_READWRITE_DISABLE_CONVERTERS,
        FALSE);

    attributes->SetUINT32(
        MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
        TRUE);

    hr =
        MFCreateSourceReaderFromMediaSource(
            media_source_.Get(),
            attributes.Get(),
            &reader_);

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    return true;
}

bool MediaFoundationCaptureSession::
ConfigureBestMode() {
    if (reader_ == nullptr) {
        last_error_.store(E_POINTER);
        return false;
    }

    Microsoft::WRL::ComPtr<
        IMFMediaType>
        best_type;

    media::VideoCaptureMode
        best_mode{};

    int best_score =
        std::numeric_limits<int>::min();

    for (DWORD index = 0;; ++index) {
        Microsoft::WRL::ComPtr<
            IMFMediaType>
            media_type;

        HRESULT hr =
            reader_->GetNativeMediaType(
                static_cast<DWORD>(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                index,
                &media_type);

        if (hr == MF_E_NO_MORE_TYPES) {
            break;
        }

        if (FAILED(hr)) {
            last_error_.store(hr);
            return false;
        }

        GUID major{};
        if (FAILED(
                media_type->GetGUID(
                    MF_MT_MAJOR_TYPE,
                    &major)) ||
            major != MFMediaType_Video) {
            continue;
        }

        UINT32 width = 0;
        UINT32 height = 0;

        if (FAILED(
                MFGetAttributeSize(
                    media_type.Get(),
                    MF_MT_FRAME_SIZE,
                    &width,
                    &height))) {
            continue;
        }

        UINT32 rate_num = 0;
        UINT32 rate_den = 1;

        if (FAILED(
                MFGetAttributeRatio(
                    media_type.Get(),
                    MF_MT_FRAME_RATE,
                    &rate_num,
                    &rate_den))) {
            rate_num = 0;
            rate_den = 1;
        }

        GUID subtype{};
        if (FAILED(
                media_type->GetGUID(
                    MF_MT_SUBTYPE,
                    &subtype))) {
            continue;
        }

        const double fps =
            rate_den == 0
                ? 0.0
                : static_cast<double>(
                      rate_num) /
                      static_cast<double>(
                          rate_den);

        const int score =
            ModeScore(
                width,
                height,
                fps,
                subtype);

        if (score <= best_score) {
            continue;
        }

        best_score = score;
        best_type = media_type;

        best_mode.width = width;
        best_mode.height = height;
        best_mode.frame_rate_num =
            rate_num;
        best_mode.frame_rate_den =
            rate_den == 0
                ? 1
                : rate_den;
        best_mode.pixel_format =
            SubtypeName(subtype);
    }

    if (best_type == nullptr) {
        last_error_.store(
            MF_E_INVALIDMEDIATYPE);
        return false;
    }

    const HRESULT hr =
        reader_->SetCurrentMediaType(
            static_cast<DWORD>(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            nullptr,
            best_type.Get());

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    selected_mode_ =
        std::move(best_mode);

    return true;
}

bool MediaFoundationCaptureSession::
ConfigureRgb32Output() {
    if (reader_ == nullptr ||
        selected_mode_.width == 0 ||
        selected_mode_.height == 0) {
        last_error_.store(E_INVALIDARG);
        return false;
    }

    Microsoft::WRL::ComPtr<
        IMFMediaType>
        output_type;

    HRESULT hr =
        MFCreateMediaType(
            &output_type);

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    hr = output_type->SetGUID(
        MF_MT_MAJOR_TYPE,
        MFMediaType_Video);

    if (SUCCEEDED(hr)) {
        hr = output_type->SetGUID(
            MF_MT_SUBTYPE,
            MFVideoFormat_RGB32);
    }

    if (SUCCEEDED(hr)) {
        hr = MFSetAttributeSize(
            output_type.Get(),
            MF_MT_FRAME_SIZE,
            selected_mode_.width,
            selected_mode_.height);
    }

    if (SUCCEEDED(hr) &&
        selected_mode_.frame_rate_num >
            0) {
        hr = MFSetAttributeRatio(
            output_type.Get(),
            MF_MT_FRAME_RATE,
            selected_mode_.
                frame_rate_num,
            selected_mode_.
                frame_rate_den);
    }

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    hr =
        reader_->SetCurrentMediaType(
            static_cast<DWORD>(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            nullptr,
            output_type.Get());

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    Microsoft::WRL::ComPtr<
        IMFMediaType>
        current_type;

    hr =
        reader_->GetCurrentMediaType(
            static_cast<DWORD>(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            &current_type);

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    UINT32 width = 0;
    UINT32 height = 0;

    hr = MFGetAttributeSize(
        current_type.Get(),
        MF_MT_FRAME_SIZE,
        &width,
        &height);

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    LONG stride =
        static_cast<LONG>(
            width * 4);

    UINT32 stride_raw = 0;

    if (SUCCEEDED(
            current_type->GetUINT32(
                MF_MT_DEFAULT_STRIDE,
                &stride_raw))) {
        stride =
            static_cast<LONG>(
                stride_raw);
    } else {
        MFGetStrideForBitmapInfoHeader(
            MFVideoFormat_RGB32.Data1,
            width,
            &stride);
    }

    output_width_ = width;
    output_height_ = height;
    output_stride_ =
        stride == 0
            ? static_cast<LONG>(
                  width * 4)
            : stride;

    return true;
}

bool MediaFoundationCaptureSession::
Start() {
    Stop();

    state_.store(
        CaptureSessionState::Starting);

    if (source_.kind !=
            CaptureSourceKind::VideoDevice ||
        source_.native_id.empty()) {
        last_error_.store(E_INVALIDARG);
        state_.store(
            CaptureSessionState::Error);
        return false;
    }

    HRESULT hr =
        MFStartup(
            MF_VERSION,
            MFSTARTUP_FULL);

    if (FAILED(hr)) {
        last_error_.store(hr);
        state_.store(
            CaptureSessionState::Error);
        return false;
    }

    mf_started_ = true;

    if (!CreateReader()) {
        state_.store(
            CaptureSessionState::SourceLost);
        Stop();
        state_.store(
            CaptureSessionState::SourceLost);
        return false;
    }

    if (!ConfigureBestMode() ||
        !ConfigureRgb32Output()) {
        state_.store(
            CaptureSessionState::Error);
        Stop();
        state_.store(
            CaptureSessionState::Error);
        return false;
    }

    stop_requested_.store(false);
    sequence_ = 0;
    last_error_.store(S_OK);

    worker_ = std::thread{
        [this]() {
            CaptureLoop();
        }};

    return true;
}

void MediaFoundationCaptureSession::
Stop() {
    stop_requested_.store(true);

    if (worker_.joinable()) {
        worker_.join();
    }

    {
        std::lock_guard lock{
            frame_mutex_};
        latest_frame_ = {};
    }

    reader_.Reset();

    if (media_source_ != nullptr) {
        media_source_->Shutdown();
    }

    media_source_.Reset();

    if (mf_started_) {
        MFShutdown();
        mf_started_ = false;
    }

    stop_requested_.store(false);
    sequence_ = 0;

    state_.store(
        CaptureSessionState::Stopped);
    last_error_.store(S_OK);
}

bool MediaFoundationCaptureSession::
CopySampleToFrame(
    IMFSample* sample,
    LONGLONG timestamp_100ns,
    CapturedFrame& frame) {
    if (sample == nullptr ||
        output_width_ == 0 ||
        output_height_ == 0) {
        return false;
    }

    Microsoft::WRL::ComPtr<
        IMFMediaBuffer>
        buffer;

    HRESULT hr =
        sample->ConvertToContiguousBuffer(
            &buffer);

    if (FAILED(hr)) {
        last_error_.store(hr);
        return false;
    }

    BYTE* data = nullptr;
    DWORD max_length = 0;
    DWORD current_length = 0;

    hr = buffer->Lock(
        &data,
        &max_length,
        &current_length);

    if (FAILED(hr) ||
        data == nullptr) {
        last_error_.store(
            FAILED(hr)
                ? hr
                : E_POINTER);
        return false;
    }

    const std::size_t row_bytes =
        static_cast<std::size_t>(
            output_width_) *
        4;

    const LONG stride =
        output_stride_ == 0
            ? static_cast<LONG>(
                  row_bytes)
            : output_stride_;

    const std::size_t abs_stride =
        static_cast<std::size_t>(
            stride < 0
                ? -static_cast<
                      std::int64_t>(
                          stride)
                : stride);

    const std::size_t required =
        abs_stride *
        static_cast<std::size_t>(
            output_height_);

    if (abs_stride < row_bytes ||
        current_length < required) {
        buffer->Unlock();
        last_error_.store(
            MF_E_BUFFERTOOSMALL);
        return false;
    }

    frame = {};
    frame.sequence = ++sequence_;
    frame.timestamp_100ns =
        timestamp_100ns;
    frame.width = output_width_;
    frame.height = output_height_;
    frame.row_pitch =
        static_cast<std::uint32_t>(
            row_bytes);
    frame.pixel_format =
        CapturePixelFormat::Bgra8;
    frame.bgra.resize(
        row_bytes *
        static_cast<std::size_t>(
            output_height_));

    for (std::uint32_t y = 0;
         y < output_height_;
         ++y) {
        const std::uint32_t
            source_y =
                stride < 0
                    ? output_height_ -
                          1 -
                          y
                    : y;

        const BYTE* source_row =
            data +
            static_cast<std::size_t>(
                source_y) *
                abs_stride;

        std::uint8_t* destination =
            frame.bgra.data() +
            static_cast<std::size_t>(
                y) *
                row_bytes;

        std::memcpy(
            destination,
            source_row,
            row_bytes);
    }

    buffer->Unlock();
    return true;
}

void MediaFoundationCaptureSession::
CaptureLoop() {
    const HRESULT co_hr =
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

    const bool should_uninitialize =
        SUCCEEDED(co_hr);

    while (!stop_requested_.load()) {
        DWORD actual_stream = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;

        Microsoft::WRL::ComPtr<
            IMFSample>
            sample;

        const HRESULT hr =
            reader_->ReadSample(
                static_cast<DWORD>(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                0,
                &actual_stream,
                &flags,
                &timestamp,
                &sample);

        if (FAILED(hr)) {
            last_error_.store(hr);
            state_.store(
                IsDeviceLossError(hr)
                    ? CaptureSessionState::
                          SourceLost
                    : CaptureSessionState::
                          Error);
            break;
        }

        if ((flags &
             MF_SOURCE_READERF_ENDOFSTREAM) !=
            0) {
            state_.store(
                CaptureSessionState::
                    SourceLost);
            last_error_.store(
                MF_E_END_OF_STREAM);
            break;
        }

        if (sample == nullptr) {
            continue;
        }

        CapturedFrame frame{};

        if (!CopySampleToFrame(
                sample.Get(),
                timestamp,
                frame)) {
            state_.store(
                CaptureSessionState::
                    Error);
            break;
        }

        {
            std::lock_guard lock{
                frame_mutex_};

            latest_frame_ =
                std::move(frame);
        }

        state_.store(
            CaptureSessionState::
                Previewing);
        last_error_.store(S_OK);
    }

    if (should_uninitialize) {
        CoUninitialize();
    }
}

bool MediaFoundationCaptureSession::
TryAcquireFrame(
    CapturedFrame& frame) {
    std::lock_guard lock{
        frame_mutex_};

    if (!latest_frame_.IsValid()) {
        return false;
    }

    frame =
        std::move(latest_frame_);
    latest_frame_ = {};
    return true;
}

std::string
MediaFoundationCaptureSession::
StatusText() const {
    std::ostringstream text;

    switch (state_.load()) {
    case CaptureSessionState::Stopped:
        return "Device capture stopped";

    case CaptureSessionState::Starting:
        return "Device capture starting";

    case CaptureSessionState::Previewing:
        text
            << "Device preview active (MF)";

        if (selected_mode_.width > 0 &&
            selected_mode_.height > 0) {
            text
                << " "
                << selected_mode_.width
                << "x"
                << selected_mode_.height
                << " @ "
                << selected_mode_.
                       FramesPerSecond()
                << " "
                << selected_mode_.
                       pixel_format;
        }

        return text.str();

    case CaptureSessionState::SourceLost:
        return "Capture device disconnected";

    case CaptureSessionState::Error:
        return "Capture device error";
    }

    return "Device capture";
}

}  // namespace fps::capture
