#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "app/application.h"

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show_command) {
    fps::app::Application application;
    return application.Run(instance, show_command);
}
