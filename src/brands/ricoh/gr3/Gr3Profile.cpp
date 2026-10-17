#include "Gr3Profile.h"

#include <cstring>

namespace {

CameraProtocolProfile makeGr3Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr3Family;
  profile.capabilities.hasBle = true;
  profile.capabilities.supportsBlePairing = true;
  profile.capabilities.supportsBlePowerState = true;
  profile.capabilities.supportsOperationMode = true;
  profile.capabilities.supportsBleShutter = true;
  profile.capabilities.supportsWifiActivation = true;
  profile.capabilities.supportsWifiDeactivation = true;
  profile.capabilities.exposesWifiSsid = true;
  profile.capabilities.exposesWifiPassphrase = true;
  profile.capabilities.exposesWifiChannel = true;
  profile.capabilities.supportsHttpLiveView = true;
  profile.capabilities.supportsHttpShutter = true;  // verified on a GR IIIx
  profile.capabilities.supportsHttpFocusSetting = true;
  // Keep BLE connected during LiveView and shoot over BLE. Releasing BLE and
  // shooting over HTTP dropped the GR III LiveView link on capture, so this
  // generation stays on the BLE shutter path.
  profile.capabilities.allowBlePreviewParking = false;
  profile.wifiActivationMethod = WifiActivationMethod::BleNetworkTypeUuid;
  profile.wifiCredentialMethod = WifiCredentialMethod::BleUuidCharacteristics;
  profile.requiresPasskeyEntry = true;
  profile.requiresProtectedReadBeforePairing = true;
  profile.standbyModeRequiresFreshReconnect = true;
  profile.standbyProbeIntervalMs = 8000;
  profile.securityProfile = RicohSecurityProfileId::Gr3Passkey;
  profile.wifiRequiresOperationModeRead = true;
  profile.waitsInBleStartupMode = true;
  profile.wifiRequiresAuthenticatedLink = true;
  return profile;
}

RicohSecurityProfile makeGr3SecurityProfile() {
  RicohSecurityProfile profile;
  profile.id = RicohSecurityProfileId::Gr3Passkey;
  profile.ioCapability = RicohBleIoCapability::KeyboardDisplay;
  profile.ownAddressMode = RicohBleOwnAddressMode::Public;
  profile.distributeEncryptionKey = true;
  profile.distributeIdentityKey = true;
  profile.distributeSigningKey = true;
  return profile;
}

}  // namespace

const CameraProtocolProfile& gr3CameraProtocolProfile() {
  static const CameraProtocolProfile profile = makeGr3Profile();
  return profile;
}

const RicohSecurityProfile& gr3SecurityProfile() {
  static const RicohSecurityProfile profile = makeGr3SecurityProfile();
  return profile;
}

bool validGr3WifiChannel(uint8_t channel) {
  return channel <= 11;
}

bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel) {
  return ssid != nullptr && passphrase != nullptr &&
         std::strlen(ssid) > 0 && std::strlen(passphrase) > 0 &&
         validGr3WifiChannel(channel);
}
