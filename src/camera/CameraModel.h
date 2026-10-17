#pragma once

#include <cstdint>

namespace rvf {

// Brand-defined model id. Unique across the models compiled into one
// firmware; the Ricoh driver uses RicohProtocolGeneration values.
using CameraModelId = uint8_t;

// One entry of the model list the user picks from before pairing.
struct CameraModelDescriptor {
    CameraModelId id;
    const char* label;      // option text in the pairing guide
    const char* setupHint;  // what to do on the camera before confirming
};

}  // namespace rvf
