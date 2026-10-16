#pragma once

#include "media/video_codec_model.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <vector>

namespace fps::media {

struct MftVideoCodecDescriptor {
    VideoCodecCandidate candidate;
    GUID clsid{};
    bool asynchronous{false};
    std::wstring hardware_url;
};

// Stable, lossless registration identity; friendly names are display text only.
std::string MftVideoCodecIdentity(const MftVideoCodecDescriptor& descriptor);

std::vector<MftVideoCodecDescriptor>
EnumerateMftVideoCodecDescriptors(
    VideoCodecKind codec,
    VideoCodecDirection direction);

}  // namespace fps::media
