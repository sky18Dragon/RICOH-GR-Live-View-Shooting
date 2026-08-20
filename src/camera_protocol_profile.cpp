#include "camera_protocol_profile.h"

#include <cstring>

namespace {

CameraProtocolProfile makeGr2Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr2Family;
  profile.wifiActivationMethod = WifiActivationMethod::ManualOnly;
  profile.wifiCredentialMethod = WifiCredentialMethod::ManualConfiguration;
  return profile;
}

CameraProtocolProfile makeGr3Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr3Family;
  profile.capabilities.hasBle = true;
  profile.capabilities.supportsBlePairing = true;
  profile.capabilities.supportsBlePowerState = true;
  profile.capabilities.supportsOperationMode = true;
  profile.capabilities.supportsBleShutter = true;
  profile.capabilities.supportsWifiActivation = true;
  profile.capabilities.exposesWifiSsid = true;
  profile.capabilities.exposesWifiPassphrase = true;
  profile.capabilities.exposesWifiChannel = true;
  profile.capabilities.supportsHttpLiveView = true;
  profile.capabilities.supportsHttpShutter = true;  // verified on a GR IIIx
  // Keep BLE connected during LiveView, matching GR IV. This trades some
  // preview throughput for deterministic BLE shutter availability and avoids
  // a disconnect/reconnect transition when leaving the preview.
  profile.capabilities.allowBlePreviewParking = false;
  profile.wifiActivationMethod = WifiActivationMethod::BleNetworkTypeUuid;
  profile.wifiCredentialMethod = WifiCredentialMethod::BleUuidCharacteristics;
  profile.requiresPasskeyEntry = true;
  profile.requiresProtectedReadBeforePairing = true;
  profile.standbyModeRequiresFreshReconnect = true;
  profile.standbyProbeIntervalMs = 8000;
  return profile;
}

CameraProtocolProfile makeGr4Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr4Family;
  profile.capabilities.hasBle = true;
  profile.capabilities.supportsBlePairing = true;
  profile.capabilities.supportsBlePowerState = true;
  profile.capabilities.supportsOperationMode = true;
  profile.capabilities.supportsBleShutter = true;
  profile.capabilities.supportsWifiActivation = true;
  profile.capabilities.exposesWifiSsid = true;
  profile.capabilities.exposesWifiPassphrase = true;
  profile.capabilities.exposesWifiChannel = true;
  profile.capabilities.exposesWifiBssid = true;
  profile.capabilities.exposesWifiSecurity = true;
  profile.capabilities.supportsHttpLiveView = true;
  profile.wifiActivationMethod = WifiActivationMethod::BleFixedHandle;
  profile.wifiCredentialMethod = WifiCredentialMethod::BleFixedHandles;
  return profile;
}

const CameraProtocolProfile kUnknownProfile;
const CameraProtocolProfile kGr2Profile = makeGr2Profile();
const CameraProtocolProfile kGr3Profile = makeGr3Profile();
const CameraProtocolProfile kGr4Profile = makeGr4Profile();

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

const RicohSecurityProfile kUnknownSecurityProfile;
const RicohSecurityProfile kGr3SecurityProfile = makeGr3SecurityProfile();
const RicohSecurityProfile kGr4SecurityProfile = makeGr4SecurityProfile();

}  // namespace

const CameraProtocolProfile& cameraProtocolProfile(RicohProtocolGeneration generation) {
  switch (generation) {
    case RicohProtocolGeneration::Gr2Family:
      return kGr2Profile;
    case RicohProtocolGeneration::Gr3Family:
      return kGr3Profile;
    case RicohProtocolGeneration::Gr4Family:
      return kGr4Profile;
    case RicohProtocolGeneration::Unknown:
      return kUnknownProfile;
  }
  return kUnknownProfile;
}

RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence) {
  const bool gr3Evidence =
      evidence.gattDiscoveryComplete &&
      evidence.hasGr3WlanService &&
      evidence.hasGr3NetworkTypeCharacteristic &&
      evidence.hasGr3SsidCharacteristic &&
      evidence.hasGr3PassphraseCharacteristic &&
      evidence.hasCameraService &&
      evidence.hasOperationModeCharacteristic &&
      evidence.hasShootingService &&
      evidence.hasShootingFlavorCharacteristic &&
      evidence.hasOperationRequestCharacteristic &&
      evidence.hasControlService;
  const bool gr4Evidence =
      evidence.hasCameraService &&
      evidence.hasOperationModeCharacteristic &&
      evidence.hasShootingService &&
      evidence.hasShootingFlavorCharacteristic &&
      evidence.hasOperationRequestCharacteristic &&
      evidence.hasControlService &&
      evidence.hasGr4PowerCharacteristicAtExpectedHandle &&
      evidence.gr4WlanHandlesInExpectedService &&
      evidence.gr4KnownWlanUuidHandleMapping &&
      evidence.gr4ExpectedWlanCharacteristicCount == 6;
  const bool partialGr4FixedHandleEvidence =
      evidence.hasGr4PowerCharacteristicAtExpectedHandle ||
      evidence.gr4WlanHandlesInExpectedService ||
      evidence.gr4KnownWlanUuidHandleMapping ||
      evidence.gr4ExpectedWlanCharacteristicCount > 0;

  // GR IV also exposes the WLAN service/Network Type UUIDs used by GR III.
  // Never classify those shared UUIDs as GR III after an interrupted GATT
  // discovery. GR III requires a complete, coherent read-only profile.
  // GR IV requires all six WLAN handles inside the WLAN service, the Power
  // handle inside Camera service, and known UUIDs at their expected handles.
  if (gr4Evidence) {
    return RicohProtocolGeneration::Gr4Family;
  }
  if (gr3Evidence && !partialGr4FixedHandleEvidence) {
    return RicohProtocolGeneration::Gr3Family;
  }
  return RicohProtocolGeneration::Unknown;
}

const char* ricohProtocolGenerationName(RicohProtocolGeneration generation) {
  switch (generation) {
    case RicohProtocolGeneration::Gr2Family:
      return "GR2_FAMILY";
    case RicohProtocolGeneration::Gr3Family:
      return "GR3_FAMILY";
    case RicohProtocolGeneration::Gr4Family:
      return "GR4_FAMILY";
    case RicohProtocolGeneration::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

RicohSecurityProfileId securityProfileForGeneration(RicohProtocolGeneration generation) {
  switch (generation) {
    case RicohProtocolGeneration::Gr3Family:
      return RicohSecurityProfileId::Gr3Passkey;
    case RicohProtocolGeneration::Gr4Family:
      return RicohSecurityProfileId::Gr4Legacy;
    case RicohProtocolGeneration::Gr2Family:
    case RicohProtocolGeneration::Unknown:
      return RicohSecurityProfileId::Unknown;
  }
  return RicohSecurityProfileId::Unknown;
}

bool canPromoteDiscoveryConnectionInPlace(RicohSecurityProfileId activeProfile,
                                          RicohProtocolGeneration detectedGeneration) {
  if (detectedGeneration != RicohProtocolGeneration::Gr4Family) {
    return false;
  }

  // Unknown discovery mode deliberately applies the byte-for-byte GR IV
  // legacy security baseline. Promoting that live connection only changes
  // the selected protocol identity; no GAP address or SMP setting changes.
  return activeProfile == RicohSecurityProfileId::Unknown ||
         activeProfile == RicohSecurityProfileId::Gr4Legacy;
}

const RicohSecurityProfile& ricohSecurityProfile(RicohSecurityProfileId id) {
  switch (id) {
    case RicohSecurityProfileId::Gr3Passkey:
      return kGr3SecurityProfile;
    case RicohSecurityProfileId::Gr4Legacy:
      return kGr4SecurityProfile;
    case RicohSecurityProfileId::Unknown:
      return kUnknownSecurityProfile;
  }
  return kUnknownSecurityProfile;
}

const char* ricohSecurityProfileName(RicohSecurityProfileId id) {
  switch (id) {
    case RicohSecurityProfileId::Gr3Passkey:
      return "GR3_PASSKEY";
    case RicohSecurityProfileId::Gr4Legacy:
      return "GR4_LEGACY";
    case RicohSecurityProfileId::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

bool protocolAllowsBleSideEffect(const CameraProtocolProfile& profile, BleSideEffect effect) {
  if (profile.generation == RicohProtocolGeneration::Unknown ||
      profile.generation == RicohProtocolGeneration::Gr2Family) {
    return false;
  }
  switch (effect) {
    case BleSideEffect::WifiActivation:
      return profile.capabilities.supportsWifiActivation &&
             (profile.wifiActivationMethod == WifiActivationMethod::BleNetworkTypeUuid ||
              profile.wifiActivationMethod == WifiActivationMethod::BleFixedHandle);
    case BleSideEffect::CameraPowerWrite:
      return false;
    case BleSideEffect::Shutter:
      return profile.capabilities.supportsBleShutter;
  }
  return false;
}

bool operationModeAllowsWifi(const CameraProtocolProfile& profile,
                             RicohCameraOperationMode mode,
                             bool operationModeReadSucceeded) {
  if (!protocolAllowsBleSideEffect(profile, BleSideEffect::WifiActivation)) {
    return false;
  }
  if (profile.generation == RicohProtocolGeneration::Gr3Family) {
    // GR III keeps the stricter requirement of a successful read -- an unknown
    // mode must never authorise a WLAN write on this generation -- but the set
    // of safe modes is the same as GR IV's. A GR IIIx sitting ready to shoot
    // reports Other (0x03), not Capture (0x00), so an allowlist of Capture
    // alone blocks Wi-Fi activation for the whole session. Verified on a GR
    // IIIx (firmware 1.60): mode 0x03 followed by WLAN on, credentials read,
    // /v1/props and a sustained LiveView stream.
    return operationModeReadSucceeded &&
           mode != RicohCameraOperationMode::BleStartup &&
           mode != RicohCameraOperationMode::PowerOffTransfer;
  }
  if (profile.generation == RicohProtocolGeneration::Gr4Family) {
    // Preserve the established GR IV policy: an unavailable mode read does not
    // supersede the power-state gate, while the two known standby modes do.
    return !operationModeReadSucceeded ||
           (mode != RicohCameraOperationMode::BleStartup &&
            mode != RicohCameraOperationMode::PowerOffTransfer);
  }
  return false;
}

bool validGr3WifiChannel(uint8_t channel) {
  return channel <= 11;
}

bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel) {
  return ssid != nullptr && passphrase != nullptr &&
         std::strlen(ssid) > 0 && std::strlen(passphrase) > 0 &&
         validGr3WifiChannel(channel);
}
