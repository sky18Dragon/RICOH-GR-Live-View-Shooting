#pragma once

#include <Arduino.h>

#include "camera_protocol_profile.h"

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
  bool hasGr3WlanService = false;
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
};

enum class RicohCameraPowerState {
  Unknown,
  On,
  OffOrShuttingDown,
};

enum class RicohCameraOperationMode {
  Unknown,
  Capture,
  Playback,
  BleStartup,
  Other,
  PowerOffTransfer,
};

// True when the advertisement carries any Ricoh identity marker (name or a
// known service UUID). Shared by scan scoring and main's candidate gating.
bool hasRicohIdentitySignal(const RicohBleDeviceInfo& info);

class RicohBleClient {
public:
  using ServiceCallback = bool (*)();
  // Invoked once (from the connect task) when a GR III pairing starts waiting
  // for the camera's six-digit code, so the UI can tell the user.
  using PasskeyPromptCallback = void (*)();

  void begin();
  void setServiceCallback(ServiceCallback callback);
  void setPasskeyPromptCallback(PasskeyPromptCallback callback);
  // GR III passkey entry without a serial console: while a pairing waits for
  // the camera's six-digit code, passkeyEntryPending() is true and the code
  // can be delivered via submitPasskey() (e.g. from an on-device button UI).
  static bool passkeyEntryPending();
  static void submitPasskey(uint32_t passkey);
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

  String statusText() const;
  const String& lastError() const;

private:
  bool prepareShutter();
  void resetShutterCache();

  bool _begun = false;
  bool _connected = false;
  bool _shutterPrepared = false;
  bool _lastFailureResourceExhausted = false;
  RicohProtocolGeneration _protocolGeneration = RicohProtocolGeneration::Unknown;
  String _lastError;
  void* _client = nullptr;
  void* _shootingFlavor = nullptr;
  void* _operationRequest = nullptr;
};
