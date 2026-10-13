#include "RicohBleProtocolRouter.h"

#include "../gr3/Gr3Detection.h"
#include "../gr4/Gr4Detection.h"

RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence) {
  return RicohBleProtocolRouter().detect(evidence);
}

namespace {

struct KnownModelDetector {
  RicohProtocolGeneration generation;
  uint8_t priority;
  RicohEvidenceMatch (*match)(const ProtocolDetectionEvidence&);
};

// Every Ricoh model with a BLE protocol, compiled in or not.
const KnownModelDetector kKnownModels[] = {
    {RicohProtocolGeneration::Gr3Family, kGr3DetectionPriority, gr3MatchEvidence},
    {RicohProtocolGeneration::Gr4Family, kGr4DetectionPriority, gr4MatchEvidence},
};

}  // namespace

// The most specific fully matching model wins, unless an even more specific
// model shows partial markers of itself -- then the discovery is ambiguous
// (e.g. GR IV, which also exposes the GR III UUIDs, seen half-way). A winner
// this firmware does not contain is reported as unknown, which blocks every
// BLE side effect.
RicohProtocolGeneration RicohBleProtocolRouter::detect(const ProtocolDetectionEvidence& evidence) const {
  const KnownModelDetector* best = nullptr;
  for (const KnownModelDetector& model : kKnownModels) {
    if (model.match(evidence) == RicohEvidenceMatch::Full &&
        (best == nullptr || model.priority > best->priority)) {
      best = &model;
    }
  }
  if (best == nullptr) {
    return RicohProtocolGeneration::Unknown;
  }
  for (const KnownModelDetector& model : kKnownModels) {
    if (&model != best && model.priority > best->priority &&
        model.match(evidence) != RicohEvidenceMatch::None) {
      return RicohProtocolGeneration::Unknown;
    }
  }
  RicohBleProtocolRouter compiled;
  return compiled.select(best->generation) ? best->generation : RicohProtocolGeneration::Unknown;
}

bool RicohBleProtocolRouter::select(RicohProtocolGeneration generation) {
  switch (generation) {
#if RVF_RICOH_GR3
    case RicohProtocolGeneration::Gr3Family:
      _protocol = &_gr3;
      return true;
#endif
#if RVF_RICOH_GR4
    case RicohProtocolGeneration::Gr4Family:
      _protocol = &_gr4Legacy;
      return true;
#endif
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
