#include "media/mft_video_transform_activation.h"

#include <mfapi.h>
#include <mferror.h>

#include <string>

namespace fps::media {
namespace {

const GUID& CodecSubtype(
    VideoCodecKind codec) noexcept {
    switch (codec) {
    case VideoCodecKind::H264:
        return MFVideoFormat_H264;
    case VideoCodecKind::HEVC:
        return MFVideoFormat_HEVC;
    case VideoCodecKind::AV1:
        return MFVideoFormat_AV1;
    }

    return MFVideoFormat_H264;
}

bool SameGuid(
    const GUID& left,
    const GUID& right) noexcept {
    return InlineIsEqualGUID(
               left,
               right) != FALSE;
}

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

std::string FriendlyName(
    IMFAttributes* activation) {
    if (activation == nullptr) {
        return {};
    }

    wchar_t* value = nullptr;
    UINT32 length = 0;

    const HRESULT hr =
        activation->GetAllocatedString(
            MFT_FRIENDLY_NAME_Attribute,
            &value,
            &length);

    std::string output;

    if (SUCCEEDED(hr) &&
        value != nullptr) {
        output =
            WideToUtf8(value);
    }

    CoTaskMemFree(value);
    return output;
}

bool HardwareEvidence(
    IMFAttributes* activation) {
    if (activation == nullptr) {
        return false;
    }

    wchar_t* value = nullptr;
    UINT32 length = 0;

    const HRESULT hr =
        activation->GetAllocatedString(
            MFT_ENUM_HARDWARE_URL_Attribute,
            &value,
            &length);

    const bool result =
        SUCCEEDED(hr) &&
        value != nullptr &&
        length > 0;

    CoTaskMemFree(value);
    return result;
}

bool MatchesDescriptor(
    IMFAttributes* activation,
    const MftVideoCodecDescriptor& descriptor) {
    if (activation == nullptr) {
        return false;
    }

    GUID clsid{};

    if (FAILED(
            activation->GetGUID(
                MFT_TRANSFORM_CLSID_Attribute,
                &clsid))) {
        return false;
    }

    if (!SameGuid(
            clsid,
            descriptor.clsid)) {
        return false;
    }

    if (!descriptor.candidate.name.empty() &&
        FriendlyName(activation) !=
            descriptor.candidate.name) {
        return false;
    }

    const bool hardware =
        HardwareEvidence(activation);

    if (descriptor.candidate.classification ==
            VideoBackendClassification::Hardware &&
        !hardware) {
        return false;
    }

    if (descriptor.candidate.classification ==
            VideoBackendClassification::Software &&
        hardware) {
        return false;
    }

    if(hardware) {
        wchar_t* url=nullptr;UINT32 length=0;
        const HRESULT hr=activation->GetAllocatedString(MFT_ENUM_HARDWARE_URL_Attribute,&url,&length);
        const bool same=SUCCEEDED(hr) && url && !descriptor.hardware_url.empty() &&
            descriptor.hardware_url==url;
        CoTaskMemFree(url);
        if(!same)return false;
    }
    return true;
}

}  // namespace

bool MatchesMftVideoCodecDescriptor(IMFAttributes* activation,
    const MftVideoCodecDescriptor& descriptor) {
    return MatchesDescriptor(activation,descriptor);
}

HRESULT ActivateMftVideoCodecDescriptor(
    const MftVideoCodecDescriptor& descriptor,
    Microsoft::WRL::ComPtr<IMFActivate>& activation,
    Microsoft::WRL::ComPtr<IMFTransform>& transform) {
    activation.Reset();
    transform.Reset();

    MFT_REGISTER_TYPE_INFO encoded_type{
        MFMediaType_Video,
        CodecSubtype(
            descriptor.candidate.codec)};

    const GUID category =
        descriptor.candidate.direction ==
                VideoCodecDirection::Encode
            ? MFT_CATEGORY_VIDEO_ENCODER
            : MFT_CATEGORY_VIDEO_DECODER;

    const MFT_REGISTER_TYPE_INFO* input =
        descriptor.candidate.direction ==
                VideoCodecDirection::Decode
            ? &encoded_type
            : nullptr;

    const MFT_REGISTER_TYPE_INFO* output =
        descriptor.candidate.direction ==
                VideoCodecDirection::Encode
            ? &encoded_type
            : nullptr;

    IMFActivate** raw = nullptr;
    UINT32 count = 0;

    const HRESULT enum_hr =
        MFTEnumEx(
            category,
            MFT_ENUM_FLAG_ALL,
            input,
            output,
            &raw,
            &count);

    if (FAILED(enum_hr)) {
        return enum_hr;
    }

    HRESULT result =
        MF_E_TOPO_CODEC_NOT_FOUND;

    for (UINT32 index = 0;
         index < count;
         ++index) {
        IMFActivate* item =
            raw[index];

        if (item == nullptr ||
            !MatchesMftVideoCodecDescriptor(
                item,
                descriptor)) {
            continue;
        }

        Microsoft::WRL::ComPtr<IMFTransform>
            candidate_transform;

        const HRESULT activate_hr =
            item->ActivateObject(
                IID_PPV_ARGS(
                    &candidate_transform));

        if (FAILED(activate_hr)) {
            result = activate_hr;
            continue;
        }

        // ComPtr assignment retains the activation before the enumeration releases it.
        activation = item;
        transform =
            std::move(
                candidate_transform);
        result = S_OK;
        break;
    }

    if (raw != nullptr) {
        for (UINT32 index = 0;
             index < count;
             ++index) {
            if (raw[index] != nullptr) {
                raw[index]->Release();
            }
        }

        CoTaskMemFree(raw);
    }

    return result;
}

}  // namespace fps::media
