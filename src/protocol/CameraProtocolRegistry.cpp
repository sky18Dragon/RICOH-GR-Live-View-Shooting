#include "CameraProtocolRegistry.h"

#include "CameraProtocolValidator.h"
#include "profiles/Gr4ProtocolProfile.h"

namespace rvf {
namespace {

const CameraProtocolProfile* validatedGr4Profile() {
    const CameraProtocolProfile& profile = gr4ProtocolProfile();
    return validateCameraProtocolProfile(profile).valid ? &profile : nullptr;
}

}  // namespace

const CameraProtocolProfile* CameraProtocolRegistry::find(CameraModel model) {
    switch (model) {
        case CameraModel::RicohGr4Hdf:
            return validatedGr4Profile();
        case CameraModel::Unknown:
        case CameraModel::RicohGr3x:
            return nullptr;
    }
    return nullptr;
}

const CameraProtocolProfile& CameraProtocolRegistry::defaultProfile() {
    return gr4ProtocolProfile();
}

CameraProtocolResolution CameraProtocolRegistry::resolve(CameraModel requestedModel) {
    CameraProtocolResolution resolution;
    resolution.requestedModel = requestedModel;
    resolution.profile = find(requestedModel);
    if (resolution.profile != nullptr) {
        resolution.resolvedModel = resolution.profile->model;
        return resolution;
    }

    resolution.profile = &defaultProfile();
    resolution.resolvedModel = resolution.profile->model;
    resolution.usedFallback = true;
    return resolution;
}

size_t CameraProtocolRegistry::profileCount() {
    return validatedGr4Profile() != nullptr ? 1 : 0;
}

const CameraProtocolProfile& CameraProtocolRegistry::profileAt(size_t index) {
    return index == 0 ? gr4ProtocolProfile() : defaultProfile();
}

}  // namespace rvf
