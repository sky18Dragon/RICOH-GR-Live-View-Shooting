#include "CameraProfileMigration.h"

namespace rvf {

CameraModel cameraModelFromStoredProfile(bool hasCameraModelKey,
                                         uint32_t storedProfileVersion,
                                         uint8_t rawCameraModel) {
    if (!hasCameraModelKey || storedProfileVersion != kCameraProfileVersion) {
        return CameraModel::Unknown;
    }
    return cameraModelFromRaw(rawCameraModel);
}

}  // namespace rvf
