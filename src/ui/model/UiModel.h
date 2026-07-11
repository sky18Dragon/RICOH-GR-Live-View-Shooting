#pragma once

#include <cstdint>

#include "../../app/AppState.h"
#include "UiPhase.h"
#include "UiScreen.h"

namespace rvf::ui {

enum class UiShutterStatus : uint8_t {
    Idle,
    Shooting,
    Succeeded,
    Failed,
};

struct UiRuntimeSnapshot {
    AppState appState = AppState::Booting;

    bool bleConnected = false;
    bool wifiConnected = false;
    bool previewRunning = false;
    bool cameraStandby = false;
    bool shutterReady = false;
    bool recovering = false;
    bool pairingReset = false;
    bool shutdownRequested = false;
    bool errorActive = false;
    UiShutterStatus shutterStatus = UiShutterStatus::Idle;

    int32_t wifiRssi = 0;
    float fps = 0.0f;
    uint32_t renderedFrames = 0;
    uint32_t droppedFrames = 0;

    // Text values are borrowed for the duration of present()/render(). The
    // presenter never parses these values to infer application state.
    const char* cameraName = "";
    const char* cameraModel = "";
    const char* battery = "";
    const char* localIp = "";
    const char* detail = "";

    int errorCode = 0;
    const char* errorMessage = "";
};

struct UiModel {
    UiScreen screen = UiScreen::Boot;
    UiPhase phase = UiPhase::Booting;
    AppState appState = AppState::Booting;

    bool bleConnected = false;
    bool wifiConnected = false;
    bool previewRunning = false;
    bool cameraStandby = false;
    bool shutterReady = false;
    UiShutterStatus shutterStatus = UiShutterStatus::Idle;

    int32_t wifiRssi = 0;
    float fps = 0.0f;
    uint32_t renderedFrames = 0;
    uint32_t droppedFrames = 0;

    const char* cameraName = "";
    const char* cameraModel = "";
    const char* battery = "";
    const char* localIp = "";
    const char* detail = "";

    int errorCode = 0;
    const char* errorMessage = "";
};

}  // namespace rvf::ui
