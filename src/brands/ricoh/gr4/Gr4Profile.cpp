#include "Gr4Profile.h"

namespace {

CameraProtocolProfile makeGr4Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr4Family;
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
  profile.capabilities.exposesWifiBssid = true;
  profile.capabilities.exposesWifiSecurity = true;
  profile.capabilities.supportsHttpLiveView = true;
  profile.capabilities.supportsHttpShutter = true;  // verified on a GR IV
  profile.capabilities.supportsHttpFocusSetting = true;
  // Keep BLE connected during LiveView and shoot over BLE, matching GR III.
  profile.capabilities.allowBlePreviewParking = false;
  // GR IV, 2026-10-13: the AP alternated between channels 6 and 11 across
  // landscape turns, and the first BLE read reported channel 1 while the AP
  // was on 11.
  profile.capabilities.wifiChannelChangesOnRestart = true;
  profile.wifiActivationMethod = WifiActivationMethod::BleFixedHandle;
  profile.wifiCredentialMethod = WifiCredentialMethod::BleFixedHandles;
  profile.requiresLegacyShutterFlavorWrite = true;
  profile.securityProfile = RicohSecurityProfileId::Gr4Legacy;
  // Unknown discovery mode deliberately applies the byte-for-byte GR IV
  // legacy security baseline. Promoting that live connection only changes
  // the selected protocol identity; no GAP address or SMP setting changes.
  profile.discoveryConnectionPromotable = true;
  return profile;
}

RicohSecurityProfile makeGr4SecurityProfile() {
  RicohSecurityProfile profile;
  profile.id = RicohSecurityProfileId::Gr4Legacy;
  profile.ioCapability = RicohBleIoCapability::DisplayYesNo;
  profile.ownAddressMode = RicohBleOwnAddressMode::RpaPublicDefault;
  profile.distributeEncryptionKey = true;
  profile.distributeIdentityKey = true;
  profile.usesFixedPasskey = true;
  profile.fixedPasskey = 123456;
  return profile;
}

}  // namespace

const CameraProtocolProfile& gr4CameraProtocolProfile() {
  static const CameraProtocolProfile profile = makeGr4Profile();
  return profile;
}

const RicohSecurityProfile& gr4SecurityProfile() {
  static const RicohSecurityProfile profile = makeGr4SecurityProfile();
  return profile;
}
