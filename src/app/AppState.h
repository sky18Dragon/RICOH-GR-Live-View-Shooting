#pragma once

namespace rvf {

enum class AppState {
    Booting,
    Idle,
    BleScan,
    CameraSleepGuard,
    BleReady,
    WifiConnecting,
    HttpProbe,
    LiveViewRunning,
    ScanningCamera,
    ConnectingBle,
    CheckingCameraPower,
    CameraPowerOff,
    ActivatingWifi,
    WifiCredentialsReady,
    ConnectingWifi,
    HttpProbing,
    PreviewStarting,
    PreviewRunning,
    PreviewStopped,
    Shooting,
    Disconnected,
    Error,
};

const char* appStateName(AppState state);

}  // namespace rvf
