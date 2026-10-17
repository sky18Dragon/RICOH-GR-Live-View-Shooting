#pragma once

#include "../core/AppMessage.h"
#include "../core/PeriodicTask.h"
#include "../camera/CameraPhase.h"

namespace rvf {

struct SystemHealthSnapshot {
    CameraPhase cameraPhase = CameraPhase::Booting;
    bool bleConnected = false;
    bool wifiConnected = false;
    bool previewRunning = false;
    bool liveviewEnabled = true;
    bool cameraSleepGuardActive = false;
    bool cameraRecoveryInProgress = false;
    uint32_t lastFrameAt = 0;
    uint32_t lastLiveViewActivityAt = 0;
    uint32_t liveViewStallTimeoutMs = 0;
};

class SystemSupervisor {
public:
    void begin(uint32_t nowMs);
    bool check(uint32_t nowMs, const SystemHealthSnapshot& snapshot, AppMessage& outMessage);
private:
    PeriodicTask _checkTask{1000};
};

}  // namespace rvf
