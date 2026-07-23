#pragma once

namespace rvf {

enum class UserCommand {
    None,
    StartPreview,
    StopPreview,
    Shoot,
    ToggleDisplayRotation,
    ToggleDisplayMirror,
    ResetPairing,
    ManualWake,
    PowerOff,
    LockScreen,
    OpenSettings,
    Back,
    Confirm,
    Up,
    Down,
};

inline const char* userCommandName(UserCommand command) {
    switch (command) {
        case UserCommand::None: return "None";
        case UserCommand::StartPreview: return "StartPreview";
        case UserCommand::StopPreview: return "StopPreview";
        case UserCommand::Shoot: return "Shoot";
        case UserCommand::ToggleDisplayRotation: return "ToggleDisplayRotation";
        case UserCommand::ToggleDisplayMirror: return "ToggleDisplayMirror";
        case UserCommand::ResetPairing: return "ResetPairing";
        case UserCommand::ManualWake: return "ManualWake";
        case UserCommand::PowerOff: return "PowerOff";
        case UserCommand::LockScreen: return "LockScreen";
        case UserCommand::OpenSettings: return "OpenSettings";
        case UserCommand::Back: return "Back";
        case UserCommand::Confirm: return "Confirm";
        case UserCommand::Up: return "Up";
        case UserCommand::Down: return "Down";
    }
    return "Unknown";
}

}  // namespace rvf
