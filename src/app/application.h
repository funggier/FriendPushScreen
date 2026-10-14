#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "graphics/placeholder_renderer.h"
#include "ui/main_window.h"

namespace fps::app {

class Application {
public:
    int Run(HINSTANCE instance, int show_command);

private:
    ui::MainWindow main_window_;
    graphics::PlaceholderRenderer renderer_;
    bool renderer_initialized_{false};
};

}  // namespace fps::app
