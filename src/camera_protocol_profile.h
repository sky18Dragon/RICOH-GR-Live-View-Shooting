#pragma once

#include <cstdint>

enum class RicohProtocolGeneration : uint8_t {
  Unknown = 0,
  Gr2Family = 2,
  Gr3Family = 3,
  Gr4Family = 4,
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
  bool exposesWifiSsid = false;
  bool exposesWifiPassphrase = false;
  bool exposesWifiChannel = false;
  bool exposesWifiBssid = false;
  bool exposesWifiSecurity = false;
  bool supportsHttpLiveView = false;
};

struct CameraProtocolProfile {
  RicohProtocolGeneration generation = RicohProtocolGeneration::Unknown;
  CameraCapabilities capabilities;
  WifiActivationMethod wifiActivationMethod = WifiActivationMethod::Unsupported;
  WifiCredentialMethod wifiCredentialMethod = WifiCredentialMethod::Unsupported;
  bool requiresPasskeyEntry = false;
  bool requiresProtectedReadBeforePairing = false;
  bool standbyModeRequiresFreshReconnect = false;
  uint32_t standbyProbeIntervalMs = 0;
  uint16_t capabilityVersion = 1;
};

struct ProtocolDetectionEvidence {
  bool hasGr3WlanService = false;
  bool hasGr3NetworkTypeCharacteristic = false;
  bool gr4PowerHandleReadSucceeded = false;
};

const CameraProtocolProfile& cameraProtocolProfile(RicohProtocolGeneration generation);
RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence);
const char* ricohProtocolGenerationName(RicohProtocolGeneration generation);

bool protocolAllowsBleSideEffect(const CameraProtocolProfile& profile, BleSideEffect effect);
bool operationModeAllowsWifi(const CameraProtocolProfile& profile,
                             RicohCameraOperationMode mode,
                             bool operationModeReadSucceeded);
bool validGr3WifiChannel(uint8_t channel);
bool validGr3WifiCredentials(const char* ssid, const char* passphrase, uint8_t channel);

