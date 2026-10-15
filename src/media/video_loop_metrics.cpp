#include "media/video_loop_metrics.h"

#include <algorithm>

namespace fps::media {

void VideoLoopMetrics::Reset() noexcept {
    snapshot_ = {};
}

void VideoLoopMetrics::AddLatency(
    VideoLatencyStats& stats,
    std::int64_t latency_100ns) noexcept {
    const std::int64_t clamped =
        std::max<std::int64_t>(
            0,
            latency_100ns);

    ++stats.samples;
    stats.total_100ns += clamped;
    stats.max_100ns =
        std::max(
            stats.max_100ns,
            clamped);
}

void VideoLoopMetrics::OnCaptureFrame()
    noexcept {
    ++snapshot_.capture_frames;
}

void VideoLoopMetrics::OnEncoderSubmission()
    noexcept {
    ++snapshot_.encoder_submissions;
}

void VideoLoopMetrics::OnEncoded(
    std::int64_t latency_100ns) noexcept {
    ++snapshot_.encoded_access_units;
    AddLatency(
        snapshot_.encode_latency,
        latency_100ns);
}

void VideoLoopMetrics::OnDecoded(
    std::int64_t latency_100ns) noexcept {
    ++snapshot_.decoded_frames;
    AddLatency(
        snapshot_.decode_latency,
        latency_100ns);
}

void VideoLoopMetrics::OnRendered(
    std::int64_t end_to_end_latency_100ns)
    noexcept {
    ++snapshot_.rendered_frames;
    AddLatency(
        snapshot_.end_to_end_latency,
        end_to_end_latency_100ns);
}

void VideoLoopMetrics::OnDropped(
    std::uint64_t count) noexcept {
    snapshot_.dropped_frames += count;
}

void VideoLoopMetrics::OnRecoveryReset()
    noexcept {
    ++snapshot_.recovery_resets;
}

void VideoLoopMetrics::SetQueueDepth(
    std::size_t depth) noexcept {
    snapshot_.current_queue_depth =
        depth;
    snapshot_.max_queue_depth =
        std::max(
            snapshot_.max_queue_depth,
            depth);
}

}  // namespace fps::media
