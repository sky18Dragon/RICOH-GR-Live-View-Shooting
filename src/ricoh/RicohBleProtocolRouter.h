#pragma once

#include "IRicohBleProtocol.h"
#include "gr3/Gr3BleProtocol.h"
#include "gr4/Gr4LegacyBleProtocol.h"

class RicohBleProtocolRouter {
public:
  bool select(RicohProtocolGeneration generation);
  void clear();

  bool hasProtocol() const;
  RicohProtocolGeneration generation() const;
  RicohSecurityProfileId securityProfile() const;
  const CameraProtocolProfile& profile() const;
  const IRicohBleProtocol* protocol() const;

private:
  const IRicohBleProtocol* _protocol = nullptr;
  Gr3BleProtocol _gr3;
  Gr4LegacyBleProtocol _gr4Legacy;
};
