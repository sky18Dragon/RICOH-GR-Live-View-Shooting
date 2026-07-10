#include "CameraDiscoveryRegistry.h"

#include "CameraProtocolRegistry.h"

namespace rvf {
namespace {

constexpr size_t kDiscoveryServiceCountPerProfile = 4;

bool present(const char* value) {
    return value != nullptr && value[0] != '\0';
}

const char* discoveryServiceAt(const CameraProtocolProfile& profile,
                               size_t index) {
    switch (index) {
        case 0:
            return profile.uuids.infoService;
        case 1:
            return profile.uuids.cameraService;
        case 2:
            return profile.uuids.shootingService;
        case 3:
            return profile.uuids.controlService;
        default:
            return nullptr;
    }
}

}  // namespace

size_t CameraDiscoveryRegistry::serviceUuidCount() {
    size_t count = 0;
    for (size_t profileIndex = 0;
         profileIndex < CameraProtocolRegistry::profileCount();
         ++profileIndex) {
        const CameraProtocolProfile& profile =
            CameraProtocolRegistry::profileAt(profileIndex);
        for (size_t serviceIndex = 0;
             serviceIndex < kDiscoveryServiceCountPerProfile;
             ++serviceIndex) {
            if (present(discoveryServiceAt(profile, serviceIndex))) {
                ++count;
            }
        }
    }
    return count;
}

const char* CameraDiscoveryRegistry::serviceUuidAt(size_t index) {
    size_t currentIndex = 0;
    for (size_t profileIndex = 0;
         profileIndex < CameraProtocolRegistry::profileCount();
         ++profileIndex) {
        const CameraProtocolProfile& profile =
            CameraProtocolRegistry::profileAt(profileIndex);
        for (size_t serviceIndex = 0;
             serviceIndex < kDiscoveryServiceCountPerProfile;
             ++serviceIndex) {
            const char* uuid = discoveryServiceAt(profile, serviceIndex);
            if (!present(uuid)) {
                continue;
            }
            if (currentIndex == index) {
                return uuid;
            }
            ++currentIndex;
        }
    }
    return nullptr;
}

}  // namespace rvf
