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
    bool hasFrame = false;

    float fps = 0.0f;
    int32_t rssi = 0;
    uint32_t decodedFrames = 0;
    uint32_t droppedFrames = 0;
    int8_t deviceBatteryPercent = -1;

    const char* cameraModel = nullptr;
    const char* cameraBattery = nullptr;
    const char* errorTitle = nullptr;
    const char* errorDetail = nullptr;
};

}  // namespace rvf
