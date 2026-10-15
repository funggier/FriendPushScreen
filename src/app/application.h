#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "diagnostics/diagnostics_service.h"
#include "graphics/placeholder_renderer.h"
#include "ui/main_window.h"
#include "ui/ui_command.h"
#include "ui/ui_state.h"

#include <string>

namespace fps::app {

class Application {
public:
    int Run(
        HINSTANCE instance,
        int show_command);

private:
    void RefreshDiagnostics();
    void OnDiagnosticsReady();
    void SaveDiagnostics();

    void HandleUiCommand(
        const ui::UiCommand& command);

    void PersistUiPreferences();

    void ApplyUiState();

    ui::MainWindow main_window_;
    graphics::PlaceholderRenderer renderer_;
    diagnostics::DiagnosticsService
        diagnostics_service_;

    ui::UiState ui_state_{};

    bool renderer_initialized_{false};
    std::wstring renderer_status_;
};

}  // namespace fps::app
