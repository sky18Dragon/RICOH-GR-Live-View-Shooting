#include "RicohBleProtocolRouter.h"

RicohProtocolGeneration detectRicohProtocol(const ProtocolDetectionEvidence& evidence) {
  return RicohBleProtocolRouter().detect(evidence);
}

// The most specific fully matching model wins, unless an even more specific
// model shows partial markers of itself -- then the discovery is ambiguous
// (e.g. GR IV, which also exposes the GR III UUIDs, seen half-way).
RicohProtocolGeneration RicohBleProtocolRouter::detect(const ProtocolDetectionEvidence& evidence) const {
  const IRicohBleProtocol* const models[] = {&_gr3, &_gr4Legacy};
  const IRicohBleProtocol* best = nullptr;
  for (const IRicohBleProtocol* model : models) {
    if (model->matchEvidence(evidence) == RicohEvidenceMatch::Full &&
        (best == nullptr || model->detectionPriority() > best->detectionPriority())) {
      best = model;
    }
  }
  if (best == nullptr) {
    return RicohProtocolGeneration::Unknown;
  }
  for (const IRicohBleProtocol* model : models) {
    if (model != best && model->detectionPriority() > best->detectionPriority() &&
        model->matchEvidence(evidence) != RicohEvidenceMatch::None) {
      return RicohProtocolGeneration::Unknown;
    }
  }
  return best->generation();
}

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
