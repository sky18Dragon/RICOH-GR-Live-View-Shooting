#include "RicohBleProtocolRouter.h"

bool RicohBleProtocolRouter::select(RicohProtocolGeneration generation) {
  switch (generation) {
    case RicohProtocolGeneration::Gr3Family:
      _protocol = &_gr3;
      return true;
    case RicohProtocolGeneration::Gr4Family:
      _protocol = &_gr4Legacy;
      return true;
    default:
      clear();
      return false;
  }
}

void RicohBleProtocolRouter::clear() {
  _protocol = nullptr;
}

bool RicohBleProtocolRouter::hasProtocol() const {
  return _protocol != nullptr;
}

RicohProtocolGeneration RicohBleProtocolRouter::generation() const {
  return _protocol == nullptr ? RicohProtocolGeneration::Unknown : _protocol->generation();
}

RicohSecurityProfileId RicohBleProtocolRouter::securityProfile() const {
  return _protocol == nullptr ? RicohSecurityProfileId::Unknown : _protocol->securityProfile();
}

const CameraProtocolProfile& RicohBleProtocolRouter::profile() const {
  return cameraProtocolProfile(generation());
}

const IRicohBleProtocol* RicohBleProtocolRouter::protocol() const {
  return _protocol;
}
