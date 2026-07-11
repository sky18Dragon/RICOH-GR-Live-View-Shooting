#pragma once

#include <cstdint>

namespace rvf::ui {

enum class UiScreen : uint8_t {
    Boot,
    Status,
    LiveView,
    Error,
    Shutdown,
    Settings,
};

inline const char* uiScreenName(UiScreen screen) {
    switch (screen) {
        case UiScreen::Boot: return "BOOT";
        case UiScreen::Status: return "STATUS";
        case UiScreen::LiveView: return "LIVE VIEW";
        case UiScreen::Error: return "ERROR";
        case UiScreen::Shutdown: return "SHUTDOWN";
        case UiScreen::Settings: return "SETTINGS";
    }
    return "UNKNOWN";
}

}  // namespace rvf::ui
