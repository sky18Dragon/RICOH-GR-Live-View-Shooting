#include "key2_gesture.h"

namespace rvf {

Key2Gesture Key2GestureTracker::update(bool pressed,
                                       uint32_t nowMs,
                                       uint32_t doublePressWindowMs,
                                       uint32_t longHoldMs) {
    if (pressed) {
        if (!_pressed) {
            _pressed = true;
            _pressedAtMs = nowMs;
            _longHoldReported = false;

            if (_singlePressPending &&
                (nowMs - _firstReleasedAtMs) <= doublePressWindowMs) {
                _secondPress = true;
            } else {
                const bool expiredSinglePress = _singlePressPending;
                _singlePressPending = false;
                _secondPress = false;
                if (expiredSinglePress) {
                    return Key2Gesture::SinglePress;
                }
            }
        }

        if (!_longHoldReported && (nowMs - _pressedAtMs) >= longHoldMs) {
            _longHoldReported = true;
            _singlePressPending = false;
            _secondPress = false;
            return Key2Gesture::LongHold;
        }
        return Key2Gesture::None;
    }

    if (_pressed) {
        _pressed = false;
        if (_longHoldReported) {
            _longHoldReported = false;
            return Key2Gesture::None;
        }
        if (_secondPress) {
            _secondPress = false;
            _singlePressPending = false;
            return Key2Gesture::DoublePress;
        }

        _singlePressPending = true;
        _firstReleasedAtMs = nowMs;
        return Key2Gesture::None;
    }

    if (_singlePressPending &&
        (nowMs - _firstReleasedAtMs) >= doublePressWindowMs) {
        _singlePressPending = false;
        return Key2Gesture::SinglePress;
    }
    return Key2Gesture::None;
}

}  // namespace rvf
