#pragma once

#include <Arduino.h>

#include "ble_pairing_policy.h"
#include "camera_protocol_profile.h"
#include "ricoh/RicohBleProtocolRouter.h"

struct RicohBleDeviceInfo {
  bool found = false;
  String name;
  String address;
  uint8_t addressType = 0;
  int rssi = 0;
  bool connectable = false;
  bool hasInfoService = false;
  bool hasCameraService = false;
  bool hasShootingService = false;
  bool hasControlService = false;
};

struct RicohBleWifiCredentials {
  bool valid = false;
  bool encryptedPassphrase = false;
  int securityType = -1;
  uint16_t frequencyMhz = 0;
  uint8_t channel = 0;
  String ssid;
  String passphrase;
  String bssid;
};

struct RicohBleConnectOptions {
  uint32_t timeoutMs = 0;
  uint32_t securityWaitMs = 0;
  uint32_t preConnectDelayMs = 0;
  bool exchangeMtu = true;
  RicohProtocolGeneration protocolHint = RicohProtocolGeneration::Unknown;
};

enum class RicohPasskeyPollAction : uint8_t {
  Start,
  Poll,
  Cancel,
};

struct RicohBleSecurityState {
  bool bonded = false;
  bool encrypted = false;
  bool authenticated = false;
  uint8_t keySize = 0;
};

enum class RicohCameraPowerState {
  Unknown,
  On,
  OffOrShuttingDown,
};

class RicohBleClient {
public:
  using ServiceCallback = bool (*)();
  // Returns a completed six-digit code for Poll, -1 while pending, or -2 when
  // the local entry UI canceled/timed out. Start and Cancel reset UI state.
  using PasskeyPoller = int32_t (*)(RicohPasskeyPollAction action);

  void begin();
  void setSecurityProfile(RicohSecurityProfileId profile);
  bool switchSecurityProfile(RicohSecurityProfileId profile);
  RicohSecurityProfileId securityProfileId() const { return _securityProfile; }
  void setBindingState(CameraBindingState state);
  CameraBindingState bindingState() const { return _bindingState; }
  bool consumeBondInvalidRequest();
  void setServiceCallback(ServiceCallback callback);
  void setPasskeyPoller(PasskeyPoller poller);
  RicohBleDeviceInfo scanForCamera(const String& preferredAddress, const String& preferredName, uint32_t scanSeconds);
  bool connect(const RicohBleDeviceInfo& info, uint32_t timeoutMs);
  bool connect(const RicohBleDeviceInfo& info, const RicohBleConnectOptions& options);
  bool isBonded(const RicohBleDeviceInfo& info);
  bool isConnected() const;
  bool shutterReady() const;
  bool shoot(bool autofocus = true);
  bool openWifi();
  bool readPowerState(RicohCameraPowerState& state);
  bool readOperationMode(RicohCameraOperationMode& mode);
  bool enablePowerStateNotify();
  bool consumePowerOffNotification();
  bool waitForWifiCredentials(RicohBleWifiCredentials& credentials, uint32_t timeoutMs);
  void disconnect();
  int consumeDisconnectReason();
  void clearDisconnectReason();
  bool deleteAllBonds();
  void resetStack(bool clearObjects = false);
  bool lastFailureWasResourceExhausted() const;
  const CameraProtocolProfile& protocolProfile() const;
  RicohBleSecurityState securityState() const;
  String connectedIdentityAddress() const { return _connectedIdentityAddress; }
  uint8_t connectedIdentityAddressType() const { return _connectedIdentityAddressType; }
  bool connectedIdentityKnown() const { return _connectedIdentityKnown; }

  String statusText() const;
  const String& lastError() const;

private:
  bool _begun = false;
  bool _connected = false;
  bool _lastFailureResourceExhausted = false;
  RicohSecurityProfileId _securityProfile = RicohSecurityProfileId::Unknown;
  CameraBindingState _bindingState = CameraBindingState::Unpaired;
  String _lastError;
  String _connectedIdentityAddress;
  uint8_t _connectedIdentityAddressType = 0;
  bool _connectedIdentityKnown = false;
  void* _client = nullptr;
  RicohBleProtocolRouter _protocolRouter;
  RicohCameraOperationMode _lastOperationMode = RicohCameraOperationMode::Unknown;
  bool _lastOperationModeValid = false;
};
