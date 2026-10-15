#include "test_support.h"

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

int main() {
    FPS_EXPECT_TRUE(true);
    FPS_EXPECT_EQ(2 + 2, 4);
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
    return fps::test::FailureCount() == 0 ? 0 : 1;
}
