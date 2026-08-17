#include "ButtonInput.h"

#include "UiAnimator.h"

namespace rvf {

void ButtonInput::reset() {
    _buttonAWasDown = false;
    _buttonBWasDown = false;
    _resetReported = false;
    _buttonBClickPending = false;
    _buttonBSecondClick = false;
    _buttonAPressedAtMs = 0;
    _buttonBPressedAtMs = 0;
    _buttonBReleasedAtMs = 0;
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

    if (!buttonBDown && !_buttonBWasDown && _buttonBClickPending &&
        uiElapsedMs(nowMs, _buttonBReleasedAtMs) >= _doubleClickWindowMs) {
        _buttonBClickPending = false;
        events.toggleDisplayMirror = true;
    }

    if (buttonBDown && !_buttonBWasDown) {
        _buttonBPressedAtMs = nowMs;
        _resetReported = false;
        _buttonBSecondClick = _buttonBClickPending &&
                              uiElapsedMs(nowMs, _buttonBReleasedAtMs) <= _doubleClickWindowMs;
        if (_buttonBClickPending && !_buttonBSecondClick) {
            _buttonBClickPending = false;
            events.toggleDisplayMirror = true;
        }
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
            _buttonBClickPending = false;
            _buttonBSecondClick = false;
            events.resetPairing = true;
        }
    } else if (_buttonBWasDown) {
        const uint32_t heldMs = uiElapsedMs(nowMs, _buttonBPressedAtMs);
        if (!_resetReported && heldMs < _resetHoldThresholdMs) {
            if (_buttonBSecondClick) {
                _buttonBClickPending = false;
                _buttonBSecondClick = false;
                events.toggleLiveViewLock = true;
            } else {
                _buttonBClickPending = true;
                _buttonBReleasedAtMs = nowMs;
            }
        }
        _resetReported = false;
        _buttonBSecondClick = false;
    }

    events.powerOff = powerOffTriggered;
    events.any = events.buttonADown || events.buttonAReleased || events.resetHoldActive ||
                 events.resetPairing || events.toggleDisplayMirror ||
                 events.toggleLiveViewLock || events.powerOff;
    _buttonAWasDown = buttonADown;
    _buttonBWasDown = buttonBDown;
    return events;
}

UserCommand ButtonInput::commandFromEvents(const ButtonEvents& events) {
    if (events.powerOff) return UserCommand::PowerOff;
    if (events.resetPairing) return UserCommand::ResetPairing;
    if (events.toggleLiveViewLock) return UserCommand::ToggleLiveViewLock;
    if (events.toggleDisplayMirror) return UserCommand::ToggleDisplayMirror;
    if (events.buttonADown) return UserCommand::Shoot;
    return UserCommand::None;
}

}  // namespace rvf
