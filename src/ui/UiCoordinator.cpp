#include "UiCoordinator.h"

#include <algorithm>

#include "UiTheme.h"

namespace rvf {

void UiCoordinator::begin(uint32_t nowMs) {
    _view = UiViewModel{};
    _lastAppState = AppState::Booting;
    _lastScene = UiScene::Boot;
    _stateChangedAtMs = nowMs;
    _sceneChangedAtMs = nowMs;
    _focusTickPlayed = false;
    _lastFocusTickAtMs = nowMs;
    _shutterOverlay.stop();
    _resetSplit.stop();
    _wakePulse.stop();
    _pendingSound = UiSound::None;
}

bool UiCoordinator::isConnectionState(AppState state) {
    switch (state) {
        case AppState::BleScan:
        case AppState::ScanningCamera:
        case AppState::ConnectingBle:
        case AppState::CheckingCameraPower:
        case AppState::BleReady:
        case AppState::ActivatingWifi:
        case AppState::WifiConnecting:
        case AppState::ConnectingWifi:
        case AppState::HttpProbe:
        case AppState::HttpProbing:
        case AppState::PreviewStarting:
            return true;
        default:
            return false;
    }
}

UiScene UiCoordinator::selectScene(const UiSnapshot& snapshot,
                                   UiOrientation orientation,
                                   bool resetVisualActive) {
    if (snapshot.appState == AppState::Error) return UiScene::Error;
    if (snapshot.resettingPairing || resetVisualActive) return UiScene::ResetPairing;
    if (snapshot.cameraSleepLike || snapshot.appState == AppState::CameraSleepGuard ||
        snapshot.appState == AppState::CameraPowerOff) {
        return UiScene::CameraSleep;
    }
    if (snapshot.appState == AppState::Booting) return UiScene::Boot;
    if (isConnectionState(snapshot.appState)) {
        return (snapshot.appState == AppState::BleScan || snapshot.appState == AppState::ScanningCamera)
                   ? UiScene::Pairing
                   : UiScene::Connecting;
    }
    if (snapshot.previewRunning || snapshot.appState == AppState::LiveViewRunning ||
        snapshot.appState == AppState::PreviewRunning || snapshot.appState == AppState::Shooting) {
        return orientation == UiOrientation::Landscape ? UiScene::LivePreview : UiScene::RemoteReady;
    }
    if (snapshot.bleConnected || snapshot.shutterReady || snapshot.appState == AppState::BleReady ||
        snapshot.appState == AppState::PreviewStopped) {
        return UiScene::RemoteReady;
    }
    return UiScene::Disconnected;
}

float UiCoordinator::connectionPhaseFloor(AppState state) {
    switch (state) {
        case AppState::BleScan: return 0.0f;
        case AppState::ScanningCamera: return 0.10f;
        case AppState::ConnectingBle: return 0.28f;
        case AppState::CheckingCameraPower: return 0.42f;
        case AppState::BleReady: return 0.52f;
        case AppState::ActivatingWifi: return 0.62f;
        case AppState::WifiConnecting:
        case AppState::ConnectingWifi: return 0.72f;
        case AppState::HttpProbe:
        case AppState::HttpProbing: return 0.84f;
        case AppState::PreviewStarting: return 0.94f;
        default: return 1.0f;
    }
}

UiOrientation UiCoordinator::desiredOrientation(UiScene scene, UiOrientation sensed) {
    if (scene == UiScene::LivePreview) return UiOrientation::Landscape;
    if (scene == UiScene::ResetPairing) return sensed;
    return UiOrientation::Portrait;
}

void UiCoordinator::requestSound(UiSound sound) {
    if (_pendingSound == UiSound::None) _pendingSound = sound;
}

UiSound UiCoordinator::consumeSound() {
    const UiSound sound = _pendingSound;
    _pendingSound = UiSound::None;
    return sound;
}

void UiCoordinator::notifyShutterStarted(uint32_t) {
    // The actual camera call remains synchronous and unchanged. This notification
    // allows the renderer to show the prepared focus state before that call.
}

void UiCoordinator::notifyShutterResult(bool success, uint32_t nowMs) {
    const uint32_t duration = _view.orientation == UiOrientation::Landscape
                                  ? UiTheme::kLandscapeShutterCurtainMs
                                  : UiTheme::kPortraitShutterFlashMs;
    _shutterOverlay.start(nowMs, duration);
    _view.shutterFailed = !success;
    requestSound(success ? UiSound::ShutterClick : UiSound::Error);
}

void UiCoordinator::notifyResetTriggered(uint32_t nowMs) {
    _resetSplit.start(nowMs, UiTheme::kResetSplitMs);
}

void UiCoordinator::notifyManualWake(uint32_t nowMs) {
    _wakePulse.start(nowMs, UiTheme::kWakeBrightenDurationMs);
    requestSound(UiSound::Success);
}

void UiCoordinator::update(const UiSnapshot& snapshot,
                           const ButtonEvents& input,
                           UiOrientation sensedOrientation,
                           uint32_t nowMs) {
    if (snapshot.appState != _lastAppState) {
        _lastAppState = snapshot.appState;
        _stateChangedAtMs = nowMs;
    }

    if (input.buttonADown) {
        _focusTickPlayed = false;
        _lastFocusTickAtMs = nowMs;
    }
    _view.focusActive = input.buttonAHeld;
    if (input.buttonAHeld && input.buttonAHoldMs >= UiTheme::kFocusHoldThresholdMs) {
        const uint32_t focusElapsed = input.buttonAHoldMs - UiTheme::kFocusHoldThresholdMs;
        _view.focusProgress = uiSmoothStep(
            static_cast<float>(focusElapsed) / static_cast<float>(UiTheme::kFocusTransitionMs));
        if (!_focusTickPlayed || uiElapsedMs(nowMs, _lastFocusTickAtMs) >= UiTheme::kFocusTickIntervalMs) {
            requestSound(UiSound::FocusTick);
            _focusTickPlayed = true;
            _lastFocusTickAtMs = nowMs;
        }
    } else {
        _view.focusProgress = 0.0f;
    }
    if (input.buttonAReleased) {
        _view.focusActive = false;
        _view.focusProgress = 0.0f;
    }

    if (input.resetPairing) notifyResetTriggered(nowMs);

    _shutterOverlay.update(nowMs);
    _resetSplit.update(nowMs);
    _wakePulse.update(nowMs);

    const bool resetVisualActive = input.resetHoldActive || _resetSplit.active;
    const UiScene scene = selectScene(snapshot, sensedOrientation, resetVisualActive);
    if (scene != _lastScene) {
        _lastScene = scene;
        _sceneChangedAtMs = nowMs;
    }

    _view.scene = scene;
    _view.orientation = desiredOrientation(scene, sensedOrientation);
    _view.nowMs = nowMs;
    _view.resetProgress = input.resetHoldActive ? input.resetHoldProgress : 0.0f;
    _view.resetSplitActive = _resetSplit.active;
    _view.shutterOverlayActive = _shutterOverlay.active;
    _view.overlayProgress = _shutterOverlay.progress(nowMs);
    _view.hasFrame = snapshot.hasFrame;
    _view.deviceBatteryPercent = snapshot.deviceBatteryPercent;
    _view.cameraBattery = snapshot.cameraBattery;
    _view.errorTitle = snapshot.errorTitle;
    _view.errorDetail = snapshot.errorDetail;
    _view.shouldRenderLivePreview = scene == UiScene::LivePreview && snapshot.previewRunning;

    const uint32_t sceneElapsed = uiElapsedMs(nowMs, _sceneChangedAtMs);
    if (scene == UiScene::Pairing || scene == UiScene::Connecting) {
        const float timeProgress = uiClamp01(static_cast<float>(sceneElapsed) /
                                             static_cast<float>(UiTheme::kConnectMergeMs));
        _view.sceneProgress = std::max(connectionPhaseFloor(snapshot.appState), timeProgress);
    } else if (scene == UiScene::Disconnected) {
        _view.sceneProgress = uiEaseOut(static_cast<float>(sceneElapsed) /
                                        static_cast<float>(UiTheme::kResetSplitMs));
    } else if (scene == UiScene::Boot) {
        const uint32_t cycle = sceneElapsed % 1000U;
        const float triangular = cycle <= 500U ? static_cast<float>(cycle) / 500.0f
                                               : static_cast<float>(1000U - cycle) / 500.0f;
        _view.sceneProgress = uiSmoothStep(triangular);
    } else {
        _view.sceneProgress = 1.0f;
    }

    if (scene == UiScene::CameraSleep) {
        _view.sleepProgress = uiSmoothStep(static_cast<float>(sceneElapsed) /
                                           static_cast<float>(UiTheme::kSleepDimDurationMs));
    } else {
        _view.sleepProgress = 0.0f;
    }
}

}  // namespace rvf
