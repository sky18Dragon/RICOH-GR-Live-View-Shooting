#pragma once

#include <cstddef>
#include <cstdint>

enum class RicohProtocolGeneration : uint8_t {
  Unknown = 0,
  Gr3Family = 3,
  Gr4Family = 4,
};

enum class WifiActivationMethod : uint8_t {
  Unsupported = 0,
  BleNetworkTypeUuid,
  BleFixedHandle,
};

enum class WifiCredentialMethod : uint8_t {
  Unsupported = 0,
  BleUuidCharacteristics,
  BleFixedHandles,
};

// Every field here must have a live reader in ricoh_ble_client.cpp; the Wi-Fi
// dispatch switches on the method fields, so editing them changes behavior.
struct CameraProtocolProfile {
  RicohProtocolGeneration generation = RicohProtocolGeneration::Unknown;
  WifiActivationMethod wifiActivationMethod = WifiActivationMethod::Unsupported;
  WifiCredentialMethod wifiCredentialMethod = WifiCredentialMethod::Unsupported;
  bool requiresAuthenticatedLink = false;
};

struct ProtocolDetectionEvidence {
  bool hasGr3WlanService = false;
  bool hasGr3NetworkTypeCharacteristic = false;
  bool hasGr4ControlService = false;
  bool gr4PowerHandleReadSucceeded = false;
};

const CameraProtocolProfile& cameraProtocolProfile(RicohProtocolGeneration generation);
RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence);
const char* ricohProtocolGenerationName(RicohProtocolGeneration generation);
bool validGr3WifiChannel(uint8_t channel);
bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel);
uint8_t parseGr3WifiChannel(const uint8_t* data, size_t length);
