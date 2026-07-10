#pragma once

#include <stdint.h>

#include "CameraModel.h"

namespace rvf {

constexpr uint32_t kCameraProfileVersion = 4;

CameraModel cameraModelFromStoredProfile(bool hasCameraModelKey,
                                         uint32_t storedProfileVersion,
                                         uint8_t rawCameraModel);

}  // namespace rvf
