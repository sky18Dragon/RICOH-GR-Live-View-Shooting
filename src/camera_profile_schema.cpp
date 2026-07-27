#include "camera_profile_schema.h"

namespace {

bool validProtocolGeneration(uint32_t value) {
  return value == 2 || value == 3 || value == 4;
}

bool validSecurityProfile(uint32_t value) {
  return value == 1 || value == 2;
}

uint8_t defaultSecurityProfile(uint8_t generation) {
  if (generation == 3) {
    return 1;
  }
  if (generation == 4) {
    return 2;
  }
  return 0;
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
  } else if (stored.schemaVersion <= 3 && stored.legacyBleIdentityPresent) {
    // Every released schema <= 3 firmware was GR IV-only. Treating a saved
    // address as Unknown would force an existing owner through pairing again.
    metadata.protocolGeneration = 4;
    metadata.protocolGenerationKnown = true;
    metadata.migratedLegacyGr4 = true;
  }
  if (stored.securityProfilePresent && validSecurityProfile(stored.securityProfileValue)) {
    metadata.securityProfile = static_cast<uint8_t>(stored.securityProfileValue);
    metadata.securityProfileKnown = true;
  } else if (metadata.protocolGenerationKnown) {
    metadata.securityProfile = defaultSecurityProfile(metadata.protocolGeneration);
    metadata.securityProfileKnown = metadata.securityProfile != 0;
  }
  metadata.bleAuthenticated =
      stored.bleAuthenticatedPresent && stored.bleAuthenticatedValue;
  if (stored.capabilityVersionPresent && stored.capabilityVersionValue > 0 &&
      stored.capabilityVersionValue <= UINT16_MAX) {
    metadata.capabilityVersion = static_cast<uint16_t>(stored.capabilityVersionValue);
  }
  if (stored.wifiSourcePresent) {
    metadata.wifiSource = decodeWifiSource(stored.wifiSourceValue);
  } else if (metadata.migratedLegacyGr4 && stored.legacyWifiValid) {
    metadata.wifiSource = WifiCredentialSource::BleFixedHandles;
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
  stored.securityProfilePresent = metadata.securityProfileKnown &&
                                  validSecurityProfile(metadata.securityProfile);
  stored.securityProfileValue = stored.securityProfilePresent ? metadata.securityProfile : 0;
  stored.bleAuthenticatedPresent = true;
  stored.bleAuthenticatedValue = metadata.bleAuthenticated;
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
