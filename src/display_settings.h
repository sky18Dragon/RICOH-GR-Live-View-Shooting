#pragma once

#include <cstdint>

namespace rvf {

constexpr uint8_t kDefaultDisplayRotation = 1;

inline uint8_t normalizeDisplayRotation(uint8_t rotation) {
  return rotation == 3 ? 3 : kDefaultDisplayRotation;
}

struct DisplaySettings {
  uint8_t rotation = kDefaultDisplayRotation;
  bool mirrored = false;
};

}  // namespace rvf
