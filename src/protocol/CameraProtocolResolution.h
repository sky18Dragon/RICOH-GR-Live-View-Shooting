#pragma once

#include "CameraModel.h"
#include "CameraProtocolProfile.h"

namespace rvf {

struct CameraProtocolResolution {
    CameraModel requestedModel = CameraModel::Unknown;
    CameraModel resolvedModel = CameraModel::Unknown;
    const CameraProtocolProfile* profile = nullptr;
    bool usedFallback = false;
};

}  // namespace rvf
