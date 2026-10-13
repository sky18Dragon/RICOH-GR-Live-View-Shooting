#include "CameraRegistry.h"

#include "../brands/ricoh/RicohModels.h"

namespace rvf {

size_t cameraModelCount() {
    return kRicohModelCount;
}

const CameraModelDescriptor& cameraModelAt(size_t index) {
    return kRicohModels[index < kRicohModelCount ? index : kRicohDefaultModelIndex];
}

size_t defaultCameraModelIndex() {
    return kRicohDefaultModelIndex;
}

size_t cameraModelIndex(CameraModelId id) {
    for (size_t i = 0; i < kRicohModelCount; ++i) {
        if (kRicohModels[i].id == id) {
            return i;
        }
    }
    return kRicohDefaultModelIndex;
}

}  // namespace rvf
