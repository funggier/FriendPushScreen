#include "media/video_loop_metrics.h"

#include "test_support.h"

#include <cstddef>
#include <cstdint>

void RunVideoLoopMetricsTests() {
    fps::media::VideoLoopMetrics metrics;

    metrics.OnCaptureFrame();
    metrics.OnCaptureFrame();
    metrics.OnEncoderSubmission();
    metrics.OnEncoded(100);
    metrics.OnEncoded(300);
    metrics.OnDecoded(200);
    metrics.OnRendered(900);
    metrics.OnDropped(2);
    metrics.OnRecoveryReset();
    metrics.SetQueueDepth(1);
    metrics.SetQueueDepth(3);
    metrics.SetQueueDepth(2);

    const auto& snapshot =
        metrics.Snapshot();

    FPS_EXPECT_EQ(
        snapshot.capture_frames,
        std::uint64_t{2});
    FPS_EXPECT_EQ(
        snapshot.encoder_submissions,
        std::uint64_t{1});
    FPS_EXPECT_EQ(
        snapshot.encoded_access_units,
        std::uint64_t{2});
    FPS_EXPECT_EQ(
        snapshot.decoded_frames,
        std::uint64_t{1});
    FPS_EXPECT_EQ(
        snapshot.rendered_frames,
        std::uint64_t{1});
    FPS_EXPECT_EQ(
        snapshot.dropped_frames,
        std::uint64_t{2});
    FPS_EXPECT_EQ(
        snapshot.recovery_resets,
        std::uint64_t{1});
    FPS_EXPECT_EQ(
        snapshot.current_queue_depth,
        std::size_t{2});
    FPS_EXPECT_EQ(
        snapshot.max_queue_depth,
        std::size_t{3});

    FPS_EXPECT_EQ(
        snapshot.encode_latency.samples,
        std::uint64_t{2});
    FPS_EXPECT_EQ(
        snapshot.encode_latency.Average100ns(),
        std::int64_t{200});
    FPS_EXPECT_EQ(
        snapshot.encode_latency.max_100ns,
        std::int64_t{300});

    FPS_EXPECT_EQ(
        snapshot.decode_latency.Average100ns(),
        std::int64_t{200});
    FPS_EXPECT_EQ(
        snapshot.end_to_end_latency.
            Average100ns(),
        std::int64_t{900});

    metrics.OnDecoded(-50);

    FPS_EXPECT_EQ(
        metrics.Snapshot().
            decode_latency.max_100ns,
        std::int64_t{200});

    metrics.Reset();

    FPS_EXPECT_EQ(
        metrics.Snapshot().
            capture_frames,
        std::uint64_t{0});
    FPS_EXPECT_EQ(
        metrics.Snapshot().
            encode_latency.samples,
        std::uint64_t{0});
}
