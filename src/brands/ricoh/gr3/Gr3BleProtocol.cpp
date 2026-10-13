#include "Gr3BleProtocol.h"

RicohProtocolGeneration Gr3BleProtocol::generation() const {
  return RicohProtocolGeneration::Gr3Family;
}

RicohSecurityProfileId Gr3BleProtocol::securityProfile() const {
  return RicohSecurityProfileId::Gr3Passkey;
}

const CameraProtocolProfile& Gr3BleProtocol::profile() const {
  return cameraProtocolProfile(generation());
}

RicohWifiActivationTransport Gr3BleProtocol::wifiActivationTransport() const {
  return RicohWifiActivationTransport::NetworkTypeUuid;
}

RicohPowerTransport Gr3BleProtocol::powerTransport() const {
  return RicohPowerTransport::CameraPowerUuid;
}

RicohPowerNotifyTransport Gr3BleProtocol::powerNotifyTransport() const {
  return RicohPowerNotifyTransport::CameraPowerUuid;
}

bool Gr3BleProtocol::triggerPairingWithProtectedRead() const {
  return true;
}

bool Gr3BleProtocol::requireAuthenticatedBond() const {
  return true;
}

bool Gr3BleProtocol::rediscoverServicesAfterSecurity() const {
  return true;
}

bool Gr3BleProtocol::decodePowerState(uint8_t value, bool& isOn, bool& isOff) const {
  isOn = value == 0x01;
  isOff = value == 0x00 || value == 0x02;
  return isOn || isOff;
}
