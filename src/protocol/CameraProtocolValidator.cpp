#include "CameraProtocolValidator.h"

namespace rvf {
namespace {

bool isMissing(const char* value) {
    return value == nullptr || value[0] == '\0';
}

CameraProtocolValidationResult failure(CameraProtocolValidationError error,
                                       const char* field) {
    CameraProtocolValidationResult result;
    result.error = error;
    result.field = field;
    return result;
}

}  // namespace

CameraProtocolValidationResult validateCameraProtocolProfile(
    const CameraProtocolProfile& profile) {
    if (profile.model == CameraModel::Unknown) {
        return failure(CameraProtocolValidationError::UnknownModel, "model");
    }
    if (isMissing(profile.modelName)) {
        return failure(CameraProtocolValidationError::MissingModelName, "modelName");
    }
    if (isMissing(profile.uuids.infoService)) {
        return failure(CameraProtocolValidationError::MissingInfoServiceUuid, "infoService");
    }
    if (isMissing(profile.uuids.cameraService)) {
        return failure(CameraProtocolValidationError::MissingCameraServiceUuid, "cameraService");
    }
    if (isMissing(profile.uuids.operationMode)) {
        return failure(CameraProtocolValidationError::MissingOperationModeUuid, "operationMode");
    }
    if (profile.capabilities.supportsWifiLiveView) {
        if (profile.handles.wlanEnable == 0) {
            return failure(CameraProtocolValidationError::MissingWlanEnableHandle, "wlanEnable");
        }
        if (profile.handles.wlanSsid == 0) {
            return failure(CameraProtocolValidationError::MissingWlanSsidHandle, "wlanSsid");
        }
        if (profile.handles.wlanPassphrase == 0) {
            return failure(CameraProtocolValidationError::MissingWlanPassphraseHandle, "wlanPassphrase");
        }
    }
    if (profile.capabilities.hasWlanSecurity && profile.handles.wlanSecurity == 0) {
        return failure(CameraProtocolValidationError::MissingWlanSecurityHandle, "wlanSecurity");
    }
    if (profile.capabilities.hasWlanFrequency && profile.handles.wlanFrequency == 0) {
        return failure(CameraProtocolValidationError::MissingWlanFrequencyHandle, "wlanFrequency");
    }
    if (profile.capabilities.hasWlanChannel && profile.handles.wlanChannel == 0) {
        return failure(CameraProtocolValidationError::MissingWlanChannelHandle, "wlanChannel");
    }
    if (profile.capabilities.hasWlanBssid && profile.handles.wlanBssid == 0) {
        return failure(CameraProtocolValidationError::MissingWlanBssidHandle, "wlanBssid");
    }
    if (profile.capabilities.hasPowerStateNotify) {
        if (profile.handles.powerState == 0) {
            return failure(CameraProtocolValidationError::MissingPowerStateHandle, "powerState");
        }
        if (profile.handles.powerStateCccd == 0) {
            return failure(CameraProtocolValidationError::MissingPowerStateCccdHandle, "powerStateCccd");
        }
    }
    if (profile.capabilities.supportsBleShutter) {
        if (isMissing(profile.uuids.shootingService)) {
            return failure(CameraProtocolValidationError::MissingShootingServiceUuid, "shootingService");
        }
        if (isMissing(profile.uuids.shootingFlavor)) {
            return failure(CameraProtocolValidationError::MissingShootingFlavorUuid, "shootingFlavor");
        }
        if (isMissing(profile.uuids.operationRequest)) {
            return failure(CameraProtocolValidationError::MissingOperationRequestUuid, "operationRequest");
        }
    }

    CameraProtocolValidationResult result;
    result.valid = true;
    return result;
}

}  // namespace rvf
