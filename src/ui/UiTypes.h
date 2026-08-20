#pragma once

#include <cstdint>

namespace rvf {

enum class UiOrientation : uint8_t {
    Portrait,
    Landscape,
};

enum class UiScene : uint8_t {
    Boot,
    Pairing,
    Connecting,
    RemoteReady,
    LivePreview,
    CameraSleep,
    ResetPairing,
    Disconnected,
    Error,
};

enum class UiSound : uint8_t {
    None,
    FocusTick,
    ShutterClick,
    Success,
    Error,
};

inline const char* uiSceneName(UiScene scene) {
    switch (scene) {
        case UiScene::Boot: return "Boot";
        case UiScene::Pairing: return "Pairing";
        case UiScene::Connecting: return "Connecting";
        case UiScene::RemoteReady: return "RemoteReady";
        case UiScene::LivePreview: return "LivePreview";
        case UiScene::CameraSleep: return "CameraSleep";
        case UiScene::ResetPairing: return "ResetPairing";
        case UiScene::Disconnected: return "Disconnected";
        case UiScene::Error: return "Error";
    }
    return "Unknown";
}

}  // namespace rvf
