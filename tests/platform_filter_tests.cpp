#include "test_support.h"

#include "platform/windows/cpu_memory_probe.h"
#include "platform/windows/dxgi_adapter_probe.h"
#include "platform/windows/monitor_probe.h"
#include "platform/windows/os_probe.h"
#include "platform/windows/window_probe.h"
#include "media/capture_device_probe.h"
#include "media/codec_capability_probe.h"

#include <mfapi.h>
#include <objbase.h>

#include <string>

void RunPlatformFilterTests() {
    using namespace fps::platform::windows;

    {
        const OsVersionQuery raw{
            .success = true,
            .major = 10,
            .minor = 0,
            .build = 19045,
            .architecture = "x64"
        };
        const auto info = MakeOsInfo(raw);
        FPS_EXPECT_EQ(info.state, fps::diagnostics::ProbeState::Available);
        FPS_EXPECT_EQ(info.build, 19045u);
        FPS_EXPECT_EQ(info.architecture, std::string{"x64"});
    }

    {
        const OsVersionQuery raw{
            .success = false,
            .error_code = 5,
            .message = "version query failed"
        };
        const auto info = MakeOsInfo(raw);
        FPS_EXPECT_EQ(info.state, fps::diagnostics::ProbeState::Error);
        FPS_EXPECT_EQ(info.error_code, 5L);
        FPS_EXPECT_EQ(info.message, std::string{"version query failed"});
    }

    FPS_EXPECT_EQ(
        NormalizeCpuModel("  Intel(R)   Synthetic CPU  "),
        std::string{"Intel(R)   Synthetic CPU"});

    {
        const CpuMemoryRaw raw{
            .cpu_model = "Synthetic CPU",
            .architecture = "x64",
            .logical_processors = 8,
            .memory_query_success = false,
            .total_physical_memory_bytes = 0,
            .error_code = 87,
            .message = "memory query failed"
        };
        const auto info = MakeCpuMemoryInfo(raw);
        FPS_EXPECT_EQ(info.state, fps::diagnostics::ProbeState::Error);
        FPS_EXPECT_EQ(info.total_physical_memory_bytes, 0ULL);
        FPS_EXPECT_EQ(info.error_code, 87L);
    }

    {
        const auto os = ProbeOperatingSystem();
        FPS_EXPECT_TRUE(os.build > 0);
        FPS_EXPECT_TRUE(!os.architecture.empty());

        const auto cpu = ProbeCpuAndMemory();
        FPS_EXPECT_TRUE(cpu.logical_processors > 0);
        FPS_EXPECT_TRUE(cpu.total_physical_memory_bytes > 0);
        FPS_EXPECT_TRUE(!cpu.cpu_model.empty());
    }

    {
        const HWND self = reinterpret_cast<HWND>(0x100);
        FPS_EXPECT_TRUE(!ShouldIncludeWindow(WindowCandidate{
            .handle = reinterpret_cast<HWND>(0x101),
            .visible = false,
            .title = "Invisible"
        }, self));

        FPS_EXPECT_TRUE(!ShouldIncludeWindow(WindowCandidate{
            .handle = reinterpret_cast<HWND>(0x102),
            .visible = true,
            .title = ""
        }, self));

        FPS_EXPECT_TRUE(!ShouldIncludeWindow(WindowCandidate{
            .handle = self,
            .visible = true,
            .title = "FriendPushScreen"
        }, self));

        FPS_EXPECT_TRUE(!ShouldIncludeWindow(WindowCandidate{
            .handle = reinterpret_cast<HWND>(0x103),
            .visible = true,
            .title = "Tool Window",
            .tool_window = true
        }, self));

        FPS_EXPECT_TRUE(!ShouldIncludeWindow(WindowCandidate{
            .handle = reinterpret_cast<HWND>(0x104),
            .owner = reinterpret_cast<HWND>(0x200),
            .visible = true,
            .title = "Owned Popup"
        }, self));

        FPS_EXPECT_TRUE(ShouldIncludeWindow(WindowCandidate{
            .handle = reinterpret_cast<HWND>(0x105),
            .visible = true,
            .title = "Application Window"
        }, self));
    }

    {
        const auto adapters = ProbeDxgiAdapters();
        FPS_EXPECT_TRUE(!adapters.empty());

        const auto monitors = ProbeMonitors();
        FPS_EXPECT_TRUE(!monitors.empty());

        const auto windows = ProbeVisibleTopLevelWindows(nullptr);
        FPS_EXPECT_TRUE(windows.size() >= 0);
    }

    {
        using fps::diagnostics::CodecDirection;
        using fps::diagnostics::CodecKind;
        using fps::diagnostics::HardwareClassification;
        using fps::media::NormalizeCodecCapability;

        const auto h264_encoder = NormalizeCodecCapability(
            CodecKind::H264,
            CodecDirection::Encode,
            "Synthetic H264 Encoder",
            true,
            false);
        FPS_EXPECT_EQ(h264_encoder.codec, CodecKind::H264);
        FPS_EXPECT_EQ(h264_encoder.direction, CodecDirection::Encode);
        FPS_EXPECT_EQ(h264_encoder.hardware, HardwareClassification::Hardware);

        const auto hevc_decoder = NormalizeCodecCapability(
            CodecKind::HEVC,
            CodecDirection::Decode,
            "Synthetic HEVC Decoder",
            false,
            true);
        FPS_EXPECT_EQ(hevc_decoder.codec, CodecKind::HEVC);
        FPS_EXPECT_EQ(hevc_decoder.direction, CodecDirection::Decode);
        FPS_EXPECT_EQ(hevc_decoder.hardware, HardwareClassification::Software);

        const auto av1_unknown = NormalizeCodecCapability(
            CodecKind::AV1,
            CodecDirection::Decode,
            "Synthetic AV1 Decoder",
            false,
            false);
        FPS_EXPECT_EQ(av1_unknown.codec, CodecKind::AV1);
        FPS_EXPECT_EQ(av1_unknown.direction, CodecDirection::Decode);
        FPS_EXPECT_EQ(av1_unknown.hardware, HardwareClassification::Unknown);

        const auto empty_result = fps::media::MakeMediaEnumerationResult(
            "capture_devices",
            0,
            0);
        FPS_EXPECT_EQ(empty_result.state, fps::diagnostics::ProbeState::Unavailable);

        const auto error_result = fps::media::MakeMediaEnumerationResult(
            "capture_devices",
            static_cast<long>(0x80004005L),
            0);
        FPS_EXPECT_EQ(error_result.state, fps::diagnostics::ProbeState::Error);
        FPS_EXPECT_EQ(error_result.error_code, static_cast<long>(0x80004005L));
    }

    {
        const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool should_uninitialize = SUCCEEDED(co_hr);

        const HRESULT mf_hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
        FPS_EXPECT_TRUE(SUCCEEDED(mf_hr));
        if (SUCCEEDED(mf_hr)) {
            const auto devices = fps::media::ProbeVideoCaptureDevices();
            const auto codecs = fps::media::ProbeCodecCapabilities();
            FPS_EXPECT_TRUE(devices.size() >= 0);
            FPS_EXPECT_TRUE(codecs.size() >= 0);
            MFShutdown();
        }

        if (should_uninitialize) {
            CoUninitialize();
        }
    }
}
