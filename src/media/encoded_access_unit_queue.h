#pragma once

#include "media/video_codec_model.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace fps::media {

struct EncodedAccessUnit {
    VideoCodecKind codec{VideoCodecKind::H264};
    std::uint64_t sequence{0};
    std::uint64_t source_sequence{0};
    std::int64_t timestamp_100ns{0};
    std::int64_t duration_100ns{0};
    bool keyframe{false};
    std::vector<std::uint8_t> bytes;
};

enum class EncodedQueuePushResult {
    Queued,
    DroppedAwaitingKeyframe,
    OverflowResetAwaitingKeyframe,
    OverflowResetKeyframeQueued,
    RecoveryKeyframeQueued
};

struct EncodedQueueStats {
    std::uint64_t pushed{0};
    std::uint64_t popped{0};
    std::uint64_t dropped{0};
    std::uint64_t overflow_events{0};
    std::uint64_t recovery_resets{0};
    std::size_t max_depth{0};
};

class BoundedEncodedQueue {
public:
    explicit BoundedEncodedQueue(
        std::size_t capacity);

    EncodedQueuePushResult Push(
        EncodedAccessUnit unit);

    std::optional<EncodedAccessUnit> Pop();

    void Clear() noexcept;

    std::size_t Size() const noexcept {
        return queue_.size();
    }

    std::size_t Capacity() const noexcept {
        return capacity_;
    }

    bool AwaitingKeyframe() const noexcept {
        return awaiting_keyframe_;
    }

    const EncodedQueueStats& Stats()
        const noexcept {
        return stats_;
    }

private:
    void RecordDepth() noexcept;

    std::size_t capacity_{1};
    std::deque<EncodedAccessUnit> queue_;
    bool awaiting_keyframe_{false};
    EncodedQueueStats stats_{};
};

}  // namespace fps::media
