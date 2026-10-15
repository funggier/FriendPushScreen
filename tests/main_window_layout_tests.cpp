#include "ui/main_window.h"

#include "test_support.h"

void RunMainWindowLayoutTests() {
    using fps::ui::CalculateViewerViewport;
    using fps::ui::ShouldDeferUiCommand;
    using fps::ui::UiCommandType;

    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::SelectCaptureSource));
    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::ToggleCapturePreview));
    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::RefreshCaptureSources));
    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::ToggleShowCursor));
    FPS_EXPECT_TRUE(
        !ShouldDeferUiCommand(
            UiCommandType::ToggleSettings));

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
