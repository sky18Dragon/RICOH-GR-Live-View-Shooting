#pragma once

#include "flow/RicohFlowState.h"
#include "../../camera/CameraPhase.h"

namespace rvf {

// Ricoh workflow state -> brand-neutral phase. BleReady counts as Connecting:
// the workflow passes through it on its way to Wi-Fi, and portrait rests in
// WifiCredentialsReady (Idle), where the UI shows the remote from the link flags.
inline CameraPhase ricohCameraPhase(RicohFlowState state) {
    switch (state) {
        case RicohFlowState::Booting:
            return CameraPhase::Booting;
        case RicohFlowState::BleScan:
        case RicohFlowState::ScanningCamera:
            return CameraPhase::Searching;
        case RicohFlowState::ConnectingBle:
        case RicohFlowState::CheckingCameraPower:
        case RicohFlowState::BleReady:
        case RicohFlowState::ActivatingWifi:
        case RicohFlowState::WifiConnecting:
        case RicohFlowState::ConnectingWifi:
        case RicohFlowState::HttpProbe:
        case RicohFlowState::HttpProbing:
        case RicohFlowState::PreviewStarting:
            return CameraPhase::Connecting;
        case RicohFlowState::LiveViewRunning:
        case RicohFlowState::PreviewRunning:
        case RicohFlowState::Shooting:
            return CameraPhase::Previewing;
        case RicohFlowState::PreviewStopped:
            return CameraPhase::Ready;
        case RicohFlowState::CameraSleepGuard:
        case RicohFlowState::CameraPowerOff:
            return CameraPhase::Sleeping;
        case RicohFlowState::Error:
            return CameraPhase::Error;
        case RicohFlowState::Idle:
        case RicohFlowState::WifiCredentialsReady:
        case RicohFlowState::Disconnected:
            return CameraPhase::Idle;
    }
    return CameraPhase::Idle;
}

}  // namespace rvf
