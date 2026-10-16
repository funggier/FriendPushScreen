#include "media/encoded_access_unit_queue.h"
#include "media/video_codec_model.h"
#include "media/mft_video_codec_catalog.h"
#include "media/video_frame_clock.h"

#include "test_support.h"

#include <cstdint>
#include <string>
#include <vector>

using fps::media::BoundedEncodedQueue;
using fps::media::EncodedAccessUnit;
using fps::media::EncodedQueuePushResult;
using fps::media::SelectVideoCodecCandidate;
using fps::media::VideoBackendClassification;
using fps::media::VideoBackendPolicy;
using fps::media::VideoCodecCandidate;
using fps::media::VideoCodecConfig;
using fps::media::VideoCodecDirection;
using fps::media::VideoCodecKind;
using fps::media::VideoFrameClock;

namespace {

EncodedAccessUnit MakeUnit(
    std::uint64_t sequence,
    bool keyframe) {
    EncodedAccessUnit unit{};
    unit.codec = VideoCodecKind::H264;
    unit.sequence = sequence;
    unit.source_sequence = sequence + 100;
    unit.timestamp_100ns =
        static_cast<std::int64_t>(
            sequence * 166'667);
    unit.duration_100ns = 166'667;
    unit.keyframe = keyframe;
    unit.bytes = {
        static_cast<std::uint8_t>(
            sequence & 0xFFu)};
    return unit;
}

}  // namespace

void RunVideoPipelineModelTests() {
    {
        fps::media::MftVideoCodecDescriptor first;
        first.hardware_url = L"device-a";
        const auto id = fps::media::MftVideoCodecIdentity(first);
        first.candidate.name = "renamed by driver";
        FPS_EXPECT_EQ(id,fps::media::MftVideoCodecIdentity(first));
        auto second = first; second.hardware_url = L"device-b";
        FPS_EXPECT_TRUE(id != fps::media::MftVideoCodecIdentity(second));
        second = first; second.candidate.direction = VideoCodecDirection::Decode;
        FPS_EXPECT_TRUE(id != fps::media::MftVideoCodecIdentity(second));
    }

    {
        VideoCodecConfig config{};
        FPS_EXPECT_TRUE(
            fps::media::
                IsValidVideoCodecConfig(
                    config));

        config.width = 0;
        FPS_EXPECT_TRUE(
            !fps::media::
                IsValidVideoCodecConfig(
                    config));
    }

    {
        const std::vector<VideoCodecCandidate>
            candidates{
                {
                    VideoCodecKind::H264,
                    VideoCodecDirection::Encode,
                    "Software H264",
                    VideoBackendClassification::
                        Software,
                    false
                },
                {
                    VideoCodecKind::H264,
                    VideoCodecDirection::Encode,
                    "Hardware H264",
                    VideoBackendClassification::
                        Hardware,
                    true
                },
                {
                    VideoCodecKind::HEVC,
                    VideoCodecDirection::Encode,
                    "Hardware HEVC",
                    VideoBackendClassification::
                        Hardware,
                    true
                }
            };

        const auto preferred =
            SelectVideoCodecCandidate(
                candidates,
                VideoCodecKind::H264,
                VideoCodecDirection::Encode,
                VideoBackendPolicy::
                    PreferHardware);

        FPS_EXPECT_TRUE(
            preferred.has_value());
        FPS_EXPECT_EQ(
            preferred->name,
            std::string{"Hardware H264"});

        const auto forced_software =
            SelectVideoCodecCandidate(
                candidates,
                VideoCodecKind::H264,
                VideoCodecDirection::Encode,
                VideoBackendPolicy::
                    ForceSoftware);

        FPS_EXPECT_TRUE(
            forced_software.has_value());
        FPS_EXPECT_EQ(
            forced_software->name,
            std::string{"Software H264"});

        const auto required_hardware =
            SelectVideoCodecCandidate(
                candidates,
                VideoCodecKind::H264,
                VideoCodecDirection::Encode,
                VideoBackendPolicy::
                    RequireHardware);

        FPS_EXPECT_TRUE(
            required_hardware.has_value());
        FPS_EXPECT_EQ(
            required_hardware->classification,
            VideoBackendClassification::
                Hardware);

        const std::vector<VideoCodecCandidate>
            software_only{
                candidates.front()
            };

        const auto impossible =
            SelectVideoCodecCandidate(
                software_only,
                VideoCodecKind::H264,
                VideoCodecDirection::Encode,
                VideoBackendPolicy::
                    RequireHardware);

        FPS_EXPECT_TRUE(
            !impossible.has_value());
    }

    {
        VideoFrameClock clock{60, 1};

        FPS_EXPECT_EQ(
            clock.FrameDuration100ns(),
            std::int64_t{166'667});

        const auto first =
            clock.Normalize(10, 1'000);
        const auto repeated =
            clock.Normalize(11, 1'000);
        const auto regressed =
            clock.Normalize(12, 500);
        const auto recovered =
            clock.Normalize(13, 1'000'000);

        FPS_EXPECT_EQ(
            first.codec_sequence,
            std::uint64_t{1});
        FPS_EXPECT_EQ(
            first.timestamp_100ns,
            std::int64_t{1'000});
        FPS_EXPECT_TRUE(
            !first.repaired);

        FPS_EXPECT_EQ(
            repeated.codec_sequence,
            std::uint64_t{2});
        FPS_EXPECT_EQ(
            repeated.timestamp_100ns,
            std::int64_t{167'667});
        FPS_EXPECT_TRUE(
            repeated.repaired);

        FPS_EXPECT_EQ(
            regressed.timestamp_100ns,
            std::int64_t{334'334});
        FPS_EXPECT_TRUE(
            regressed.repaired);

        FPS_EXPECT_EQ(
            recovered.timestamp_100ns,
            std::int64_t{1'000'000});
        FPS_EXPECT_TRUE(
            !recovered.repaired);

        clock.Reset();

        const auto missing =
            clock.Normalize(20, -1);
        FPS_EXPECT_EQ(
            missing.codec_sequence,
            std::uint64_t{1});
        FPS_EXPECT_EQ(
            missing.timestamp_100ns,
            std::int64_t{0});
        FPS_EXPECT_TRUE(
            missing.repaired);
    }

    {
        BoundedEncodedQueue queue{2};

        FPS_EXPECT_EQ(
            queue.Capacity(),
            std::size_t{2});

        FPS_EXPECT_EQ(
            queue.Push(
                MakeUnit(1, true)),
            EncodedQueuePushResult::Queued);
        FPS_EXPECT_EQ(
            queue.Push(
                MakeUnit(2, false)),
            EncodedQueuePushResult::Queued);

        FPS_EXPECT_EQ(
            queue.Size(),
            std::size_t{2});
        FPS_EXPECT_EQ(
            queue.Stats().max_depth,
            std::size_t{2});

        FPS_EXPECT_EQ(
            queue.Push(
                MakeUnit(3, false)),
            EncodedQueuePushResult::
                OverflowResetAwaitingKeyframe);

        FPS_EXPECT_EQ(
            queue.Size(),
            std::size_t{0});
        FPS_EXPECT_TRUE(
            queue.AwaitingKeyframe());
        FPS_EXPECT_EQ(
            queue.Stats().overflow_events,
            std::uint64_t{1});
        FPS_EXPECT_EQ(
            queue.Stats().recovery_resets,
            std::uint64_t{1});
        FPS_EXPECT_EQ(
            queue.Stats().dropped,
            std::uint64_t{3});

        FPS_EXPECT_EQ(
            queue.Push(
                MakeUnit(4, false)),
            EncodedQueuePushResult::
                DroppedAwaitingKeyframe);

        FPS_EXPECT_EQ(
            queue.Stats().dropped,
            std::uint64_t{4});

        FPS_EXPECT_EQ(
            queue.Push(
                MakeUnit(5, true)),
            EncodedQueuePushResult::
                RecoveryKeyframeQueued);

        FPS_EXPECT_TRUE(
            !queue.AwaitingKeyframe());
        FPS_EXPECT_EQ(
            queue.Size(),
            std::size_t{1});

        const auto recovered =
            queue.Pop();

        FPS_EXPECT_TRUE(
            recovered.has_value());
        FPS_EXPECT_EQ(
            recovered->sequence,
            std::uint64_t{5});
        FPS_EXPECT_TRUE(
            recovered->keyframe);
        FPS_EXPECT_EQ(
            queue.Stats().popped,
            std::uint64_t{1});
    }

    {
        BoundedEncodedQueue queue{2};
        queue.Push(MakeUnit(1, true));
        queue.Push(MakeUnit(2, false));

        FPS_EXPECT_EQ(
            queue.Push(
                MakeUnit(3, true)),
            EncodedQueuePushResult::
                OverflowResetKeyframeQueued);

        FPS_EXPECT_EQ(
            queue.Size(),
            std::size_t{1});
        FPS_EXPECT_TRUE(
            !queue.AwaitingKeyframe());

        const auto newest =
            queue.Pop();
        FPS_EXPECT_TRUE(
            newest.has_value());
        FPS_EXPECT_EQ(
            newest->sequence,
            std::uint64_t{3});
        FPS_EXPECT_TRUE(
            newest->keyframe);
    }
}
