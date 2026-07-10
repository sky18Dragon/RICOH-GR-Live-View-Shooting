#include "CameraProtocolRegistry.h"

#include "profiles/Gr4ProtocolProfile.h"

namespace rvf {

const CameraProtocolProfile* CameraProtocolRegistry::find(CameraModel model) {
    switch (model) {
        case CameraModel::RicohGr4Hdf:
            return &gr4ProtocolProfile();
        case CameraModel::Unknown:
        case CameraModel::RicohGr3x:
            return nullptr;
    }
    return nullptr;
}

const CameraProtocolProfile& CameraProtocolRegistry::defaultProfile() {
    return gr4ProtocolProfile();
}

}  // namespace rvf
