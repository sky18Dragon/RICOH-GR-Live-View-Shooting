#include "CameraProfileMigration.h"

namespace rvf {

CameraModel cameraModelFromStoredProfile(bool hasCameraModelKey,
                                         uint32_t storedProfileVersion,
                                         uint8_t rawCameraModel) {
    if (!hasCameraModelKey || storedProfileVersion < kCameraProfileVersion) {
        return CameraModel::Unknown;
    }

    switch (rawCameraModel) {
        case 1:
            return CameraModel::RicohGr4Hdf;
        case 2:
            return CameraModel::RicohGr3x;
        default:
            return CameraModel::Unknown;
    }
}

}  // namespace rvf
