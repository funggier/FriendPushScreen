#include "media/capture_device_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace fps::media {
namespace {

class MediaFoundationScope {
public:
    MediaFoundationScope() {
        hr_ = MFStartup(
            MF_VERSION,
            MFSTARTUP_LITE);
    }

    ~MediaFoundationScope() {
        if (SUCCEEDED(hr_)) {
            MFShutdown();
        }
    }

    bool Ok() const noexcept {
        return SUCCEEDED(hr_);
    }

    HRESULT Result() const noexcept {
        return hr_;
    }

private:
    HRESULT hr_{E_FAIL};
};

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

HRESULT EnumerateActivates(
    IMFActivate*** activates,
    UINT32* count) {
    if (activates == nullptr ||
        count == nullptr) {
        return E_POINTER;
    }

    *activates = nullptr;
    *count = 0;

    Microsoft::WRL::ComPtr<
        IMFAttributes>
        attributes;

    HRESULT hr =
        MFCreateAttributes(
            &attributes,
            1);

    if (FAILED(hr)) {
        return hr;
    }

    hr = attributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)) {
        return hr;
    }

    return MFEnumDeviceSources(
        attributes.Get(),
        activates,
        count);
}

Microsoft::WRL::ComPtr<IMFActivate>
FindActivation(
    std::string_view symbolic_link) {
    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    const HRESULT hr =
        EnumerateActivates(
            &activates,
            &count);

    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<IMFActivate>
        match;

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
            match = activate;
        }

        activate->Release();

        if (match != nullptr) {
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
    return match;
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

    wchar_t guid_text[64]{};
    const int written =
        StringFromGUID2(
            subtype,
            guid_text,
            static_cast<int>(
                std::size(guid_text)));

    if (written <= 0) {
        return "Unknown";
    }

    return WideToUtf8(guid_text);
}

}  // namespace

std::vector<diagnostics::CaptureDeviceInfo>
ProbeVideoCaptureDevices() {
    std::vector<
        diagnostics::CaptureDeviceInfo>
        devices;

    MediaFoundationScope mf;
    if (!mf.Ok()) {
        return devices;
    }

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    const HRESULT hr =
        EnumerateActivates(
            &activates,
            &count);

    if (FAILED(hr)) {
        return devices;
    }

    for (UINT32 index = 0;
         index < count;
         ++index) {
        IMFActivate* activate =
            activates[index];

        if (activate == nullptr) {
            continue;
        }

        diagnostics::CaptureDeviceInfo
            info{};

        info.friendly_name =
            ReadAllocatedString(
                activate,
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);

        info.symbolic_link =
            ReadAllocatedString(
                activate,
                MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);

        devices.push_back(
            std::move(info));

        activate->Release();
    }

    CoTaskMemFree(activates);
    return devices;
}

std::vector<VideoCaptureMode>
ProbeVideoCaptureDeviceModes(
    std::string_view symbolic_link) {
    std::vector<VideoCaptureMode>
        modes;

    if (symbolic_link.empty()) {
        return modes;
    }

    MediaFoundationScope mf;
    if (!mf.Ok()) {
        return modes;
    }

    auto activate =
        FindActivation(
            symbolic_link);

    if (activate == nullptr) {
        return modes;
    }

    Microsoft::WRL::ComPtr<
        IMFMediaSource>
        media_source;

    HRESULT hr =
        activate->ActivateObject(
            IID_PPV_ARGS(
                &media_source));

    if (FAILED(hr)) {
        return modes;
    }

    Microsoft::WRL::ComPtr<
        IMFSourceReader>
        reader;

    hr =
        MFCreateSourceReaderFromMediaSource(
            media_source.Get(),
            nullptr,
            &reader);

    if (FAILED(hr)) {
        media_source->Shutdown();
        return modes;
    }

    for (DWORD index = 0;; ++index) {
        Microsoft::WRL::ComPtr<
            IMFMediaType>
            media_type;

        hr = reader->GetNativeMediaType(
            static_cast<DWORD>(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            index,
            &media_type);

        if (hr == MF_E_NO_MORE_TYPES) {
            break;
        }

        if (FAILED(hr)) {
            break;
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
        std::string format =
            "Unknown";

        if (SUCCEEDED(
                media_type->GetGUID(
                    MF_MT_SUBTYPE,
                    &subtype))) {
            format =
                SubtypeName(subtype);
        }

        VideoCaptureMode mode{};
        mode.width = width;
        mode.height = height;
        mode.frame_rate_num =
            rate_num;
        mode.frame_rate_den =
            rate_den == 0
                ? 1
                : rate_den;
        mode.pixel_format =
            std::move(format);

        modes.push_back(
            std::move(mode));
    }

    media_source->Shutdown();

    std::sort(
        modes.begin(),
        modes.end(),
        [](const auto& left,
           const auto& right) {
            return std::tie(
                       left.width,
                       left.height,
                       left.frame_rate_num,
                       left.frame_rate_den,
                       left.pixel_format) <
                std::tie(
                       right.width,
                       right.height,
                       right.frame_rate_num,
                       right.frame_rate_den,
                       right.pixel_format);
        });

    modes.erase(
        std::unique(
            modes.begin(),
            modes.end(),
            [](const auto& left,
               const auto& right) {
                return left.width ==
                           right.width &&
                    left.height ==
                           right.height &&
                    left.frame_rate_num ==
                           right.frame_rate_num &&
                    left.frame_rate_den ==
                           right.frame_rate_den &&
                    left.pixel_format ==
                           right.pixel_format;
            }),
        modes.end());

    return modes;
}

diagnostics::ProbeResult
MakeMediaEnumerationResult(
    std::string_view name,
    long error_code,
    std::size_t count) {
    diagnostics::ProbeResult result{};
    result.name =
        std::string{name};
    result.error_code =
        error_code;

    if (error_code != 0) {
        result.state =
            diagnostics::ProbeState::Error;
        result.message =
            "Media Foundation enumeration failed";
    } else if (count == 0) {
        result.state =
            diagnostics::ProbeState::Unavailable;
        result.message =
            "No matching media capability was found";
    } else {
        result.state =
            diagnostics::ProbeState::Available;
    }

    return result;
}

}  // namespace fps::media
