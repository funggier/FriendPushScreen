#include "media/encoded_access_unit_queue.h"

#include <algorithm>
#include <utility>

namespace fps::media {

BoundedEncodedQueue::BoundedEncodedQueue(
    std::size_t capacity)
    : capacity_(std::max<std::size_t>(
          1,
          capacity)) {}

void BoundedEncodedQueue::RecordDepth()
    noexcept {
    stats_.max_depth =
        std::max(
            stats_.max_depth,
            queue_.size());
}

EncodedQueuePushResult
BoundedEncodedQueue::Push(
    EncodedAccessUnit unit) {
    ++stats_.pushed;

    if (awaiting_keyframe_) {
        if (!unit.keyframe) {
            ++stats_.dropped;
            return EncodedQueuePushResult::
                DroppedAwaitingKeyframe;
        }

        queue_.clear();
        queue_.push_back(
            std::move(unit));
        awaiting_keyframe_ = false;
        RecordDepth();

        return EncodedQueuePushResult::
            RecoveryKeyframeQueued;
    }

    if (queue_.size() < capacity_) {
        queue_.push_back(
            std::move(unit));
        RecordDepth();

        return EncodedQueuePushResult::Queued;
    }

    ++stats_.overflow_events;
    ++stats_.recovery_resets;
    stats_.dropped += queue_.size();
    queue_.clear();

    if (unit.keyframe) {
        queue_.push_back(
            std::move(unit));
        RecordDepth();

        return EncodedQueuePushResult::
            OverflowResetKeyframeQueued;
    }

    ++stats_.dropped;
    awaiting_keyframe_ = true;

    return EncodedQueuePushResult::
        OverflowResetAwaitingKeyframe;
}

std::optional<EncodedAccessUnit>
BoundedEncodedQueue::Pop() {
    if (queue_.empty()) {
        return std::nullopt;
    }

    EncodedAccessUnit unit =
        std::move(queue_.front());
    queue_.pop_front();
    ++stats_.popped;

    return unit;
}

void BoundedEncodedQueue::Clear() noexcept {
    queue_.clear();
    awaiting_keyframe_ = false;
}

}  // namespace fps::media
