#include "media/video_frame_clock.h"

#include <algorithm>
#include <cstdint>

namespace fps::media {
namespace {

std::int64_t CalculateFrameDuration(
    std::uint32_t fps_numerator,
    std::uint32_t fps_denominator) noexcept {
    if (fps_numerator == 0 ||
        fps_denominator == 0) {
        return 166'667;
    }

    constexpr std::int64_t kUnitsPerSecond =
        10'000'000;

    const std::int64_t numerator =
        kUnitsPerSecond *
        static_cast<std::int64_t>(
            fps_denominator);

    return std::max<std::int64_t>(
        1,
        (numerator +
         static_cast<std::int64_t>(
             fps_numerator / 2)) /
            static_cast<std::int64_t>(
                fps_numerator));
}

}  // namespace

VideoFrameClock::VideoFrameClock(
    std::uint32_t fps_numerator,
    std::uint32_t fps_denominator)
    : frame_duration_100ns_(
          CalculateFrameDuration(
              fps_numerator,
              fps_denominator)) {}

NormalizedVideoTiming
VideoFrameClock::Normalize(
    std::uint64_t source_sequence,
    std::int64_t source_timestamp_100ns) {
    NormalizedVideoTiming timing{};
    timing.codec_sequence =
        next_codec_sequence_++;
    timing.source_sequence =
        source_sequence;
    timing.source_timestamp_100ns =
        source_timestamp_100ns;
    timing.duration_100ns =
        frame_duration_100ns_;

    if (last_timestamp_100ns_ < 0) {
        if (source_timestamp_100ns >= 0) {
            timing.timestamp_100ns =
                source_timestamp_100ns;
        } else {
            timing.timestamp_100ns = 0;
            timing.repaired = true;
        }
    } else if (
        source_timestamp_100ns >
        last_timestamp_100ns_) {
        timing.timestamp_100ns =
            source_timestamp_100ns;
    } else {
        timing.timestamp_100ns =
            last_timestamp_100ns_ +
            frame_duration_100ns_;
        timing.repaired = true;
    }

    last_timestamp_100ns_ =
        timing.timestamp_100ns;

    return timing;
}

void VideoFrameClock::Reset() noexcept {
    next_codec_sequence_ = 1;
    last_timestamp_100ns_ = -1;
}

}  // namespace fps::media
