#include "UiPresenter.h"

namespace rvf::ui {

namespace {

const char* safeText(const char* value) {
    return value != nullptr ? value : "";
}

}  // namespace

UiModel UiPresenter::present(const UiRuntimeSnapshot& snapshot) const {
    UiModel model;
    model.screen = mapScreen(snapshot);
    model.phase = mapPhase(snapshot);
    model.appState = snapshot.appState;
    model.bleConnected = snapshot.bleConnected;
    model.wifiConnected = snapshot.wifiConnected;
    model.previewRunning = snapshot.previewRunning;
    model.cameraStandby = snapshot.cameraStandby;
    model.shutterReady = snapshot.shutterReady;
    model.shutterStatus = snapshot.shutterStatus;
    model.wifiRssi = snapshot.wifiRssi;
    model.fps = snapshot.fps;
    model.renderedFrames = snapshot.renderedFrames;
    model.droppedFrames = snapshot.droppedFrames;
    model.cameraName = safeText(snapshot.cameraName);
    model.cameraModel = safeText(snapshot.cameraModel);
    model.battery = safeText(snapshot.battery);
    model.localIp = safeText(snapshot.localIp);
    model.detail = safeText(snapshot.detail);
    model.errorCode = snapshot.errorCode;
    model.errorMessage = safeText(snapshot.errorMessage);
    return model;
}

UiScreen UiPresenter::mapScreen(const UiRuntimeSnapshot& snapshot) {
    if (snapshot.shutdownRequested) {
        return UiScreen::Shutdown;
    }
    if (snapshot.appState == AppState::Error || snapshot.errorActive) {
        return UiScreen::Error;
    }
    if (snapshot.pairingReset || snapshot.recovering || snapshot.cameraStandby) {
        return UiScreen::Status;
    }
    if (snapshot.previewRunning ||
        snapshot.appState == AppState::LiveViewRunning ||
        snapshot.appState == AppState::PreviewRunning) {
        return UiScreen::LiveView;
    }
    if (snapshot.appState == AppState::Booting) {
        return UiScreen::Boot;
    }
    return UiScreen::Status;
}

UiPhase UiPresenter::mapPhase(const UiRuntimeSnapshot& snapshot) {
    if (snapshot.appState == AppState::Error || snapshot.errorActive) {
        return UiPhase::Error;
    }
    if (snapshot.pairingReset) {
        return UiPhase::PairingReset;
    }
    if (snapshot.recovering) {
        return UiPhase::Recovering;
    }
    if (snapshot.shutterStatus != UiShutterStatus::Idle) {
        return UiPhase::Shooting;
    }
    if (snapshot.cameraStandby) {
        return UiPhase::CameraStandby;
    }

    switch (snapshot.appState) {
        case AppState::Booting:
        case AppState::Idle:
            return UiPhase::Booting;
        case AppState::BleScan:
        case AppState::ScanningCamera:
            return UiPhase::BleScanning;
        case AppState::ConnectingBle:
            return UiPhase::BleConnecting;
        case AppState::BleReady:
            return UiPhase::BleConnected;
        case AppState::CheckingCameraPower:
            return UiPhase::CheckingCameraPower;
        case AppState::CameraSleepGuard:
        case AppState::CameraPowerOff:
            return UiPhase::CameraStandby;
        case AppState::ActivatingWifi:
            return UiPhase::ActivatingWifi;
        case AppState::WifiConnecting:
        case AppState::ConnectingWifi:
            return snapshot.wifiConnected ? UiPhase::WifiConnected : UiPhase::WifiConnecting;
        case AppState::HttpProbe:
        case AppState::HttpProbing:
            return UiPhase::HttpProbing;
        case AppState::PreviewStarting:
            return UiPhase::StartingPreview;
        case AppState::LiveViewRunning:
        case AppState::PreviewRunning:
            return UiPhase::PreviewRunning;
        case AppState::Shooting:
            return UiPhase::Shooting;
        case AppState::PreviewStopped:
        case AppState::Disconnected:
            return UiPhase::Recovering;
        case AppState::Error:
            return UiPhase::Error;
    }
    return UiPhase::Error;
}

}  // namespace rvf::ui
