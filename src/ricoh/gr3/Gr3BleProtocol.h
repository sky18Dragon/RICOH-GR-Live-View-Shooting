#pragma once

#include "../IRicohBleProtocol.h"

class Gr3BleProtocol final : public IRicohBleProtocol {
public:
  RicohProtocolGeneration generation() const override;
  RicohSecurityProfileId securityProfile() const override;
  const CameraProtocolProfile& profile() const override;
  RicohWifiActivationTransport wifiActivationTransport() const override;
  RicohPowerTransport powerTransport() const override;
  RicohPowerNotifyTransport powerNotifyTransport() const override;
  bool triggerPairingWithProtectedRead() const override;
  bool requireAuthenticatedBond() const override;
  bool rediscoverServicesAfterSecurity() const override;
  bool decodePowerState(uint8_t value, bool& isOn, bool& isOff) const override;
};
