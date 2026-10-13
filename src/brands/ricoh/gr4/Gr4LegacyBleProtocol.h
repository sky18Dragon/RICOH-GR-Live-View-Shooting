#pragma once

#include "../common/IRicohBleProtocol.h"

class Gr4LegacyBleProtocol final : public IRicohBleProtocol {
public:
  RicohProtocolGeneration generation() const override;
  RicohSecurityProfileId securityProfile() const override;
  const CameraProtocolProfile& profile() const override;
  bool triggerPairingWithProtectedRead() const override;
  bool requireAuthenticatedBond() const override;
  bool rediscoverServicesAfterSecurity() const override;
  bool decodePowerState(uint8_t value, bool& isOn, bool& isOff) const override;

  bool openWifi(RicohGattSession& gatt, const RicohWifiGate& gate, std::string& error) const override;
  bool closeWifi(RicohGattSession& gatt, std::string& error) const override;
  bool readPowerValue(RicohGattSession& gatt, std::vector<uint8_t>& value, std::string& error) const override;
  bool enablePowerNotify(RicohGattSession& gatt, std::string& error) const override;
  bool requestCaptureOperationMode(RicohGattSession& gatt, std::string& error) const override;
  bool readWifiCredentials(RicohGattSession& gatt,
                           RicohWifiCredentials& credentials,
                           std::string& error) const override;
};
