#include "media/mft_video_media_type.h"
#include "media/mft_video_transform_activation.h"

#include "test_support.h"

#include <cstdint>
#include <wrl/client.h>

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

    const HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    FPS_EXPECT_TRUE(SUCCEEDED(MFStartup(MF_VERSION)));
    Microsoft::WRL::ComPtr<IMFMediaType> type;
    FPS_EXPECT_TRUE(SUCCEEDED(MFCreateMediaType(&type)));
    MFSetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,1376,768);
    MFVideoArea area{};area.Area.cx=1366;area.Area.cy=768;
    type->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,reinterpret_cast<const UINT8*>(&area),sizeof(area));
    fps::media::DecodedNv12Layout layout;
    FPS_EXPECT_TRUE(SUCCEEDED(fps::media::ResolveDecodedNv12Layout(type.Get(),1366,768,layout)));
    FPS_EXPECT_EQ(layout.coded_width,1376U);
    FPS_EXPECT_EQ(layout.stride,1376U);
    type->SetUINT32(MF_MT_DEFAULT_STRIDE,1408);
    FPS_EXPECT_TRUE(SUCCEEDED(fps::media::ResolveDecodedNv12Layout(type.Get(),1366,768,layout)));
    FPS_EXPECT_EQ(layout.stride,1408U);
    type->SetUINT32(MF_MT_DEFAULT_STRIDE,1360);
    FPS_EXPECT_TRUE(FAILED(fps::media::ResolveDecodedNv12Layout(type.Get(),1366,768,layout)));
    type->SetUINT32(MF_MT_DEFAULT_STRIDE,0xFFFFFFFE);
    FPS_EXPECT_TRUE(FAILED(fps::media::ResolveDecodedNv12Layout(type.Get(),1366,768,layout)));
    layout={8,4,6,2,12};
    std::vector<std::uint8_t> input(72,0),compact;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<8;++col)
        input[row*12+col]=static_cast<std::uint8_t>(row*16+col);
    for(unsigned row=0;row<2;++row)for(unsigned col=0;col<8;++col)
        input[48+row*12+col]=static_cast<std::uint8_t>(150+row*8+col);
    FPS_EXPECT_TRUE(SUCCEEDED(fps::media::CopyNv12VisibleRows(input,layout,compact)));
    FPS_EXPECT_EQ(compact.size(),18U);
    FPS_EXPECT_EQ(compact[11],21);
    FPS_EXPECT_EQ(compact[12],150);
    FPS_EXPECT_EQ(compact[17],155);
    input.resize(71);
    FPS_EXPECT_TRUE(FAILED(fps::media::CopyNv12VisibleRows(input,layout,compact)));
    FPS_EXPECT_TRUE(compact.empty());
    Microsoft::WRL::ComPtr<IMFAttributes> activation;
    MFCreateAttributes(&activation,4);
    const GUID codec_id{0x12345678,0,0,{0,0,0,0,0,0,0,1}};
    activation->SetGUID(MFT_TRANSFORM_CLSID_Attribute,codec_id);
    activation->SetString(MFT_FRIENDLY_NAME_Attribute,L"Same codec name");
    activation->SetString(MFT_ENUM_HARDWARE_URL_Attribute,L"opaque-device-a");
    fps::media::MftVideoCodecDescriptor descriptor;
    descriptor.clsid=codec_id;descriptor.candidate.name="Same codec name";
    descriptor.candidate.classification=fps::media::VideoBackendClassification::Hardware;
    descriptor.hardware_url=L"opaque-device-b";
    FPS_EXPECT_TRUE(!fps::media::MatchesMftVideoCodecDescriptor(activation.Get(),descriptor));
    activation->SetString(MFT_ENUM_HARDWARE_URL_Attribute,L"opaque-device-b");
    FPS_EXPECT_TRUE(fps::media::MatchesMftVideoCodecDescriptor(activation.Get(),descriptor));
    MFShutdown();if(SUCCEEDED(co))CoUninitialize();
}
