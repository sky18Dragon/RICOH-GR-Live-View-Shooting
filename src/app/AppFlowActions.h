#pragma once

#include <cstdint>

namespace rvf {

struct AppFlowActions {
    bool (*cameraSleepGuardBlocksFlow)(const char* reason) = nullptr;
    bool (*cameraSleepGuardActive)() = nullptr;
    void (*showCameraSleepGuardStatus)() = nullptr;

    bool (*isBleConnected)() = nullptr;
    bool (*isWifiConnected)() = nullptr;
    bool (*consumePowerOffNotification)(const char* reason) = nullptr;
    bool (*consumePowerOffDisconnect)(const char* reason) = nullptr;
    void (*disconnectWifi)() = nullptr;
    void (*clearBleDisconnectReason)() = nullptr;
    void (*disconnectWifiLiveViewToBleReady)(const char* reason) = nullptr;
    void (*disconnectAllTransportsToBleScan)(const char* reason) = nullptr;

    bool (*runBleDiscovery)() = nullptr;
    bool (*activateCameraWifiOverBle)() = nullptr;
    bool (*hasUsableCachedWifiCredentials)() = nullptr;
    bool (*connectCachedWifiFromProfile)() = nullptr;
    void (*onCachedWifiConnected)() = nullptr;
    void (*onCachedWifiConnectFailed)() = nullptr;
    bool (*readFreshWifiCredentials)() = nullptr;
    void (*applyFreshWifiCredentials)() = nullptr;
    bool (*connectFreshWifiFromProfile)() = nullptr;
    void (*onFreshWifiConnected)() = nullptr;

    bool (*fetchCameraProps)() = nullptr;
    void (*onHttpProbeSucceeded)() = nullptr;
    void (*onHttpProbeFailed)() = nullptr;
    void (*showStartingLiveView)() = nullptr;
    bool (*openLiveView)() = nullptr;
    void (*onLiveViewOpened)() = nullptr;
    void (*onLiveViewOpenFailed)() = nullptr;
    bool (*previewStreamRunning)() = nullptr;
    bool (*readAndProcessLiveViewFrame)() = nullptr;
    void (*logPreviewStats)() = nullptr;

    bool (*cameraRecoveryInProgress)() = nullptr;
    void (*setCameraRecoveryInProgress)(bool inProgress) = nullptr;
    bool (*reasonRequiresBleRescan)(const char* reason) = nullptr;
    void (*showRecoveryBleReadyRetry)(const char* reason) = nullptr;
    void (*showRecoveryBleScan)(const char* reason) = nullptr;
    void (*resetBleStackBeforeScanAfterLinkLoss)(const char* reason) = nullptr;
    void (*shortRecoveryDelay)() = nullptr;
    void (*onRecoveryGuardBlocked)() = nullptr;
    void (*onRecoveryFinished)(bool recovered) = nullptr;

    void (*requestManualCameraWake)(const char* source) = nullptr;
    bool (*shutterReady)() = nullptr;
    void (*showShutterBleNotReady)() = nullptr;
    bool (*shootAutofocus)() = nullptr;
    void (*onShutterOk)() = nullptr;
    void (*onShutterFailed)() = nullptr;
    bool (*previewKeptAfterShutterFailure)() = nullptr;

    void (*delayAndYield)(uint32_t delayMs) = nullptr;
    uint32_t (*lastFrameAt)() = nullptr;
    uint32_t (*lastLiveViewActivityAt)() = nullptr;
    uint32_t (*lastCameraRecoveryAt)() = nullptr;
    void (*setLastCameraRecoveryAt)(uint32_t timestampMs) = nullptr;
    bool liveviewEnabled = true;
    uint8_t wifiOpenAttempts = 0;
    uint32_t retryDelayMs = 0;
    uint32_t bleScanRetryIntervalMs = 0;
    uint32_t liveViewStallTimeoutMs = 0;
};

}  // namespace rvf
