#pragma once

#include <cstddef>
#include <cstdint>

namespace fps::media {

struct VideoLatencyStats {
    std::uint64_t samples{0};
    std::int64_t total_100ns{0};
    std::int64_t max_100ns{0};

    std::int64_t Average100ns()
        const noexcept {
        return samples == 0
            ? 0
            : total_100ns /
                  static_cast<std::int64_t>(
                      samples);
    }
};

struct VideoLoopMetricsSnapshot {
    std::uint64_t capture_frames{0};
    std::uint64_t encoder_submissions{0};
    std::uint64_t encoded_access_units{0};
    std::uint64_t decoded_frames{0};
    std::uint64_t rendered_frames{0};
    std::uint64_t dropped_frames{0};
    std::uint64_t recovery_resets{0};
    std::size_t current_queue_depth{0};
    std::size_t max_queue_depth{0};
    VideoLatencyStats encode_latency;
    VideoLatencyStats decode_latency;
    VideoLatencyStats end_to_end_latency;
};

class VideoLoopMetrics {
public:
    void Reset() noexcept;

    void OnCaptureFrame() noexcept;
    void OnEncoderSubmission() noexcept;
    void OnEncoded(
        std::int64_t latency_100ns) noexcept;
    void OnDecoded(
        std::int64_t latency_100ns) noexcept;
    void OnRendered(
        std::int64_t end_to_end_latency_100ns)
        noexcept;

    void OnDropped(
        std::uint64_t count = 1) noexcept;
    void OnRecoveryReset() noexcept;

    void SetQueueDepth(
        std::size_t depth) noexcept;

    const VideoLoopMetricsSnapshot& Snapshot()
        const noexcept {
        return snapshot_;
    }

private:
    static void AddLatency(
        VideoLatencyStats& stats,
        std::int64_t latency_100ns) noexcept;

    VideoLoopMetricsSnapshot snapshot_{};
};

}  // namespace fps::media
