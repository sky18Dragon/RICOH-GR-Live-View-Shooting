#pragma once

#include "../app/AppState.h"

namespace rvf {

struct UiSnapshot {
    AppState appState = AppState::Booting;
    bool bleConnected = false;
    bool wifiConnected = false;
    bool previewRunning = false;
    bool shutterReady = false;
    bool cameraSleepLike = false;
    bool resettingPairing = false;
    bool pairingGuideActive = false;
    bool pairingGuideGr2Selected = false;
    bool pairingGuideGr4Selected = false;
    bool wifiProvisioningActive = false;
    bool wifiProvisioningPreparing = false;
    bool hasFrame = false;

    float fps = 0.0f;
    int32_t rssi = 0;
    uint32_t decodedFrames = 0;
    uint32_t droppedFrames = 0;
    int8_t deviceBatteryPercent = -1;
    bool deviceCharging = false;

    const char* cameraModel = nullptr;
    const char* provisioningSsid = nullptr;
    const char* provisioningPassword = nullptr;
    const char* provisioningUrl = nullptr;
    const char* errorTitle = nullptr;
    const char* errorDetail = nullptr;
};

}  // namespace rvf
