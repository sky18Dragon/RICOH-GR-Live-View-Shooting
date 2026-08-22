#pragma once

#include "../camera_protocol_profile.h"

enum class RicohWifiActivationTransport : uint8_t {
  None,
  FixedHandle,
  NetworkTypeUuid,
};

enum class RicohPowerTransport : uint8_t {
  None,
  FixedHandle,
  CameraPowerUuid,
};

enum class RicohPowerNotifyTransport : uint8_t {
  None,
  FixedCccdHandle,
  CameraPowerUuid,
};

class IRicohBleProtocol {
public:
  virtual ~IRicohBleProtocol() = default;

  virtual RicohProtocolGeneration generation() const = 0;
  virtual RicohSecurityProfileId securityProfile() const = 0;
  virtual const CameraProtocolProfile& profile() const = 0;
  virtual RicohWifiActivationTransport wifiActivationTransport() const = 0;
  virtual RicohPowerTransport powerTransport() const = 0;
  virtual RicohPowerNotifyTransport powerNotifyTransport() const = 0;
  virtual bool triggerPairingWithProtectedRead() const = 0;
  virtual bool requireAuthenticatedBond() const = 0;
  virtual bool rediscoverServicesAfterSecurity() const = 0;
  virtual bool decodePowerState(uint8_t value, bool& isOn, bool& isOff) const = 0;
};
