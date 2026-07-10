#pragma once

#include <stddef.h>

#include "CameraProtocolProfile.h"
#include "CameraProtocolResolution.h"

namespace rvf {

class CameraProtocolRegistry {
public:
    static const CameraProtocolProfile* find(CameraModel model);
    static const CameraProtocolProfile& defaultProfile();
    static CameraProtocolResolution resolve(CameraModel requestedModel);
    static size_t profileCount();
    static const CameraProtocolProfile& profileAt(size_t index);
};

}  // namespace rvf
