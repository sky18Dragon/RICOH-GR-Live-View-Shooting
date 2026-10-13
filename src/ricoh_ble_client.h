#pragma once

#include <Arduino.h>

#include "ble_link_mode_policy.h"
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
  int8_t connectRetries = -1;
  // Callers may still set this; connect() currently always exchanges ATT_MTU
  // because GR 128-bit UUID discovery is too slow at the default MTU of 23.
  bool exchangeMtu = true;
  RicohProtocolGeneration protocolHint = RicohProtocolGeneration::Unknown;
  // When set, read-only discovery must identify this exact generation before
  // the formal security/pairing flow is allowed to continue.
  RicohProtocolGeneration expectedGeneration = RicohProtocolGeneration::Unknown;
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

  bool begin();
  bool setSecurityProfile(RicohSecurityProfileId profile);
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
  // startupModeProbe lets a GR III family camera that still reports
  // bleStartup decide for itself whether to accept the NetworkType write.
  bool openWifi(bool startupModeProbe = false);
  bool closeWifi();
  bool readPowerState(RicohCameraPowerState& state);
  bool readOperationMode(RicohCameraOperationMode& mode);
  // Writes OperationMode=capture only when OperationModeList offers it.
  bool requestCaptureOperationMode();
  bool enablePowerStateNotify();
  bool consumePowerOffNotification();
  // Subscribes BatteryLevel, ShotCount and CaptureStatus individually; true
  // when at least one is live. Runs once per connection after the link is
  // usable (authenticated encryption on GR III).
  bool enableCameraStatusNotify();
  bool statusNotifyAttempted() const { return _statusNotifyAttempted; }
  // Bumped by every ShotCount or CaptureStatus notification.
  uint32_t shotEventSequence() const;
  bool shotNotifyActive() const;
  // Last BatteryLevel byte, or -1 while no battery notify is subscribed.
  int batteryLevel() const;
  bool waitForWifiCredentials(RicohBleWifiCredentials& credentials, uint32_t timeoutMs);
  void disconnect();
  // Requests new connection parameters on the live link and makes them the
  // answer to any later peer update request. Completion is asynchronous;
  // readConnParams() reports what the controller actually negotiated.
  bool applyConnParams(const BleConnParams& params, const char* reason);
  // Current negotiated parameters (min == max == the live interval).
  bool readConnParams(BleConnParams& params) const;
  // Connection-parameter update requests received from the camera.
  uint32_t peerConnParamRequests() const;
  // Bumped on every established link. Lets callers notice a reconnect that
  // happened entirely inside one blocking recovery call.
  uint32_t connectionSerial() const { return _connectionSerial; }
  int consumeDisconnectReason();
  void clearDisconnectReason();
  bool deleteAllBonds();
  bool resetStack(bool clearObjects = false);
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
  // A failed host stop/object-clear leaves NimBLE ownership uncertain.  Only a
  // later successful resetStack() may clear this latch and permit operations.
  bool _stackRestartBlocked = false;
  bool _connected = false;
  uint32_t _connectionSerial = 0;
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
  bool _statusNotifyAttempted = false;
};
