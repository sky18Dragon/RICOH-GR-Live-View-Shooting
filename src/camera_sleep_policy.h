#pragma once

#include <cstdint>

inline bool cameraSleepAutoPowerOffDue(bool sleepActive,
                                       uint32_t enteredAtMs,
                                       uint32_t nowMs,
                                       uint32_t timeoutMs) {
    return sleepActive &&
           timeoutMs > 0 &&
           static_cast<uint32_t>(nowMs - enteredAtMs) >= timeoutMs;
}
