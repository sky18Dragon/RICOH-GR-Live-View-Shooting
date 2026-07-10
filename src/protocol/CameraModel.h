#pragma once

#include <stdint.h>

namespace rvf {

enum class CameraModel : uint8_t {
    Unknown = 0,
    RicohGr4Hdf = 1,
    RicohGr3x = 2,
};

const char* cameraModelName(CameraModel model);

}  // namespace rvf
