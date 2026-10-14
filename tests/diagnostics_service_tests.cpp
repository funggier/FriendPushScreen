#include "test_support.h"

#include "diagnostics/diagnostics_formatter.h"
#include "diagnostics/diagnostics_service.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <stdexcept>
#include <string>

void RunDiagnosticsServiceTests() {
    using namespace std::chrono_literals;
    using namespace fps::diagnostics;

    {
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();
        std::atomic<int> os_calls{0};

        DiagnosticsProbeBundle bundle{};
        bundle.operating_system = [&]() {
            ++os_calls;
            entered_promise.set_value();
            release.wait();
            OsInfo info{};
            info.state = ProbeState::Available;
            info.build = 19045;
            info.architecture = "x64";
            return info;
        };

        DiagnosticsService service{std::move(bundle)};
        FPS_EXPECT_TRUE(service.StartCollection(nullptr));
        FPS_EXPECT_TRUE(entered.wait_for(2s) == std::future_status::ready);
        FPS_EXPECT_TRUE(!service.StartCollection(nullptr));
        FPS_EXPECT_EQ(os_calls.load(), 1);

        release_promise.set_value();
        FPS_EXPECT_TRUE(service.WaitForIdle(2s));
        FPS_EXPECT_TRUE(service.Latest() != nullptr);
    }

    {
        DiagnosticsProbeBundle bundle{};
        bundle.operating_system = []() {
            OsInfo info{};
            info.state = ProbeState::Available;
            info.build = 19045;
            info.architecture = "x64";
            return info;
        };
        bundle.graphics_adapters = []() {
            return std::vector<GraphicsAdapterInfo>{
                GraphicsAdapterInfo{.description = "Synthetic GPU"}};
        };
        bundle.capture_devices = []() -> std::vector<CaptureDeviceInfo> {
            throw std::runtime_error{"capture failed"};
        };
        bundle.network = []() {
            NetworkInfo info{};
            info.result = ProbeResult{
                "network",
                ProbeState::Available,
                0,
                {}
            };
            info.adapters.push_back(NetworkAdapterInfo{
                .name = "Ethernet",
                .operational = true
            });
            info.tailscale = TailscalePresence::Absent;
            return info;
        };

        DiagnosticsService service{std::move(bundle)};
        FPS_EXPECT_TRUE(service.StartCollection(nullptr));
        FPS_EXPECT_TRUE(service.WaitForIdle(2s));

        const auto snapshot = service.Latest();
        FPS_EXPECT_TRUE(snapshot != nullptr);
        if (snapshot != nullptr) {
            FPS_EXPECT_EQ(snapshot->os.build, 19045u);
            FPS_EXPECT_EQ(snapshot->graphics_adapters.size(), std::size_t{1});
            FPS_EXPECT_EQ(snapshot->network.adapters.size(), std::size_t{1});

            bool capture_error_found = false;
            for (const auto& result : snapshot->probe_results) {
                if (result.name == "capture_devices" &&
                    result.state == ProbeState::Error) {
                    capture_error_found = true;
                }
            }
            FPS_EXPECT_TRUE(capture_error_found);
        }
    }

    {
        DiagnosticsSnapshot snapshot{};
        snapshot.app_version = "0.1.0";
        snapshot.visible_windows.push_back(
            WindowInfo{.title = "Secret Project - fake-token-12345"});

        const auto path =
            std::filesystem::temp_directory_path() /
            "friendpushscreen-diagnostics-export-test.txt";
        std::error_code remove_error;
        std::filesystem::remove(path, remove_error);

        FPS_EXPECT_TRUE(SaveDiagnosticsUtf8(snapshot, path));

        std::ifstream input{path, std::ios::binary};
        const std::string text{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};

        FPS_EXPECT_TRUE(!text.empty());
        FPS_EXPECT_TRUE(
            text.find("FriendPushScreen Diagnostics") != std::string::npos);
        FPS_EXPECT_TRUE(
            text.find("Secret Project - fake-token-12345") == std::string::npos);
        FPS_EXPECT_TRUE(
            text.find("Visible Windows") != std::string::npos);

        std::filesystem::remove(path, remove_error);
    }
}
