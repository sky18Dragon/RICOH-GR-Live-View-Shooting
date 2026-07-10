#include "CameraModel.h"

namespace rvf {

const char* cameraModelName(CameraModel model) {
    switch (model) {
        case CameraModel::RicohGr4Hdf:
            return "RICOH_GR_IV_HDF";
        case CameraModel::RicohGr3x:
            return "RICOH_GR_IIIx";
        case CameraModel::Unknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

}  // namespace rvf
