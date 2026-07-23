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
  if (evidence.hasGr3WlanService && evidence.hasGr3NetworkTypeCharacteristic) {
    return RicohProtocolGeneration::Gr3Family;
  }
  if (evidence.gr4PowerHandleReadSucceeded) {
    return RicohProtocolGeneration::Gr4Family;
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
    return operationModeReadSucceeded && mode == RicohCameraOperationMode::Capture;
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
