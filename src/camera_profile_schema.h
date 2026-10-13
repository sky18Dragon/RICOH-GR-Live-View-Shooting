#pragma once

#include <cstdint>

constexpr uint32_t CAMERA_PROFILE_SCHEMA_VERSION = 4;
constexpr uint16_t CAMERA_CAPABILITY_SCHEMA_VERSION = 1;

enum class WifiCredentialSource : uint8_t {
  Unknown = 0,
  BleFixedHandles = 1,
  BleUuidCharacteristics = 2,
  ManualConfiguration = 3,
};
