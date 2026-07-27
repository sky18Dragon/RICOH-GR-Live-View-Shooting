#include "ButtonInput.h"

#include "UiAnimator.h"

namespace rvf {

void ButtonInput::reset() {
    _buttonAWasDown = false;
    _buttonBWasDown = false;
    _resetReported = false;
    _buttonAPressedAtMs = 0;
    _buttonBPressedAtMs = 0;
}

ButtonEvents ButtonInput::update(bool buttonADown,
                                 bool buttonBDown,
                                 bool powerOffTriggered,
                                 uint32_t nowMs) {
    ButtonEvents events;

    if (buttonADown && !_buttonAWasDown) {
        _buttonAPressedAtMs = nowMs;
        events.buttonADown = true;
    }
    if (buttonADown) {
        events.buttonAHeld = true;
        events.buttonAHoldMs = uiElapsedMs(nowMs, _buttonAPressedAtMs);
    } else if (_buttonAWasDown) {
        events.buttonAReleased = true;
        events.buttonAHoldMs = uiElapsedMs(nowMs, _buttonAPressedAtMs);
        events.buttonA = true;
    }

    if (buttonBDown && !_buttonBWasDown) {
        _buttonBPressedAtMs = nowMs;
        _resetReported = false;
    }
    if (buttonBDown) {
        events.resetHoldActive = true;
        events.resetHoldMs = uiElapsedMs(nowMs, _buttonBPressedAtMs);
        if (_resetHoldThresholdMs == 0) {
            events.resetHoldProgress = 1.0f;
        } else {
            events.resetHoldProgress = uiClamp01(
                static_cast<float>(events.resetHoldMs) / static_cast<float>(_resetHoldThresholdMs));
        }
        if (!_resetReported && events.resetHoldMs >= _resetHoldThresholdMs) {
            _resetReported = true;
            events.resetPairing = true;
        }
    } else if (_buttonBWasDown) {
        _resetReported = false;
    }

    events.powerOff = powerOffTriggered;
    events.any = events.buttonADown || events.buttonAReleased || events.resetHoldActive ||
                 events.resetPairing || events.powerOff;
    _buttonAWasDown = buttonADown;
    _buttonBWasDown = buttonBDown;
    return events;
}

UserCommand ButtonInput::commandFromEvents(const ButtonEvents& events) {
    if (events.powerOff) return UserCommand::PowerOff;
    if (events.resetPairing) return UserCommand::ResetPairing;
    if (events.buttonAReleased) return UserCommand::Shoot;
    return UserCommand::None;
}

}  // namespace rvf
