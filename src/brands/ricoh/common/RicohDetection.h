#pragma once

#include <cstdint>

#include "../../../camera_protocol_profile.h"

// How well a read-only GATT discovery matches one model.
enum class RicohEvidenceMatch : uint8_t {
  None,
  Partial,  // some of the model's markers, not all
  Full,     // every marker the model requires
};
