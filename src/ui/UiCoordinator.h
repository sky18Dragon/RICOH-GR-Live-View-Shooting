#pragma once

#include <cstdint>

#include "ButtonInput.h"
#include "UiAnimator.h"
#include "UiSnapshot.h"
#include "UiTypes.h"

namespace rvf {

struct UiViewModel {
    UiScene scene = UiScene::Boot;
    UiOrientation orientation = UiOrientation::Portrait;
    uint32_t nowMs = 0;
    float sceneProgress = 0.0f;
    float focusProgress = 0.0f;
    float resetProgress = 0.0f;
    float overlayProgress = 0.0f;
    float sleepProgress = 0.0f;
    bool focusActive = false;
    bool shutterOverlayActive = false;
    bool shutterFailed = false;
    bool resetSplitActive = false;
    bool shouldRenderLivePreview = false;
    bool hasFrame = false;
    int8_t deviceBatteryPercent = -1;
    const char* cameraBattery = nullptr;
    const char* errorTitle = nullptr;
    const char* errorDetail = nullptr;
};

class UiCoordinator {
public:
    void begin(uint32_t nowMs);
    void update(const UiSnapshot& snapshot,
                const ButtonEvents& input,
                UiOrientation sensedOrientation,
                uint32_t nowMs);

    void notifyShutterStarted(uint32_t nowMs);
    void notifyShutterResult(bool success, uint32_t nowMs);
    void notifyResetTriggered(uint32_t nowMs);
    void notifyManualWake(uint32_t nowMs);

    const UiViewModel& viewModel() const { return _view; }
    bool shouldRenderLivePreview() const { return _view.shouldRenderLivePreview; }
    UiSound consumeSound();

    static UiScene selectScene(const UiSnapshot& snapshot,
                               UiOrientation orientation,
                               bool resetVisualActive = false);
    static float connectionPhaseFloor(AppState state);

private:
    static bool isConnectionState(AppState state);
    static UiOrientation desiredOrientation(UiScene scene, UiOrientation sensed);
    void requestSound(UiSound sound);

    UiViewModel _view;
    AppState _lastAppState = AppState::Booting;
    UiScene _lastScene = UiScene::Boot;
    uint32_t _stateChangedAtMs = 0;
    uint32_t _sceneChangedAtMs = 0;
    bool _focusTickPlayed = false;
    uint32_t _lastFocusTickAtMs = 0;
    AnimationState _shutterOverlay;
    AnimationState _resetSplit;
    AnimationState _wakePulse;
    UiSound _pendingSound = UiSound::None;
};

}  // namespace rvf
