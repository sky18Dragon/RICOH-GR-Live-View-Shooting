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

struct StoredCameraProfileMetadata {
  uint32_t schemaVersion = 3;
  bool protocolGenerationPresent = false;
  uint32_t protocolGenerationValue = 0;
  bool capabilityVersionPresent = false;
  uint32_t capabilityVersionValue = 0;
  bool wifiSourcePresent = false;
  uint32_t wifiSourceValue = 0;
  bool wifiCredentialValidityPresent = false;
  bool wifiCredentialValidityValue = false;
  bool legacyWifiValid = false;
};

struct CameraProfileMetadata {
  uint32_t schemaVersion = CAMERA_PROFILE_SCHEMA_VERSION;
  uint8_t protocolGeneration = 0;
  bool protocolGenerationKnown = false;
  uint16_t capabilityVersion = CAMERA_CAPABILITY_SCHEMA_VERSION;
  WifiCredentialSource wifiSource = WifiCredentialSource::Unknown;
  bool wifiCredentialsValid = false;
};

CameraProfileMetadata decodeCameraProfileMetadata(const StoredCameraProfileMetadata& stored);
StoredCameraProfileMetadata encodeCameraProfileMetadata(const CameraProfileMetadata& metadata);

