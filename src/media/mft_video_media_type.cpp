#include "media/mft_video_media_type.h"

#include <wrl/client.h>

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

}  // namespace fps::media
