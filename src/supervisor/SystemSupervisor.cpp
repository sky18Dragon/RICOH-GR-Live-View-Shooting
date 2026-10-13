#include "SystemSupervisor.h"

#include <limits.h>

namespace rvf {

namespace {

uint32_t elapsedSince(uint32_t nowMs, uint32_t timestampMs) {
    return (timestampMs - nowMs) < 0x80000000UL ? 0 : nowMs - timestampMs;
}

AppMessage makeMessage(AppEventType type, uint32_t nowMs, int code, const char* detail) {
    AppMessage message;
    message.type = type;
    message.timestampMs = nowMs;
    message.code = code;
    message.detail = detail;
    return message;
}

}  // namespace

void SystemSupervisor::begin(uint32_t nowMs) {
    _checkTask.reset(nowMs);
}

bool SystemSupervisor::check(uint32_t nowMs,
                             const SystemHealthSnapshot& snapshot,
                             AppMessage& outMessage) {
    if (!_checkTask.consumeIfDue(nowMs)) {
        return false;
    }

    outMessage = AppMessage{};
    if (snapshot.cameraSleepGuardActive || snapshot.cameraRecoveryInProgress) {
        return false;
    }

    if (snapshot.cameraPhase != CameraPhase::Previewing || !snapshot.liveviewEnabled) {
        return false;
    }

    if (!snapshot.bleConnected) {
        outMessage = makeMessage(AppEventType::BleDisconnected, nowMs, 0, "supervisor preview lost BLE");
        return true;
    }
    if (!snapshot.wifiConnected) {
        outMessage = makeMessage(AppEventType::WifiDisconnected, nowMs, 0, "supervisor preview lost WiFi");
        return true;
    }
    if (!snapshot.previewRunning) {
        outMessage = makeMessage(AppEventType::PreviewStopped, nowMs, 0, "supervisor preview closed");
        return true;
    }

    const uint32_t frameIdleMs = elapsedSince(nowMs, snapshot.lastFrameAt);
    const uint32_t streamIdleMs = elapsedSince(nowMs, snapshot.lastLiveViewActivityAt);
    const bool frameStalled = snapshot.liveViewStallTimeoutMs > 0 &&
                              frameIdleMs > snapshot.liveViewStallTimeoutMs;
    const bool streamStalled = snapshot.liveViewStallTimeoutMs > 0 &&
                               streamIdleMs > snapshot.liveViewStallTimeoutMs;
    if (frameStalled || streamStalled) {
        const uint32_t idleMs = frameStalled ? frameIdleMs : streamIdleMs;
        const int code = idleMs > static_cast<uint32_t>(INT_MAX) ? INT_MAX : static_cast<int>(idleMs);
        outMessage = makeMessage(AppEventType::PreviewTimeout,
                                 nowMs,
                                 code,
                                 frameStalled
                                   ? "supervisor preview frame idle"
                                   : "supervisor preview stream idle");
        return true;
    }

    return false;
}

}  // namespace rvf
