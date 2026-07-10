#pragma once

#include "CameraProtocolRegistry.h"

namespace rvf {

class CameraProtocolSelection {
public:
    bool select(CameraModel model) {
        if (CameraProtocolRegistry::find(model) == nullptr) {
            return false;
        }
        _model = model;
        return true;
    }

    const CameraProtocolProfile& protocol() const {
        const CameraProtocolProfile* profile = CameraProtocolRegistry::find(_model);
        return profile != nullptr ? *profile : CameraProtocolRegistry::defaultProfile();
    }

    CameraModel cameraModel() const {
        return _model;
    }

private:
    CameraModel _model = CameraModel::RicohGr4Hdf;
};

}  // namespace rvf
