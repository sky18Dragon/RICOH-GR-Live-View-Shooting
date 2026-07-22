#pragma once

#include <cstdint>

#include "UiTheme.h"
#include "UserCommand.h"

namespace rvf {

struct ButtonEvents {
    bool buttonA = false;  // Compatibility alias: one Shoot request on release.
    bool buttonADown = false;
    bool buttonAHeld = false;
    bool buttonAReleased = false;
    uint32_t buttonAHoldMs = 0;

    bool resetHoldActive = false;
    float resetHoldProgress = 0.0f;
    uint32_t resetHoldMs = 0;
    bool resetPairing = false;

    bool powerOff = false;
    bool any = false;
};

class ButtonInput {
public:
    explicit ButtonInput(uint32_t resetHoldThresholdMs = UiTheme::kResetHoldMs)
        : _resetHoldThresholdMs(resetHoldThresholdMs) {}

    void reset();
    ButtonEvents update(bool buttonADown,
                        bool buttonBDown,
                        bool powerOffTriggered,
                        uint32_t nowMs);

    static UserCommand commandFromEvents(const ButtonEvents& events);

private:
    uint32_t _resetHoldThresholdMs = UiTheme::kResetHoldMs;
    bool _buttonAWasDown = false;
    bool _buttonBWasDown = false;
    bool _resetReported = false;
    uint32_t _buttonAPressedAtMs = 0;
    uint32_t _buttonBPressedAtMs = 0;
};

}  // namespace rvf
