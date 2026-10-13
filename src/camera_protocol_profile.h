#pragma once

#include <cstddef>
#include <cstdint>

enum class RicohProtocolGeneration : uint8_t {
  Unknown = 0,
  Gr2Family = 2,
  Gr3Family = 3,
  Gr4Family = 4,
};

inline bool ricohProtocolMatchesExpectedGeneration(
    RicohProtocolGeneration expected,
    RicohProtocolGeneration detected) {
  return expected == RicohProtocolGeneration::Unknown || expected == detected;
}

enum class RicohSecurityProfileId : uint8_t {
  Unknown = 0,
  Gr3Passkey = 1,
  Gr4Legacy = 2,
};

enum class RicohBleIoCapability : uint8_t {
  DisplayYesNo = 1,
  KeyboardDisplay = 2,
};

enum class RicohBleOwnAddressMode : uint8_t {
  Public = 1,
  RpaPublicDefault = 2,
};

struct RicohSecurityProfile {
  RicohSecurityProfileId id = RicohSecurityProfileId::Unknown;
  RicohBleIoCapability ioCapability = RicohBleIoCapability::DisplayYesNo;
  RicohBleOwnAddressMode ownAddressMode = RicohBleOwnAddressMode::RpaPublicDefault;
  bool distributeEncryptionKey = false;
  bool distributeIdentityKey = false;
  bool distributeSigningKey = false;
  bool usesFixedPasskey = false;
  uint32_t fixedPasskey = 0;
};

enum class WifiActivationMethod : uint8_t {
  Unsupported = 0,
  ManualOnly,
  BleNetworkTypeUuid,
  BleFixedHandle,
};

enum class WifiCredentialMethod : uint8_t {
  Unsupported = 0,
  ManualConfiguration,
  BleUuidCharacteristics,
  BleFixedHandles,
};

enum class RicohCameraOperationMode : uint8_t {
  Unknown = 0xFF,
  Capture = 0x00,
  Playback = 0x01,
  BleStartup = 0x02,
  Other = 0x03,
  PowerOffTransfer = 0x04,
};

enum class BleSideEffect : uint8_t {
  WifiActivation,
  WifiDeactivation,
  CameraPowerWrite,
  Shutter,
};

struct CameraCapabilities {
  bool hasBle = false;
  bool supportsBlePairing = false;
  bool supportsBlePowerState = false;
  bool supportsOperationMode = false;
  bool supportsBleShutter = false;
  bool supportsWifiActivation = false;
  bool supportsWifiDeactivation = false;
  bool exposesWifiSsid = false;
  bool exposesWifiPassphrase = false;
  bool exposesWifiChannel = false;
  bool exposesWifiBssid = false;
  bool exposesWifiSecurity = false;
  bool supportsHttpLiveView = false;
  // POST /v1/camera/shoot, confirmed on the camera rather than assumed.
  bool supportsHttpShutter = false;
  // PUT /v1/params/lens body focusSetting=<enum> before LiveView opens.
  // GR III/IV share this GR Remote write shape; GR II uses shoot?af=camera.
  bool supportsHttpFocusSetting = false;
  // Landscape is Wi-Fi only: BLE brings the camera AP up and is released once
  // the preview streams, so the shutter must work over HTTP. A generation must
  // opt in explicitly; one that does not keeps BLE through the preview.
  bool allowBlePreviewParking = false;
  // The camera AP may come back on a different channel after each WLAN ON,
  // so the cached channel is only a guess: keep the channel-hinted join short
  // and fall back to a full scan quickly.
  bool wifiChannelChangesOnRestart = false;
};

struct CameraProtocolProfile {
  RicohProtocolGeneration generation = RicohProtocolGeneration::Unknown;
  CameraCapabilities capabilities;
  WifiActivationMethod wifiActivationMethod = WifiActivationMethod::Unsupported;
  WifiCredentialMethod wifiCredentialMethod = WifiCredentialMethod::Unsupported;
  bool requiresPasskeyEntry = false;
  bool requiresProtectedReadBeforePairing = false;
  // Preserve the established GR IV shutter preparation sequence. GR III must
  // send OperationRequest directly: the same UUID is its Drive Mode setting,
  // and rewriting it immediately before START can consume the first shot.
  bool requiresLegacyShutterFlavorWrite = false;
  bool standbyModeRequiresFreshReconnect = false;
  uint32_t standbyProbeIntervalMs = 0;
  uint16_t capabilityVersion = 1;
};

struct ProtocolDetectionEvidence {
  bool gattDiscoveryComplete = false;
  bool hasGr3WlanService = false;
  bool hasGr3NetworkTypeCharacteristic = false;
  bool hasGr3SsidCharacteristic = false;
  bool hasGr3PassphraseCharacteristic = false;
  bool hasGr3ChannelCharacteristic = false;
  bool hasCameraService = false;
  bool hasOperationModeCharacteristic = false;
  bool hasShootingService = false;
  bool hasShootingFlavorCharacteristic = false;
  bool hasOperationRequestCharacteristic = false;
  bool hasControlService = false;
  bool hasGr4PowerCharacteristicAtExpectedHandle = false;
  bool gr4WlanHandlesInExpectedService = false;
  bool gr4KnownWlanUuidHandleMapping = false;
  uint8_t gr4ExpectedWlanCharacteristicCount = 0;
};

const CameraProtocolProfile& cameraProtocolProfile(RicohProtocolGeneration generation);
RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence);
const char* ricohProtocolGenerationName(RicohProtocolGeneration generation);
RicohSecurityProfileId securityProfileForGeneration(RicohProtocolGeneration generation);
bool canPromoteDiscoveryConnectionInPlace(RicohSecurityProfileId activeProfile,
                                          RicohProtocolGeneration detectedGeneration);
const RicohSecurityProfile& ricohSecurityProfile(RicohSecurityProfileId id);
const char* ricohSecurityProfileName(RicohSecurityProfileId id);

bool protocolAllowsBleSideEffect(const CameraProtocolProfile& profile, BleSideEffect effect);
bool operationModeAllowsWifi(const CameraProtocolProfile& profile,
                             RicohCameraOperationMode mode,
                             bool operationModeReadSucceeded);
// A powered-on GR III family camera that reports bleStartup is not shutting
// down: the caller should keep the link and wait instead of entering the
// camera sleep guard.
bool operationModeWaitsForCameraReady(const CameraProtocolProfile& profile,
                                      RicohCameraOperationMode mode,
                                      bool operationModeReadSucceeded);
// Whether an explicit NetworkType probe may be sent despite the mode gate.
// The camera arbitrates NetworkType writes itself and rejects them with a
// GATT write error, so a probe cannot raise WLAN against the camera's will.
bool operationModeAllowsWifiProbe(const CameraProtocolProfile& profile,
                                  RicohCameraOperationMode mode,
                                  bool operationModeReadSucceeded);
// OperationModeList payload: byte 0 is the entry count, followed by one byte
// per mode the camera currently accepts.
bool operationModeListContains(const uint8_t* data, size_t length, RicohCameraOperationMode mode);
bool validGr3WifiChannel(uint8_t channel);
bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel);
