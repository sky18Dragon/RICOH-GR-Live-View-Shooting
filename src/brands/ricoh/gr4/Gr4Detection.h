#pragma once

#include "../common/RicohDetection.h"

// Compiled into every firmware, even one without the GR4 protocol, so
// a camera of this model is never mistaken for another model.
RicohEvidenceMatch gr4MatchEvidence(const ProtocolDetectionEvidence& evidence);
constexpr uint8_t kGr4DetectionPriority = 2;
