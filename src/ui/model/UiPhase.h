#pragma once

#include <cstdint>

namespace rvf::ui {

enum class UiPhase : uint8_t {
    Booting,
    BleScanning,
    BleConnecting,
    BleConnected,
    CheckingCameraPower,
    CameraStandby,
    ActivatingWifi,
    WifiConnecting,
    WifiConnected,
    HttpProbing,
    StartingPreview,
    PreviewRunning,
    Shooting,
    Recovering,
    PairingReset,
    Error,
};

inline const char* uiPhaseName(UiPhase phase) {
    switch (phase) {
        case UiPhase::Booting: return "BOOTING";
        case UiPhase::BleScanning: return "BLE SCANNING";
        case UiPhase::BleConnecting: return "BLE CONNECTING";
        case UiPhase::BleConnected: return "BLE CONNECTED";
        case UiPhase::CheckingCameraPower: return "CHECKING CAMERA";
        case UiPhase::CameraStandby: return "CAMERA STANDBY";
        case UiPhase::ActivatingWifi: return "ACTIVATING WIFI";
        case UiPhase::WifiConnecting: return "WIFI CONNECTING";
        case UiPhase::WifiConnected: return "WIFI CONNECTED";
        case UiPhase::HttpProbing: return "HTTP PROBING";
        case UiPhase::StartingPreview: return "STARTING PREVIEW";
        case UiPhase::PreviewRunning: return "PREVIEW RUNNING";
        case UiPhase::Shooting: return "SHOOTING";
        case UiPhase::Recovering: return "RECOVERING";
        case UiPhase::PairingReset: return "PAIRING RESET";
        case UiPhase::Error: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace rvf::ui
