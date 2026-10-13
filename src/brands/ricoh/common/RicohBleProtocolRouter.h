#pragma once

#include "IRicohBleProtocol.h"
#include "../gr3/Gr3BleProtocol.h"
#include "../gr4/Gr4LegacyBleProtocol.h"

// detect() over every compiled-in model.
RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence);

class RicohBleProtocolRouter {
public:
  // Classifies a read-only GATT discovery across the compiled-in models.
  RicohProtocolGeneration detect(const ProtocolDetectionEvidence& evidence) const;

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
