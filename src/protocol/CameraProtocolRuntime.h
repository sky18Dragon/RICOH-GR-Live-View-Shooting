#pragma once

#include "CameraProtocolProfile.h"

namespace rvf {

bool isBlePairingModeSupported(BlePairingMode mode);
bool canChangeCameraModelDuringBleActivity(bool connected,
                                           bool scanning,
                                           bool connecting);
bool canOpenWifiWithProfile(const CameraProtocolProfile& profile);
bool canReadWifiCredentialsWithProfile(const CameraProtocolProfile& profile);
bool canEnablePowerStateNotifyWithProfile(const CameraProtocolProfile& profile);
bool canShootWithProfile(const CameraProtocolProfile& profile);

}  // namespace rvf
