#pragma once

#include <cstdint>

#include "UiTheme.h"
#include "UserCommand.h"

namespace rvf {

struct ButtonEvents {
    bool buttonA = false;  // Compatibility alias retained for release feedback.
    bool buttonADown = false;
    bool buttonAHeld = false;
    bool buttonAReleased = false;
    uint32_t buttonAHoldMs = 0;

    bool resetHoldActive = false;
    float resetHoldProgress = 0.0f;
    uint32_t resetHoldMs = 0;
    bool resetPairing = false;
    bool buttonBClicked = false;
    bool buttonBDoubleClicked = false;
    bool toggleDisplayMirror = false;
    bool toggleLiveViewLock = false;

    bool powerOff = false;
    bool any = false;
};

class ButtonInput {
public:
    explicit ButtonInput(uint32_t resetHoldThresholdMs = UiTheme::kResetHoldMs,
                         uint32_t doubleClickWindowMs = UiTheme::kButtonBDoubleClickMs)
        : _resetHoldThresholdMs(resetHoldThresholdMs),
          _doubleClickWindowMs(doubleClickWindowMs) {}

    void reset();
    ButtonEvents update(bool buttonADown,
                        bool buttonBDown,
                        bool powerOffTriggered,
                        uint32_t nowMs);

    static UserCommand commandFromEvents(const ButtonEvents& events);

private:
    uint32_t _resetHoldThresholdMs = UiTheme::kResetHoldMs;
    uint32_t _doubleClickWindowMs = UiTheme::kButtonBDoubleClickMs;
    bool _buttonAWasDown = false;
    bool _buttonBWasDown = false;
    bool _resetReported = false;
    bool _buttonBClickPending = false;
    bool _buttonBSecondClick = false;
    uint32_t _buttonAPressedAtMs = 0;
    uint32_t _buttonBPressedAtMs = 0;
    uint32_t _buttonBReleasedAtMs = 0;
};

}  // namespace rvf
