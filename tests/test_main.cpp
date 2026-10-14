#include "test_support.h"

void RunDiagnosticsFormatterTests();

int main() {
    FPS_EXPECT_TRUE(true);
    FPS_EXPECT_EQ(2 + 2, 4);
    RunDiagnosticsFormatterTests();
    return fps::test::FailureCount() == 0 ? 0 : 1;
}
