#pragma once

#include "media/video_codec_model.h"

#include <mfapi.h>
#include <span>
#include <vector>
#include <cstdint>

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


struct DecodedNv12Layout {
    std::uint32_t coded_width{0}, coded_height{0};
    std::uint32_t width{0}, height{0}, stride{0};
};
HRESULT ResolveDecodedNv12Layout(IMFMediaType* type,
    std::uint32_t width, std::uint32_t height, DecodedNv12Layout& layout);
HRESULT CopyNv12VisibleRows(std::span<const std::uint8_t> input,
    const DecodedNv12Layout& layout, std::vector<std::uint8_t>& output);

}  // namespace fps::media
