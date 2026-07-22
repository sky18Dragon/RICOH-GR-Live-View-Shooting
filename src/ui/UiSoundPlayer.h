#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "UiTypes.h"

namespace rvf {

class UiSoundPlayer {
public:
    bool begin();
    void play(UiSound sound, uint32_t nowMs);
    void update(uint32_t nowMs);
    bool available() const { return _available; }

private:
    bool _available = false;
    bool _secondTonePending = false;
    uint32_t _secondToneAtMs = 0;
    UiSound _lastSound = UiSound::None;
    uint32_t _lastSoundAtMs = 0;
};

}  // namespace rvf
