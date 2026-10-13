#pragma once

#include "../../app/AppState.h"
#include "../../camera/CameraPhase.h"

namespace rvf {

// Ricoh workflow state -> brand-neutral phase. BleReady counts as Connecting:
// the workflow passes through it on its way to Wi-Fi, and portrait rests in
// WifiCredentialsReady (Idle), where the UI shows the remote from the link flags.
inline CameraPhase ricohCameraPhase(AppState state) {
    switch (state) {
        case AppState::Booting:
            return CameraPhase::Booting;
        case AppState::BleScan:
        case AppState::ScanningCamera:
            return CameraPhase::Searching;
        case AppState::ConnectingBle:
        case AppState::CheckingCameraPower:
        case AppState::BleReady:
        case AppState::ActivatingWifi:
        case AppState::WifiConnecting:
        case AppState::ConnectingWifi:
        case AppState::HttpProbe:
        case AppState::HttpProbing:
        case AppState::PreviewStarting:
            return CameraPhase::Connecting;
        case AppState::LiveViewRunning:
        case AppState::PreviewRunning:
        case AppState::Shooting:
            return CameraPhase::Previewing;
        case AppState::PreviewStopped:
            return CameraPhase::Ready;
        case AppState::CameraSleepGuard:
        case AppState::CameraPowerOff:
            return CameraPhase::Sleeping;
        case AppState::Error:
            return CameraPhase::Error;
        case AppState::Idle:
        case AppState::WifiCredentialsReady:
        case AppState::Disconnected:
            return CameraPhase::Idle;
    }
    return CameraPhase::Idle;
}

}  // namespace rvf
