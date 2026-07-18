#include "camera_profile_schema.h"

namespace {

bool validProtocolGeneration(uint32_t value) {
  return value == 2 || value == 3 || value == 4;
}

WifiCredentialSource decodeWifiSource(uint32_t value) {
  switch (value) {
    case 1:
      return WifiCredentialSource::BleFixedHandles;
    case 2:
      return WifiCredentialSource::BleUuidCharacteristics;
    case 3:
      return WifiCredentialSource::ManualConfiguration;
    default:
      return WifiCredentialSource::Unknown;
  }
}

}  // namespace

CameraProfileMetadata decodeCameraProfileMetadata(const StoredCameraProfileMetadata& stored) {
  CameraProfileMetadata metadata;
  if (stored.protocolGenerationPresent && validProtocolGeneration(stored.protocolGenerationValue)) {
    metadata.protocolGeneration = static_cast<uint8_t>(stored.protocolGenerationValue);
    metadata.protocolGenerationKnown = true;
  }
  if (stored.capabilityVersionPresent && stored.capabilityVersionValue > 0 &&
      stored.capabilityVersionValue <= UINT16_MAX) {
    metadata.capabilityVersion = static_cast<uint16_t>(stored.capabilityVersionValue);
  }
  if (stored.wifiSourcePresent) {
    metadata.wifiSource = decodeWifiSource(stored.wifiSourceValue);
  }
  metadata.wifiCredentialsValid = stored.wifiCredentialValidityPresent
                                    ? stored.wifiCredentialValidityValue
                                    : stored.legacyWifiValid;
  return metadata;
}

StoredCameraProfileMetadata encodeCameraProfileMetadata(const CameraProfileMetadata& metadata) {
  StoredCameraProfileMetadata stored;
  stored.schemaVersion = CAMERA_PROFILE_SCHEMA_VERSION;
  stored.protocolGenerationPresent = metadata.protocolGenerationKnown &&
                                     validProtocolGeneration(metadata.protocolGeneration);
  stored.protocolGenerationValue = stored.protocolGenerationPresent ? metadata.protocolGeneration : 0;
  stored.capabilityVersionPresent = true;
  stored.capabilityVersionValue = metadata.capabilityVersion == 0
                                    ? CAMERA_CAPABILITY_SCHEMA_VERSION
                                    : metadata.capabilityVersion;
  stored.wifiSourcePresent = metadata.wifiSource != WifiCredentialSource::Unknown;
  stored.wifiSourceValue = static_cast<uint8_t>(metadata.wifiSource);
  stored.wifiCredentialValidityPresent = true;
  stored.wifiCredentialValidityValue = metadata.wifiCredentialsValid;
  stored.legacyWifiValid = metadata.wifiCredentialsValid;
  return stored;
}

