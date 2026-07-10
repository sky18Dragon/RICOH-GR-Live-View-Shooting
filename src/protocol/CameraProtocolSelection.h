#pragma once

#include "CameraProtocolRegistry.h"

namespace rvf {

class CameraProtocolSelection {
public:
    void setProtocol(const CameraProtocolProfile& protocol) {
        _protocol = &protocol;
    }

    const CameraProtocolProfile& protocol() const {
        return _protocol != nullptr ? *_protocol : CameraProtocolRegistry::defaultProfile();
    }

    CameraModel cameraModel() const {
        return protocol().model;
    }

private:
    const CameraProtocolProfile* _protocol = nullptr;
};

}  // namespace rvf
