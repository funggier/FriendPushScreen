#include "media/codec_capability_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mfapi.h>
#include <mftransform.h>

#include <array>
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

bool HasHardwareEvidence(IMFActivate* activate) {
    wchar_t* value = nullptr;
    UINT32 length = 0;
    const HRESULT hr = activate->GetAllocatedString(
        MFT_ENUM_HARDWARE_URL_Attribute,
        &value,
        &length);
    const bool present = SUCCEEDED(hr) && value != nullptr && length > 0;
    CoTaskMemFree(value);
    return present;
}

void EnumerateCodecDirection(
    diagnostics::CodecKind codec,
    diagnostics::CodecDirection direction,
    const GUID& subtype,
    std::vector<diagnostics::CodecCapabilityInfo>& output) {
    MFT_REGISTER_TYPE_INFO type_info{MFMediaType_Video, subtype};

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    const GUID category =
        direction == diagnostics::CodecDirection::Encode
            ? MFT_CATEGORY_VIDEO_ENCODER
            : MFT_CATEGORY_VIDEO_DECODER;

    const MFT_REGISTER_TYPE_INFO* input_type =
        direction == diagnostics::CodecDirection::Decode ? &type_info : nullptr;
    const MFT_REGISTER_TYPE_INFO* output_type =
        direction == diagnostics::CodecDirection::Encode ? &type_info : nullptr;

    const HRESULT hr = MFTEnumEx(
        category,
        MFT_ENUM_FLAG_ALL,
        input_type,
        output_type,
        &activates,
        &count);
    if (FAILED(hr)) {
        return;
    }

    for (UINT32 index = 0; index < count; ++index) {
        IMFActivate* activate = activates[index];
        if (activate == nullptr) {
            continue;
        }

        const std::string name = ReadAllocatedString(
            activate,
            MFT_FRIENDLY_NAME_Attribute);
        const bool hardware = HasHardwareEvidence(activate);

        output.push_back(NormalizeCodecCapability(
            codec,
            direction,
            name,
            hardware,
            false));

        activate->Release();
    }

    CoTaskMemFree(activates);
}

}  // namespace

diagnostics::CodecCapabilityInfo NormalizeCodecCapability(
    diagnostics::CodecKind codec,
    diagnostics::CodecDirection direction,
    std::string name,
    bool hardware_evidence,
    bool software_evidence) {
    diagnostics::CodecCapabilityInfo info{};
    info.codec = codec;
    info.direction = direction;
    info.name = std::move(name);

    if (hardware_evidence) {
        info.hardware = diagnostics::HardwareClassification::Hardware;
    } else if (software_evidence) {
        info.hardware = diagnostics::HardwareClassification::Software;
    } else {
        info.hardware = diagnostics::HardwareClassification::Unknown;
    }

    return info;
}

std::vector<diagnostics::CodecCapabilityInfo> ProbeCodecCapabilities() {
    std::vector<diagnostics::CodecCapabilityInfo> capabilities;

    struct CodecDefinition {
        diagnostics::CodecKind codec;
        const GUID* subtype;
    };

    const std::array<CodecDefinition, 3> codecs{{
        {diagnostics::CodecKind::H264, &MFVideoFormat_H264},
        {diagnostics::CodecKind::HEVC, &MFVideoFormat_HEVC},
        {diagnostics::CodecKind::AV1, &MFVideoFormat_AV1},
    }};

    for (const auto& codec : codecs) {
        EnumerateCodecDirection(
            codec.codec,
            diagnostics::CodecDirection::Encode,
            *codec.subtype,
            capabilities);
        EnumerateCodecDirection(
            codec.codec,
            diagnostics::CodecDirection::Decode,
            *codec.subtype,
            capabilities);
    }

    return capabilities;
}

}  // namespace fps::media
