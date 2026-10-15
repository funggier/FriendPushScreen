#pragma once

#include "media/video_codec_model.h"

#include <mfapi.h>

namespace fps::media {

enum class MftVideoMediaTypeRole {
    RawNv12,
    Encoded
};

struct MftVideoMediaTypeSpec {
    MftVideoMediaTypeRole role{
        MftVideoMediaTypeRole::RawNv12};
    VideoCodecKind codec{
        VideoCodecKind::H264};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t fps_numerator{0};
    std::uint32_t fps_denominator{1};
    std::uint32_t bitrate_bps{0};
};

MftVideoMediaTypeSpec
MakeEncoderInputTypeSpec(
    const VideoCodecConfig& config);

MftVideoMediaTypeSpec
MakeEncoderOutputTypeSpec(
    const VideoCodecConfig& config);

MftVideoMediaTypeSpec
MakeDecoderInputTypeSpec(
    const VideoCodecConfig& config);

MftVideoMediaTypeSpec
MakeDecoderOutputTypeSpec(
    const VideoCodecConfig& config);

HRESULT CreateMftVideoMediaType(
    const MftVideoMediaTypeSpec& spec,
    IMFMediaType** output);

}  // namespace fps::media
