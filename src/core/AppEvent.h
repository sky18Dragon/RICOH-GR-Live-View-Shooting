#pragma once

namespace rvf {

enum class AppEventType {
    None,
    BootCompleted,
    BleScanStarted,
    BleDeviceFound,
    BleConnected,
    BleDisconnected,
    CameraPowerOn,
    CameraPowerOff,
    CameraPowerUnknown,
    WifiActivationRequested,
    WifiConnected,
    WifiDisconnected,
    HttpProbeSucceeded,
    HttpProbeFailed,
    PreviewStarted,
    PreviewStopped,
    PreviewTimeout,
    ButtonShortPress,
    ButtonLongPress,
    ShutterPressed,
    ErrorRaised,

    StateChanged,
    ShutterSucceeded,
    ShutterFailed,
    RecoveryStarted,
    RecoverySucceeded,
    RecoveryFailed,
    PairingResetStarted,
    PairingResetCompleted,
    ShutdownRequested,
};

}  // namespace rvf
