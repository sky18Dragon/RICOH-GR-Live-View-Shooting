#pragma once

#include <cstdint>

namespace rvf {

enum class Key2Gesture : uint8_t {
    None,
    SinglePress,
    DoublePress,
    LongHold,
};

class Key2GestureTracker {
public:
    Key2Gesture update(bool pressed,
                       uint32_t nowMs,
                       uint32_t doublePressWindowMs,
                       uint32_t longHoldMs);

private:
    bool _pressed = false;
    bool _longHoldReported = false;
    bool _singlePressPending = false;
    bool _secondPress = false;
    uint32_t _pressedAtMs = 0;
    uint32_t _firstReleasedAtMs = 0;
};

}  // namespace rvf
