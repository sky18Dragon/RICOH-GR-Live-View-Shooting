#include "Gr3Detection.h"

// A GR III family camera needs a complete, coherent read-only profile: the
// WLAN UUIDs are shared with GR IV, so an interrupted discovery must never
// classify as GR III. GR IV's own fixed-handle markers veto this match (it is
// the more specific model).
RicohEvidenceMatch gr3MatchEvidence(const ProtocolDetectionEvidence& evidence) {
  const bool full =
      evidence.gattDiscoveryComplete &&
      evidence.hasGr3WlanService &&
      evidence.hasGr3NetworkTypeCharacteristic &&
      evidence.hasGr3SsidCharacteristic &&
      evidence.hasGr3PassphraseCharacteristic &&
      evidence.hasCameraService &&
      evidence.hasOperationModeCharacteristic &&
      evidence.hasShootingService &&
      evidence.hasShootingFlavorCharacteristic &&
      evidence.hasOperationRequestCharacteristic &&
      evidence.hasControlService;
  return full ? RicohEvidenceMatch::Full : RicohEvidenceMatch::None;
}

