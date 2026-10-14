#include "test_support.h"

void RunDiagnosticsFormatterTests();
void RunPlatformFilterTests();
void RunDiagnosticsServiceTests();

int main() {
    FPS_EXPECT_TRUE(true);
    FPS_EXPECT_EQ(2 + 2, 4);
    RunDiagnosticsFormatterTests();
    RunPlatformFilterTests();
    RunDiagnosticsServiceTests();
    return fps::test::FailureCount() == 0 ? 0 : 1;
}
