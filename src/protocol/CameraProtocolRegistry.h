#pragma once

#include "CameraProtocolProfile.h"

namespace rvf {

class CameraProtocolRegistry {
public:
    static const CameraProtocolProfile* find(CameraModel model);
    static const CameraProtocolProfile& defaultProfile();
};

}  // namespace rvf
