#pragma once

#include <stdint.h>

#include "CameraProtocolProfile.h"

namespace rvf {

enum class CameraProtocolValidationError : uint8_t {
    None,
    UnknownModel,
    MissingModelName,
    MissingInfoServiceUuid,
    MissingCameraServiceUuid,
    MissingOperationModeUuid,
    MissingWlanEnableHandle,
    MissingWlanSsidHandle,
    MissingWlanPassphraseHandle,
    MissingWlanSecurityHandle,
    MissingWlanFrequencyHandle,
    MissingWlanChannelHandle,
    MissingWlanBssidHandle,
    MissingPowerStateHandle,
    MissingPowerStateCccdHandle,
    MissingShootingServiceUuid,
    MissingShootingFlavorUuid,
    MissingOperationRequestUuid,
};

struct CameraProtocolValidationResult {
    bool valid = false;
    CameraProtocolValidationError error = CameraProtocolValidationError::None;
    const char* field = nullptr;
};

CameraProtocolValidationResult validateCameraProtocolProfile(
    const CameraProtocolProfile& profile);

}  // namespace rvf
