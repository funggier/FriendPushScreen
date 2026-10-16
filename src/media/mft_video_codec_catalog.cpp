#include "media/mft_video_codec_catalog.h"

#include <mfapi.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <string>
#include <algorithm>
#include <utility>
#include <vector>

namespace fps::media {
namespace {

using Microsoft::WRL::ComPtr;

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

std::string ReadFriendlyName(
    IMFActivate* activate) {
    wchar_t* value = nullptr;
    UINT32 length = 0;

    const HRESULT hr =
        activate->GetAllocatedString(
            MFT_FRIENDLY_NAME_Attribute,
            &value,
            &length);

    if (FAILED(hr) ||
        value == nullptr) {
        CoTaskMemFree(value);
        return {};
    }

    std::string output =
        WideToUtf8(value);

    CoTaskMemFree(value);
    return output;
}

std::wstring ReadHardwareUrl(IMFActivate* activate) {
    wchar_t* value=nullptr;UINT32 length=0;
    const HRESULT hr=activate->GetAllocatedString(MFT_ENUM_HARDWARE_URL_Attribute,&value,&length);
    std::wstring result;
    if(SUCCEEDED(hr) && value)result=value;
    CoTaskMemFree(value);return result;
}

void ReadTransformAttributes(
    IMFActivate* activate,
    bool& d3d11_aware,
    bool& asynchronous) {
    d3d11_aware = false;
    asynchronous = false;

    ComPtr<IMFTransform> transform;

    if (FAILED(
            activate->ActivateObject(
                IID_PPV_ARGS(
                    &transform)))) {
        return;
    }

    ComPtr<IMFAttributes> attributes;

    if (SUCCEEDED(
            transform->GetAttributes(
                &attributes)) &&
        attributes != nullptr) {
        UINT32 value = 0;

        if (SUCCEEDED(
                attributes->GetUINT32(
                    MF_SA_D3D11_AWARE,
                    &value))) {
            d3d11_aware = value != 0;
        }

        value = 0;

        if (SUCCEEDED(
                attributes->GetUINT32(
                    MF_TRANSFORM_ASYNC,
                    &value))) {
            asynchronous = value != 0;
        }
    }

    transform.Reset();
    activate->ShutdownObject();
}

}  // namespace

std::string MftVideoCodecIdentity(const MftVideoCodecDescriptor& descriptor) {
    wchar_t guid[40]{};
    if (StringFromGUID2(descriptor.clsid, guid, 40) == 0) return {};
    std::string id = std::string{VideoCodecKindName(descriptor.candidate.codec)} + ":" +
        (descriptor.candidate.direction == VideoCodecDirection::Encode ? "encode:" : "decode:") +
        std::to_string(static_cast<int>(descriptor.candidate.classification)) + ":" + WideToUtf8(guid) + ":";
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : WideToUtf8(descriptor.hardware_url.c_str())) {
        id.push_back(hex[c >> 4]); id.push_back(hex[c & 15]);
    }
    return id;
}

std::vector<MftVideoCodecDescriptor>
EnumerateMftVideoCodecDescriptors(
    VideoCodecKind codec,
    VideoCodecDirection direction) {
    std::vector<MftVideoCodecDescriptor>
        output;

    const GUID subtype =
        CodecSubtype(codec);

    MFT_REGISTER_TYPE_INFO encoded_type{
        MFMediaType_Video,
        subtype};

    const GUID category =
        direction ==
                VideoCodecDirection::Encode
            ? MFT_CATEGORY_VIDEO_ENCODER
            : MFT_CATEGORY_VIDEO_DECODER;

    const MFT_REGISTER_TYPE_INFO* input =
        direction ==
                VideoCodecDirection::Decode
            ? &encoded_type
            : nullptr;

    const MFT_REGISTER_TYPE_INFO* output_type =
        direction ==
                VideoCodecDirection::Encode
            ? &encoded_type
            : nullptr;

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    const HRESULT hr =
        MFTEnumEx(
            category,
            MFT_ENUM_FLAG_ALL,
            input,
            output_type,
            &activates,
            &count);

    if (FAILED(hr)) {
        return output;
    }

    output.reserve(count);

    for (UINT32 index = 0;
         index < count;
         ++index) {
        IMFActivate* activate =
            activates[index];

        if (activate == nullptr) {
            continue;
        }

        MftVideoCodecDescriptor
            descriptor{};

        descriptor.candidate.codec = codec;
        descriptor.candidate.direction =
            direction;
        descriptor.candidate.name =
            ReadFriendlyName(activate);

        descriptor.hardware_url=ReadHardwareUrl(activate);
        const bool hardware=!descriptor.hardware_url.empty();

        descriptor.candidate.classification =
            hardware
                ? VideoBackendClassification::
                      Hardware
                : VideoBackendClassification::
                      Software;

        activate->GetGUID(
            MFT_TRANSFORM_CLSID_Attribute,
            &descriptor.clsid);

        const bool duplicate=std::any_of(output.begin(),output.end(),[&](const auto& previous) {
            return IsEqualGUID(previous.clsid,descriptor.clsid) &&
                previous.candidate.name==descriptor.candidate.name &&
                previous.candidate.classification==descriptor.candidate.classification &&
                previous.hardware_url==descriptor.hardware_url;
        });
        if(duplicate){activate->Release();continue;}

        ReadTransformAttributes(
            activate,
            descriptor.candidate.d3d11_aware,
            descriptor.asynchronous);

        output.push_back(
            std::move(descriptor));

        activate->Release();
    }

    CoTaskMemFree(activates);
    return output;
}

}  // namespace fps::media
