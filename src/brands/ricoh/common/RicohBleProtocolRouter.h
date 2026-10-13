#pragma once

#include "../RicohModelSelection.h"
#include "IRicohBleProtocol.h"
#include "RicohDetection.h"
#if RVF_RICOH_GR3
#include "../gr3/Gr3BleProtocol.h"
#endif
#if RVF_RICOH_GR4
#include "../gr4/Gr4LegacyBleProtocol.h"
#endif

// detect() with this firmware's models.
RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence);

class RicohBleProtocolRouter {
public:
  // Classifies a read-only GATT discovery. Every known model's detection rule
  // takes part; only a model compiled into this firmware can be the answer.
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
#if RVF_RICOH_GR3
  Gr3BleProtocol _gr3;
#endif
#if RVF_RICOH_GR4
  Gr4LegacyBleProtocol _gr4Legacy;
#endif
};
