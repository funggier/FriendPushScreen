#include "test_support.h"

#include "diagnostics/diagnostics_formatter.h"
#include "diagnostics/diagnostics_snapshot.h"
#include "diagnostics/logger.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

void RunDiagnosticsFormatterTests() {
    using namespace fps::diagnostics;

    FPS_EXPECT_EQ(ProbeStateToString(ProbeState::Available), std::string{"Available"});
    FPS_EXPECT_EQ(ProbeStateToString(ProbeState::Unavailable), std::string{"Unavailable"});
    FPS_EXPECT_EQ(ProbeStateToString(ProbeState::Unsupported), std::string{"Unsupported"});
    FPS_EXPECT_EQ(ProbeStateToString(ProbeState::Error), std::string{"Error"});

    DiagnosticsSnapshot snapshot{};
    snapshot.app_version = "0.1.0";
    snapshot.os.state = ProbeState::Available;
    snapshot.os.build = 19045;
    snapshot.cpu_memory.state = ProbeState::Available;
    snapshot.cpu_memory.cpu_model = "Synthetic CPU";
    snapshot.cpu_memory.logical_processors = 8;
    snapshot.cpu_memory.total_physical_memory_bytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;

    snapshot.graphics_adapters.push_back(GraphicsAdapterInfo{.description = "Synthetic GPU"});
    snapshot.capture_devices.push_back(CaptureDeviceInfo{.friendly_name = "Synthetic Capture"});
    snapshot.codec_capabilities.push_back(CodecCapabilityInfo{
        .codec = CodecKind::H264,
        .direction = CodecDirection::Encode,
        .hardware = HardwareClassification::Hardware,
        .name = "Synthetic H264 Encoder"
    });
    snapshot.network.adapters.push_back(NetworkAdapterInfo{.name = "Ethernet", .operational = true});
    snapshot.visible_windows.push_back(WindowInfo{.title = "Secret Project - Payroll"});
    snapshot.probe_results.push_back(ProbeResult{
        .name = "thai_probe",
        .state = ProbeState::Error,
        .error_code = 1234,
        .message = "ทดสอบภาษาไทย"
    });

    const std::string text = FormatDiagnosticsText(snapshot);

    FPS_EXPECT_TRUE(text.find("Application") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Operating System") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("CPU / Memory") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Graphics Adapters") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Capture Devices") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Codec Capabilities") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("H.264 Encode: Available") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("H.264 Decode: Unavailable") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("HEVC Encode: Unavailable") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("HEVC Decode: Unavailable") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("AV1 Encode: Unavailable") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("AV1 Decode: Unavailable") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Network") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Visible Windows") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("Secret Project - Payroll") == std::string::npos);
    FPS_EXPECT_TRUE(text.find("1234") != std::string::npos);
    FPS_EXPECT_TRUE(text.find("ทดสอบภาษาไทย") != std::string::npos);

    const std::string local_text = FormatDiagnosticsText(snapshot, true);
    FPS_EXPECT_TRUE(local_text.find("Secret Project - Payroll") != std::string::npos);

    const auto log_path = std::filesystem::temp_directory_path() / "friendpushscreen-logger-test.log";
    std::error_code remove_error;
    std::filesystem::remove(log_path, remove_error);

    Logger test_logger{log_path};
    test_logger.Log(LogLevel::Warning, "diagnostics-test", "logger smoke message");

    std::ifstream input{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};

    FPS_EXPECT_TRUE(log_text.find("Warning") != std::string::npos);
    FPS_EXPECT_TRUE(log_text.find("diagnostics-test") != std::string::npos);
    FPS_EXPECT_TRUE(log_text.find("logger smoke message") != std::string::npos);

    std::filesystem::remove(log_path, remove_error);
}
