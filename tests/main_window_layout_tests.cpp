#include "ui/main_window.h"

#include "test_support.h"

void RunMainWindowLayoutTests() {
    using fps::ui::CalculateViewerViewport;

    {
        const auto viewport =
            CalculateViewerViewport(
                1280,
                720,
                30);

        FPS_EXPECT_EQ(viewport.x, 0);
        FPS_EXPECT_EQ(viewport.y, 30);
        FPS_EXPECT_EQ(viewport.width, 1280);
        FPS_EXPECT_EQ(viewport.height, 690);
    }

    {
        const auto viewport =
            CalculateViewerViewport(
                1280,
                720,
                0);

        FPS_EXPECT_EQ(viewport.x, 0);
        FPS_EXPECT_EQ(viewport.y, 0);
        FPS_EXPECT_EQ(viewport.width, 1280);
        FPS_EXPECT_EQ(viewport.height, 720);
    }

    {
        const auto viewport =
            CalculateViewerViewport(
                640,
                20,
                30);

        FPS_EXPECT_EQ(viewport.x, 0);
        FPS_EXPECT_EQ(viewport.y, 20);
        FPS_EXPECT_EQ(viewport.width, 640);
        FPS_EXPECT_EQ(viewport.height, 0);
    }

    {
        const auto viewport =
            CalculateViewerViewport(
                -100,
                -50,
                30);

        FPS_EXPECT_EQ(viewport.width, 0);
        FPS_EXPECT_EQ(viewport.height, 0);
    }
}
