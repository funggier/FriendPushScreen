#include "media/mft_video_media_type.h"

#include "test_support.h"

#include <cstdint>

void RunMftVideoMediaTypeTests() {
    fps::media::VideoCodecConfig config{};
    config.codec =
        fps::media::VideoCodecKind::H264;
    config.width = 1920;
    config.height = 1080;
    config.fps_numerator = 60;
    config.fps_denominator = 1;
    config.target_bitrate_bps =
        20'000'000;

    const auto encoder_input =
        fps::media::
            MakeEncoderInputTypeSpec(config);
    const auto encoder_output =
        fps::media::
            MakeEncoderOutputTypeSpec(config);
    const auto decoder_input =
        fps::media::
            MakeDecoderInputTypeSpec(config);
    const auto decoder_output =
        fps::media::
            MakeDecoderOutputTypeSpec(config);

    FPS_EXPECT_EQ(
        encoder_input.role,
        fps::media::
            MftVideoMediaTypeRole::RawNv12);
    FPS_EXPECT_EQ(
        encoder_output.role,
        fps::media::
            MftVideoMediaTypeRole::Encoded);
    FPS_EXPECT_EQ(
        decoder_input.role,
        fps::media::
            MftVideoMediaTypeRole::Encoded);
    FPS_EXPECT_EQ(
        decoder_output.role,
        fps::media::
            MftVideoMediaTypeRole::RawNv12);

    FPS_EXPECT_EQ(
        encoder_input.width,
        std::uint32_t{1920});
    FPS_EXPECT_EQ(
        encoder_input.height,
        std::uint32_t{1080});
    FPS_EXPECT_EQ(
        encoder_output.bitrate_bps,
        std::uint32_t{20'000'000});
    FPS_EXPECT_EQ(
        decoder_input.fps_numerator,
        std::uint32_t{60});
}
