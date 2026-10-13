#include "Gr4Detection.h"

// GR IV requires all six WLAN handles inside the WLAN service, the Power
// handle inside the Camera service and the known UUIDs at their expected
// handles. Any one of those fixed-handle markers alone is a partial match.
RicohEvidenceMatch gr4MatchEvidence(const ProtocolDetectionEvidence& evidence) {
  const bool full =
      evidence.hasCameraService &&
      evidence.hasOperationModeCharacteristic &&
      evidence.hasShootingService &&
      evidence.hasShootingFlavorCharacteristic &&
      evidence.hasOperationRequestCharacteristic &&
      evidence.hasControlService &&
      evidence.hasGr4PowerCharacteristicAtExpectedHandle &&
      evidence.gr4WlanHandlesInExpectedService &&
      evidence.gr4KnownWlanUuidHandleMapping &&
      evidence.gr4ExpectedWlanCharacteristicCount == 6;
  if (full) {
    return RicohEvidenceMatch::Full;
  }
  const bool partial =
      evidence.hasGr4PowerCharacteristicAtExpectedHandle ||
      evidence.gr4WlanHandlesInExpectedService ||
      evidence.gr4KnownWlanUuidHandleMapping ||
      evidence.gr4ExpectedWlanCharacteristicCount > 0;
  return partial ? RicohEvidenceMatch::Partial : RicohEvidenceMatch::None;
}

