#include "test_support.h"

void RunFeedbackProtectionTests();
void RunPeerSessionTests();
void RunPeerProtocolTests();
void RunPeerIdentityTests();
void RunDiagnosticsFormatterTests();
void RunPlatformFilterTests();
void RunDiagnosticsServiceTests();
void RunUiStateTests();
void RunSettingsStoreTests();
void RunCaptureSourceCatalogTests();
void RunMainWindowLayoutTests();
void RunPreviewRendererTests();
void RunRuntimePathsTests();
void RunVideoPipelineModelTests();
void RunVideoLoopMetricsTests();
void RunMftVideoMediaTypeTests();
void RunVideoLocalLoopTests();

int main() {
    FPS_EXPECT_TRUE(true);
    FPS_EXPECT_EQ(2 + 2, 4);
    RunPeerIdentityTests();
    RunPeerProtocolTests();
    RunPeerSessionTests();
    RunFeedbackProtectionTests();
    RunDiagnosticsFormatterTests();
    RunPlatformFilterTests();
    RunDiagnosticsServiceTests();
    RunUiStateTests();
    RunSettingsStoreTests();
    RunCaptureSourceCatalogTests();
    RunMainWindowLayoutTests();
    RunPreviewRendererTests();
    RunRuntimePathsTests();
    RunVideoPipelineModelTests();
    RunVideoLoopMetricsTests();
    RunMftVideoMediaTypeTests();
    RunVideoLocalLoopTests();
    return fps::test::FailureCount() == 0 ? 0 : 1;
}
