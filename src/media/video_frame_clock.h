#pragma once

#include <cstdint>

namespace fps::media {

struct NormalizedVideoTiming {
    std::uint64_t codec_sequence{0};
    std::uint64_t source_sequence{0};
    std::int64_t source_timestamp_100ns{0};
    std::int64_t timestamp_100ns{0};
    std::int64_t duration_100ns{0};
    bool repaired{false};
};

class VideoFrameClock {
public:
    VideoFrameClock(
        std::uint32_t fps_numerator,
        std::uint32_t fps_denominator);

    NormalizedVideoTiming Normalize(
        std::uint64_t source_sequence,
        std::int64_t source_timestamp_100ns);

    void Reset() noexcept;

    std::int64_t FrameDuration100ns()
        const noexcept {
        return frame_duration_100ns_;
    }

private:
    std::int64_t frame_duration_100ns_{
        166'667};
    std::uint64_t next_codec_sequence_{1};
    std::int64_t last_timestamp_100ns_{-1};
};

}  // namespace fps::media
