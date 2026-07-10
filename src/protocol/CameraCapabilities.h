#pragma once

namespace rvf {

struct CameraCapabilities {
    bool hasWlanSecurity;
    bool hasWlanFrequency;
    bool hasWlanChannel;
    bool hasWlanBssid;
    bool hasPowerStateNotify;
    bool supportsBleShutter;
    bool supportsWifiLiveView;
};

}  // namespace rvf
