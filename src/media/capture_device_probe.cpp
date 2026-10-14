#include "media/capture_device_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>

#include <string>
#include <vector>

namespace fps::media {
namespace {

std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }

    const int length = static_cast<int>(wcslen(value));
    const int required = WideCharToMultiByte(
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

    std::string output(static_cast<std::size_t>(required), '\0');
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

std::string ReadAllocatedString(IMFActivate* activate, REFGUID key) {
    wchar_t* value = nullptr;
    UINT32 length = 0;
    const HRESULT hr = activate->GetAllocatedString(key, &value, &length);
    if (FAILED(hr) || value == nullptr) {
        CoTaskMemFree(value);
        return {};
    }

    std::string result = WideToUtf8(value);
    CoTaskMemFree(value);
    return result;
}

}  // namespace

std::vector<diagnostics::CaptureDeviceInfo> ProbeVideoCaptureDevices() {
    std::vector<diagnostics::CaptureDeviceInfo> devices;

    Microsoft::WRL::ComPtr<IMFAttributes> attributes;
    HRESULT hr = MFCreateAttributes(&attributes, 1);
    if (FAILED(hr)) {
        return devices;
    }

    hr = attributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        return devices;
    }

    IMFActivate** activates = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(attributes.Get(), &activates, &count);
    if (FAILED(hr)) {
        return devices;
    }

    for (UINT32 index = 0; index < count; ++index) {
        IMFActivate* activate = activates[index];
        if (activate == nullptr) {
            continue;
        }

        diagnostics::CaptureDeviceInfo info{};
        info.friendly_name = ReadAllocatedString(
            activate,
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
        info.symbolic_link = ReadAllocatedString(
            activate,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
        devices.push_back(std::move(info));
        activate->Release();
    }

    CoTaskMemFree(activates);
    return devices;
}

diagnostics::ProbeResult MakeMediaEnumerationResult(
    std::string_view name,
    long error_code,
    std::size_t count) {
    diagnostics::ProbeResult result{};
    result.name = std::string{name};
    result.error_code = error_code;

    if (error_code != 0) {
        result.state = diagnostics::ProbeState::Error;
        result.message = "Media Foundation enumeration failed";
    } else if (count == 0) {
        result.state = diagnostics::ProbeState::Unavailable;
        result.message = "No matching media capability was found";
    } else {
        result.state = diagnostics::ProbeState::Available;
    }

    return result;
}

}  // namespace fps::media
