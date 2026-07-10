#pragma once

#include <stdint.h>

#include "CameraCapabilities.h"
#include "CameraModel.h"

namespace rvf {

enum class BlePairingMode : uint8_t {
    ExistingDefault = 0,
    PasskeyEntry = 1,
};

struct CameraGattHandles {
    uint16_t wlanEnable;
    uint16_t wlanSsid;
    uint16_t wlanPassphrase;
    uint16_t wlanSecurity;
    uint16_t wlanFrequency;
    uint16_t wlanChannel;
    uint16_t wlanBssid;

    uint16_t powerState;
    uint16_t powerStateCccd;
};

struct CameraGattUuids {
    const char* infoService;
    const char* cameraService;
    const char* operationMode;
    const char* shootingService;
    const char* shootingFlavor;
    const char* operationRequest;
    const char* controlService;
};

struct CameraProtocolProfile {
    CameraModel model;
    const char* modelName;

    CameraGattHandles handles;
    CameraGattUuids uuids;
    CameraCapabilities capabilities;

    BlePairingMode pairingMode;

    uint8_t wlanEnableValue;
    uint8_t powerOnValue;
    uint8_t powerOffValue;
};

}  // namespace rvf
