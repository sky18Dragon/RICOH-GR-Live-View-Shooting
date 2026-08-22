#include "Gr4LegacyBleProtocol.h"

RicohProtocolGeneration Gr4LegacyBleProtocol::generation() const {
  return RicohProtocolGeneration::Gr4Family;
}

RicohSecurityProfileId Gr4LegacyBleProtocol::securityProfile() const {
  return RicohSecurityProfileId::Gr4Legacy;
}

const CameraProtocolProfile& Gr4LegacyBleProtocol::profile() const {
  return cameraProtocolProfile(generation());
}

RicohWifiActivationTransport Gr4LegacyBleProtocol::wifiActivationTransport() const {
  return RicohWifiActivationTransport::FixedHandle;
}

RicohPowerTransport Gr4LegacyBleProtocol::powerTransport() const {
  return RicohPowerTransport::FixedHandle;
}

RicohPowerNotifyTransport Gr4LegacyBleProtocol::powerNotifyTransport() const {
  return RicohPowerNotifyTransport::FixedCccdHandle;
}

bool Gr4LegacyBleProtocol::triggerPairingWithProtectedRead() const {
  return false;
}

bool Gr4LegacyBleProtocol::requireAuthenticatedBond() const {
  return false;
}

bool Gr4LegacyBleProtocol::rediscoverServicesAfterSecurity() const {
  return false;
}

bool Gr4LegacyBleProtocol::decodePowerState(uint8_t value, bool& isOn, bool& isOff) const {
  isOn = value == 0x01;
  isOff = value == 0x00;
  return isOn || isOff;
}
