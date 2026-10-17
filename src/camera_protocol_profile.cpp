#include "camera_protocol_profile.h"

#include "brands/ricoh/RicohModelSelection.h"
#if RVF_RICOH_GR2
#include "brands/ricoh/gr2/Gr2Profile.h"
#endif
#if RVF_RICOH_GR3
#include "brands/ricoh/gr3/Gr3Profile.h"
#endif
#if RVF_RICOH_GR4
#include "brands/ricoh/gr4/Gr4Profile.h"
#endif

// Shared Ricoh policy. Per-model differences are parameters of
// CameraProtocolProfile, set in brands/ricoh/<model>/<Model>Profile.cpp;
// nothing below branches on a model.

namespace {
const CameraProtocolProfile kUnknownProfile;
const RicohSecurityProfile kUnknownSecurityProfile;

bool isStandbyOperationMode(RicohCameraOperationMode mode) {
  return mode == RicohCameraOperationMode::BleStartup ||
         mode == RicohCameraOperationMode::PowerOffTransfer;
}
}  // namespace


const CameraProtocolProfile& cameraProtocolProfile(RicohProtocolGeneration generation) {
  switch (generation) {
#if RVF_RICOH_GR2
    case RicohProtocolGeneration::Gr2Family:
      return gr2CameraProtocolProfile();
#endif
#if RVF_RICOH_GR3
    case RicohProtocolGeneration::Gr3Family:
      return gr3CameraProtocolProfile();
#endif
#if RVF_RICOH_GR4
    case RicohProtocolGeneration::Gr4Family:
      return gr4CameraProtocolProfile();
#endif
    default:
      // Unknown, or a model this firmware does not contain: every BLE side
      // effect stays blocked.
      return kUnknownProfile;
  }
  return kUnknownProfile;
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
  return cameraProtocolProfile(generation).securityProfile;
}

bool canPromoteDiscoveryConnectionInPlace(RicohSecurityProfileId activeProfile,
                                          RicohProtocolGeneration detectedGeneration) {
  const CameraProtocolProfile& detected = cameraProtocolProfile(detectedGeneration);
  return detected.discoveryConnectionPromotable &&
         (activeProfile == RicohSecurityProfileId::Unknown ||
          activeProfile == detected.securityProfile);
}

const RicohSecurityProfile& ricohSecurityProfile(RicohSecurityProfileId id) {
  switch (id) {
#if RVF_RICOH_GR3
    case RicohSecurityProfileId::Gr3Passkey:
      return gr3SecurityProfile();
#endif
#if RVF_RICOH_GR4
    case RicohSecurityProfileId::Gr4Legacy:
      return gr4SecurityProfile();
#endif
    default:
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

bool cameraIsWifiOnly(const CameraProtocolProfile& profile) {
  return profile.wifiActivationMethod == WifiActivationMethod::ManualOnly;
}

bool cameraNeedsManualWifiProvisioning(const CameraProtocolProfile& profile) {
  return profile.wifiCredentialMethod == WifiCredentialMethod::ManualConfiguration;
}

bool protocolAllowsBleSideEffect(const CameraProtocolProfile& profile, BleSideEffect effect) {
  if (!profile.capabilities.hasBle) {
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
  if (!protocolAllowsBleSideEffect(profile, BleSideEffect::WifiActivation)) {
    return false;
  }
  // GR III requires a successful read -- an unknown mode must never authorise
  // a WLAN write there -- while GR IV lets an unreadable mode defer to the
  // power-state gate. Both block the two known standby modes. A GR IIIx ready
  // to shoot reports Other (0x03), not Capture, so the rule is "not standby"
  // rather than "is Capture" (verified on GR IIIx firmware 1.60).
  if (!operationModeReadSucceeded) {
    return !profile.wifiRequiresOperationModeRead;
  }
  return !isStandbyOperationMode(mode);
}

bool operationModeWaitsForCameraReady(const CameraProtocolProfile& profile,
                                      RicohCameraOperationMode mode,
                                      bool operationModeReadSucceeded) {
  return profile.waitsInBleStartupMode &&
         operationModeReadSucceeded &&
         mode == RicohCameraOperationMode::BleStartup;
}

bool operationModeAllowsWifiProbe(const CameraProtocolProfile& profile,
                                  RicohCameraOperationMode mode,
                                  bool operationModeReadSucceeded) {
  return protocolAllowsBleSideEffect(profile, BleSideEffect::WifiActivation) &&
         (operationModeAllowsWifi(profile, mode, operationModeReadSucceeded) ||
          operationModeWaitsForCameraReady(profile, mode, operationModeReadSucceeded));
}

bool operationModeListContains(const uint8_t* data, size_t length, RicohCameraOperationMode mode) {
  if (data == nullptr || length == 0) {
    return false;
  }
  const size_t count = data[0];
  const uint8_t wanted = static_cast<uint8_t>(mode);
  for (size_t i = 1; i <= count && i < length; ++i) {
    if (data[i] == wanted) {
      return true;
    }
  }
  return false;
}


