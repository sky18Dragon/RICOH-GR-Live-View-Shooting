#pragma once

#include "../camera/CameraPhase.h"

namespace rvf {

struct UiSnapshot {
    CameraPhase cameraPhase = CameraPhase::Booting;
    bool bleConnected = false;
    bool wifiConnected = false;
    bool previewRunning = false;
    bool shutterReady = false;
    bool cameraSleepLike = false;
    bool resettingPairing = false;
    bool pairingGuideActive = false;
    // CameraRegistry index of the highlighted model while the guide is shown.
    uint8_t pairingGuideSelectedIndex = 0;
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
