#pragma once

#include <cstdint>

// Debounces the PMIC charging flag for the on-screen indicator.
//
// Near full charge the StickS3 PMIC alternates between charging and top-off
// pauses, so the raw flag flipped every 3-6 s with USB plugged in (log
// 2026-10-13). Turning the indicator on is immediate -- plugging in should
// show at once -- but turning it off needs the raw flag to stay off for
// offHoldMs, so top-off flapping reads as "charging" and an unplug clears
// after the hold.
class ChargingIndicatorFilter {
public:
  explicit ChargingIndicatorFilter(uint32_t offHoldMs) : _offHoldMs(offHoldMs) {}

  // Feeds one raw sample. Returns true when the displayed state changed.
  bool update(bool rawCharging, uint32_t nowMs) {
    if (rawCharging) {
      _offPending = false;
      if (!_shown) {
        _shown = true;
        return true;
      }
      return false;
    }
    if (!_shown) {
      return false;
    }
    if (!_offPending) {
      _offPending = true;
      _offSinceMs = nowMs;
      return false;
    }
    if (static_cast<uint32_t>(nowMs - _offSinceMs) >= _offHoldMs) {
      _shown = false;
      _offPending = false;
      return true;
    }
    return false;
  }

  bool charging() const { return _shown; }

private:
  uint32_t _offHoldMs;
  uint32_t _offSinceMs = 0;
  bool _shown = false;
  bool _offPending = false;
};
