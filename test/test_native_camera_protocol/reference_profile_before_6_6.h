#pragma once

// camera_protocol_profile.cpp as it was before stage 6.6 moved per-model
// rules into profile parameters. Verbatim except for the namespace; the
// stage 6.6 tests compare the new code against it exhaustively.

#include "camera_protocol_profile.h"

namespace reference_before_6_6 {

CameraProtocolProfile makeGr2Profile() {
  CameraProtocolProfile profile;
  profile.generation = RicohProtocolGeneration::Gr2Family;
  // GR II is a Wi-Fi-only camera. The official GR Remote web application uses
  // /v1/liveview and the camera/shoot HTTP family at 192.168.0.1; WLAN must be
  // enabled on the body and its camera-specific WPA2 credentials configured
  // manually on the controller.
  profile.capabilities.supportsHttpLiveView = true;
  profile.capabilities.supportsHttpShutter = true;
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
    case BleSideEffect::WifiDeactivation:
      return profile.capabilities.supportsWifiDeactivation &&
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
  if (!reference_before_6_6::protocolAllowsBleSideEffect(profile, BleSideEffect::WifiActivation)) {
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

bool operationModeWaitsForCameraReady(const CameraProtocolProfile& profile,
                                      RicohCameraOperationMode mode,
                                      bool operationModeReadSucceeded) {
  return profile.generation == RicohProtocolGeneration::Gr3Family &&
         operationModeReadSucceeded &&
         mode == RicohCameraOperationMode::BleStartup;
}

bool operationModeAllowsWifiProbe(const CameraProtocolProfile& profile,
                                  RicohCameraOperationMode mode,
                                  bool operationModeReadSucceeded) {
  return reference_before_6_6::protocolAllowsBleSideEffect(profile, BleSideEffect::WifiActivation) &&
         (reference_before_6_6::operationModeAllowsWifi(profile, mode, operationModeReadSucceeded) ||
          reference_before_6_6::operationModeWaitsForCameraReady(profile, mode, operationModeReadSucceeded));
}

}  // namespace reference_before_6_6
