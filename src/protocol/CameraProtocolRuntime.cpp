#include "CameraProtocolRuntime.h"

namespace rvf {
namespace {

bool present(const char* value) {
    return value != nullptr && value[0] != '\0';
}

}  // namespace

bool isBlePairingModeSupported(BlePairingMode mode) {
    return mode == BlePairingMode::ExistingDefault;
}

bool canChangeCameraModelDuringBleActivity(bool connected,
                                           bool scanning,
                                           bool connecting) {
    return !connected && !scanning && !connecting;
}

bool canOpenWifiWithProfile(const CameraProtocolProfile& profile) {
    return profile.capabilities.supportsWifiLiveView &&
           profile.handles.wlanEnable != 0;
}

bool canReadWifiCredentialsWithProfile(const CameraProtocolProfile& profile) {
    return profile.capabilities.supportsWifiLiveView &&
           profile.handles.wlanSsid != 0 &&
           profile.handles.wlanPassphrase != 0;
}

bool canEnablePowerStateNotifyWithProfile(const CameraProtocolProfile& profile) {
    return profile.capabilities.hasPowerStateNotify &&
           profile.handles.powerState != 0 &&
           profile.handles.powerStateCccd != 0;
}

bool canShootWithProfile(const CameraProtocolProfile& profile) {
    return profile.capabilities.supportsBleShutter &&
           present(profile.uuids.shootingService) &&
           present(profile.uuids.shootingFlavor) &&
           present(profile.uuids.operationRequest);
}

}  // namespace rvf
