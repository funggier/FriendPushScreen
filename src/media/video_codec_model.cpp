#include "media/video_codec_model.h"

#include <limits>

namespace fps::media {
namespace {

int CandidateScore(
    const VideoCodecCandidate& candidate,
    VideoBackendPolicy policy) noexcept {
    int score = 0;

    switch (policy) {
    case VideoBackendPolicy::PreferHardware:
        switch (candidate.classification) {
        case VideoBackendClassification::Hardware:
            score = 300;
            break;
        case VideoBackendClassification::Software:
            score = 200;
            break;
        case VideoBackendClassification::Unknown:
            score = 100;
            break;
        }
        break;

    case VideoBackendPolicy::RequireHardware:
        if (candidate.classification !=
            VideoBackendClassification::Hardware) {
            return std::numeric_limits<int>::min();
        }
        score = 300;
        break;

    case VideoBackendPolicy::ForceSoftware:
        if (candidate.classification !=
            VideoBackendClassification::Software) {
            return std::numeric_limits<int>::min();
        }
        score = 300;
        break;
    }

    if (candidate.d3d11_aware) {
        score += 20;
    }

    return score;
}

}  // namespace

const char* VideoCodecKindName(
    VideoCodecKind codec) noexcept {
    switch (codec) {
    case VideoCodecKind::H264:
        return "H.264";
    case VideoCodecKind::HEVC:
        return "HEVC";
    case VideoCodecKind::AV1:
        return "AV1";
    }

    return "Unknown";
}

bool IsValidVideoCodecConfig(
    const VideoCodecConfig& config) noexcept {
    return config.width > 0 &&
        config.height > 0 &&
        config.fps_numerator > 0 &&
        config.fps_denominator > 0 &&
        config.target_bitrate_bps > 0 &&
        config.keyframe_interval_frames > 0;
}

std::optional<VideoCodecCandidate>
SelectVideoCodecCandidate(
    const std::vector<VideoCodecCandidate>& candidates,
    VideoCodecKind codec,
    VideoCodecDirection direction,
    VideoBackendPolicy policy) {
    const VideoCodecCandidate* best = nullptr;
    int best_score =
        std::numeric_limits<int>::min();

    for (const auto& candidate : candidates) {
        if (candidate.codec != codec ||
            candidate.direction != direction) {
            continue;
        }

        const int score =
            CandidateScore(
                candidate,
                policy);

        if (score > best_score) {
            best = &candidate;
            best_score = score;
        }
    }

    if (best == nullptr ||
        best_score ==
            std::numeric_limits<int>::min()) {
        return std::nullopt;
    }

    return *best;
}

}  // namespace fps::media
