#include "media/mft_video_media_type.h"

#include <wrl/client.h>
#include <mferror.h>
#include <cstring>
#include <climits>

namespace fps::media {
namespace {

const GUID& EncodedSubtype(
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

MftVideoMediaTypeSpec MakeSpec(
    const VideoCodecConfig& config,
    MftVideoMediaTypeRole role) {
    MftVideoMediaTypeSpec spec{};
    spec.role = role;
    spec.codec = config.codec;
    spec.width = config.width;
    spec.height = config.height;
    spec.fps_numerator =
        config.fps_numerator;
    spec.fps_denominator =
        config.fps_denominator;
    spec.bitrate_bps =
        config.target_bitrate_bps;
    return spec;
}

}  // namespace

MftVideoMediaTypeSpec
MakeEncoderInputTypeSpec(
    const VideoCodecConfig& config) {
    return MakeSpec(
        config,
        MftVideoMediaTypeRole::RawNv12);
}

MftVideoMediaTypeSpec
MakeEncoderOutputTypeSpec(
    const VideoCodecConfig& config) {
    return MakeSpec(
        config,
        MftVideoMediaTypeRole::Encoded);
}

MftVideoMediaTypeSpec
MakeDecoderInputTypeSpec(
    const VideoCodecConfig& config) {
    return MakeSpec(
        config,
        MftVideoMediaTypeRole::Encoded);
}

MftVideoMediaTypeSpec
MakeDecoderOutputTypeSpec(
    const VideoCodecConfig& config) {
    return MakeSpec(
        config,
        MftVideoMediaTypeRole::RawNv12);
}

HRESULT CreateMftVideoMediaType(
    const MftVideoMediaTypeSpec& spec,
    IMFMediaType** output) {
    if (output == nullptr ||
        spec.width == 0 ||
        spec.height == 0 ||
        spec.fps_numerator == 0 ||
        spec.fps_denominator == 0) {
        return E_INVALIDARG;
    }

    *output = nullptr;

    Microsoft::WRL::ComPtr<IMFMediaType>
        media_type;

    HRESULT hr =
        MFCreateMediaType(&media_type);

    if (FAILED(hr)) {
        return hr;
    }

    hr = media_type->SetGUID(
        MF_MT_MAJOR_TYPE,
        MFMediaType_Video);

    if (FAILED(hr)) {
        return hr;
    }

    hr = media_type->SetGUID(
        MF_MT_SUBTYPE,
        spec.role ==
                MftVideoMediaTypeRole::RawNv12
            ? MFVideoFormat_NV12
            : EncodedSubtype(spec.codec));

    if (FAILED(hr)) {
        return hr;
    }

    hr = MFSetAttributeSize(
        media_type.Get(),
        MF_MT_FRAME_SIZE,
        spec.width,
        spec.height);

    if (FAILED(hr)) {
        return hr;
    }

    hr = MFSetAttributeRatio(
        media_type.Get(),
        MF_MT_FRAME_RATE,
        spec.fps_numerator,
        spec.fps_denominator);

    if (FAILED(hr)) {
        return hr;
    }

    hr = MFSetAttributeRatio(
        media_type.Get(),
        MF_MT_PIXEL_ASPECT_RATIO,
        1,
        1);

    if (FAILED(hr)) {
        return hr;
    }

    hr = media_type->SetUINT32(
        MF_MT_INTERLACE_MODE,
        MFVideoInterlace_Progressive);

    if (FAILED(hr)) {
        return hr;
    }

    if (spec.role ==
        MftVideoMediaTypeRole::Encoded) {
        if (spec.bitrate_bps == 0) {
            return E_INVALIDARG;
        }

        hr = media_type->SetUINT32(
            MF_MT_AVG_BITRATE,
            spec.bitrate_bps);
    } else {
        hr = media_type->SetUINT32(
            MF_MT_ALL_SAMPLES_INDEPENDENT,
            TRUE);
    }

    if (FAILED(hr)) {
        return hr;
    }

    *output = media_type.Detach();
    return S_OK;
}


HRESULT ResolveDecodedNv12Layout(IMFMediaType* type, std::uint32_t width,
    std::uint32_t height, DecodedNv12Layout& layout) {
    layout={};
    if(!type || width==0 || height==0 || width%2 || height%2)return E_INVALIDARG;
    UINT32 coded_width=0,coded_height=0;
    HRESULT hr=MFGetAttributeSize(type,MF_MT_FRAME_SIZE,&coded_width,&coded_height);
    if(FAILED(hr))return hr;
    if(coded_width<width || coded_height<height || coded_width%2 || coded_height%2)return MF_E_INVALIDMEDIATYPE;
    if(coded_width!=width || coded_height!=height) {
        MFVideoArea area{};UINT32 size=0;
        hr=type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,reinterpret_cast<UINT8*>(&area),sizeof(area),&size);
        if(FAILED(hr))hr=type->GetBlob(MF_MT_GEOMETRIC_APERTURE,reinterpret_cast<UINT8*>(&area),sizeof(area),&size);
        if(FAILED(hr) || size!=sizeof(area) || area.OffsetX.value || area.OffsetX.fract ||
            area.OffsetY.value || area.OffsetY.fract || area.Area.cx!=static_cast<LONG>(width) ||
            area.Area.cy!=static_cast<LONG>(height))return MF_E_INVALIDMEDIATYPE;
    }
    UINT32 stride=0;
    if(FAILED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stride))) {
        LONG calculated=0;
        hr=MFGetStrideForBitmapInfoHeader(MFVideoFormat_NV12.Data1,coded_width,&calculated);
        if(FAILED(hr) || calculated<=0)return MF_E_INVALIDMEDIATYPE;
        stride=static_cast<UINT32>(calculated);
    }
    if(stride<coded_width || stride>static_cast<UINT32>(LONG_MAX))return MF_E_INVALIDMEDIATYPE;
    layout={coded_width,coded_height,width,height,stride};
    return S_OK;
}
HRESULT CopyNv12VisibleRows(std::span<const std::uint8_t> input,
    const DecodedNv12Layout& layout, std::vector<std::uint8_t>& output) {
    output.clear();
    if(layout.width==0 || layout.height==0 || layout.width%2 || layout.height%2 ||
        layout.coded_width<layout.width || layout.coded_height<layout.height ||
        layout.coded_width%2 || layout.coded_height%2 || layout.stride<layout.coded_width ||
        static_cast<std::uint64_t>(layout.stride)*layout.coded_height*3/2>input.size())return E_INVALIDARG;
    output.resize(static_cast<size_t>(layout.width)*layout.height*3/2);
    for(UINT row=0;row<layout.height;++row)
        std::memcpy(output.data()+static_cast<size_t>(row)*layout.width,
            input.data()+static_cast<size_t>(row)*layout.stride,layout.width);
    for(UINT row=0;row<layout.height/2;++row)
        std::memcpy(output.data()+static_cast<size_t>(layout.height+row)*layout.width,
            input.data()+static_cast<size_t>(layout.coded_height+row)*layout.stride,layout.width);
    return S_OK;
}

}  // namespace fps::media
