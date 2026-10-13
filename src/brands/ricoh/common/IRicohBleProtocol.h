#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../../camera_protocol_profile.h"
#include "RicohGattSession.h"

// What the camera last reported about its operation mode, for models that
// gate Wi-Fi activation on it.
struct RicohWifiGate {
  bool startupModeProbe = false;
  RicohCameraOperationMode operationMode = RicohCameraOperationMode::Unknown;
  bool operationModeValid = false;
};

// One Ricoh model's BLE protocol: how it pairs and how it drives the camera
// over a connected link. Operations write `error` on failure and clear it on
// success; RicohBleClient keeps the connection and the model-neutral checks.
class IRicohBleProtocol {
public:
  virtual ~IRicohBleProtocol() = default;

  virtual RicohProtocolGeneration generation() const = 0;
  virtual RicohSecurityProfileId securityProfile() const = 0;
  virtual const CameraProtocolProfile& profile() const = 0;
  virtual bool triggerPairingWithProtectedRead() const = 0;
  virtual bool requireAuthenticatedBond() const = 0;
  virtual bool rediscoverServicesAfterSecurity() const = 0;
  virtual bool decodePowerState(uint8_t value, bool& isOn, bool& isOff) const = 0;

  virtual bool openWifi(RicohGattSession& gatt, const RicohWifiGate& gate, std::string& error) const = 0;
  virtual bool closeWifi(RicohGattSession& gatt, std::string& error) const = 0;
  // Raw power-state bytes; decodePowerState() interprets the first one.
  virtual bool readPowerValue(RicohGattSession& gatt, std::vector<uint8_t>& value, std::string& error) const = 0;
  virtual bool enablePowerNotify(RicohGattSession& gatt, std::string& error) const = 0;
  virtual bool requestCaptureOperationMode(RicohGattSession& gatt, std::string& error) const = 0;
};
