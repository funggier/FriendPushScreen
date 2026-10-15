#include "graphics/preview_renderer.h"

#include "test_support.h"

#include <cmath>

void RunPreviewRendererTests() {
    using fps::graphics::CalculateAspectFit;

    {
        const auto fit =
            CalculateAspectFit(
                1920,
                1080,
                1920,
                1080);

        FPS_EXPECT_TRUE(
            std::abs(fit.x) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(fit.y) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.width -
                1920.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.height -
                1080.0F) < 0.01F);
    }

    {
        const auto fit =
            CalculateAspectFit(
                1264,
                691,
                2560,
                1440);

        FPS_EXPECT_TRUE(
            std::abs(
                fit.height -
                691.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.width -
                1228.4445F) < 0.1F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.x -
                17.7778F) < 0.1F);
        FPS_EXPECT_TRUE(
            std::abs(fit.y) < 0.01F);
    }

    {
        const auto fit =
            CalculateAspectFit(
                1280,
                720,
                1080,
                1920);

        FPS_EXPECT_TRUE(
            std::abs(
                fit.width -
                405.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.height -
                720.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.x -
                437.5F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(fit.y) < 0.01F);
    }

    {
        const auto fit =
            CalculateAspectFit(
                0,
                720,
                1920,
                1080);

        FPS_EXPECT_TRUE(
            fit.width == 0.0F &&
            fit.height == 0.0F &&
            fit.scale == 0.0);
    }
}
