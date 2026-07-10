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

CameraModel cameraModelFromRaw(uint8_t raw) {
    switch (raw) {
        case static_cast<uint8_t>(CameraModel::RicohGr4Hdf):
            return CameraModel::RicohGr4Hdf;
        case static_cast<uint8_t>(CameraModel::RicohGr3x):
            return CameraModel::RicohGr3x;
        case static_cast<uint8_t>(CameraModel::Unknown):
        default:
            return CameraModel::Unknown;
    }
}

}  // namespace rvf
