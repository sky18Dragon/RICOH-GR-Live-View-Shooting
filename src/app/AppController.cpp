#include "AppController.h"

#if !defined(RVF_NATIVE_BUILD)
#include <Arduino.h>
#endif

namespace rvf {

namespace {

uint32_t controllerMillis() {
#if defined(RVF_NATIVE_BUILD)
    return 1;
#else
    return millis();
#endif
}

}  // namespace

uint32_t elapsedSince(uint32_t nowMs, uint32_t timestampMs) {
    return (timestampMs - nowMs) < 0x80000000UL ? 0 : nowMs - timestampMs;
}

const char* appStateName(AppState state) {
    switch (state) {
        case AppState::Booting:
            return "BOOTING";
        case AppState::Idle:
            return "IDLE";
        case AppState::BleScan:
            return "BLE_SCAN";
        case AppState::ScanningCamera:
            return "SCANNING_CAMERA";
        case AppState::CameraSleepGuard:
            return "CAMERA_SLEEP_GUARD";
        case AppState::CameraPowerOff:
            return "CAMERA_POWER_OFF";
        case AppState::BleReady:
            return "BLE_READY";
        case AppState::WifiConnecting:
            return "WIFI_CONNECTING";
        case AppState::ConnectingWifi:
            return "CONNECTING_WIFI";
        case AppState::HttpProbe:
            return "HTTP_PROBE";
        case AppState::HttpProbing:
            return "HTTP_PROBING";
        case AppState::LiveViewRunning:
            return "LIVEVIEW_RUNNING";
        case AppState::PreviewRunning:
            return "PREVIEW_RUNNING";
        case AppState::ConnectingBle:
            return "CONNECTING_BLE";
        case AppState::CheckingCameraPower:
            return "CHECKING_CAMERA_POWER";
        case AppState::ActivatingWifi:
            return "ACTIVATING_WIFI";
        case AppState::WifiCredentialsReady:
            return "WIFI_CREDENTIALS_READY";
        case AppState::PreviewStarting:
            return "PREVIEW_STARTING";
        case AppState::PreviewStopped:
            return "PREVIEW_STOPPED";
        case AppState::Shooting:
            return "SHOOTING";
        case AppState::Disconnected:
            return "DISCONNECTED";
        case AppState::Error:
            return "ERROR";
    }
    return "UNKNOWN";
}

void AppController::begin(AppState initialState) {
    _state = initialState;
    _previewRequested = false;
    _previewRequestChanged = false;
}

AppTickPlan AppController::planTick(uint32_t nowMs) const {
    (void)nowMs;

    AppTickPlan plan;
    const bool previewActive = isPreviewActive();
    plan.monitorWifi = previewActive;
    plan.refreshProps = previewActive;
    plan.monitorLiveView = previewActive;
    return plan;
}

bool AppController::runCameraFlowOnce(const AppFlowActions& actions, uint32_t nowMs) {
    const uint32_t flowStartMs = nowMs != 0 ? nowMs : controllerMillis();
    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow("camera flow")) {
        return false;
    }

    transitionTo(AppState::BleScan, "enter BLE scan mode", flowStartMs);
    if (actions.runBleDiscovery == nullptr || !actions.runBleDiscovery()) {
        return false;
    }

    for (uint8_t attempt = 0; attempt < actions.wifiOpenAttempts; ++attempt) {
        if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
            return false;
        }

        if (connectWifiAfterBleReady(actions)) {
            if (!_previewRequested) {
                if (actions.disconnectWifi != nullptr) {
                    actions.disconnectWifi();
                }
                transitionTo(AppState::WifiCredentialsReady,
                             "portrait requested after WiFi connect",
                             controllerMillis());
                return true;
            }
            if (httpProbeCamera(actions) &&
                startLiveViewFromProbe(actions)) {
#if !defined(RVF_NATIVE_BUILD)
                Serial.printf("Flow: camera online total_ms=%lu\n",
                              static_cast<unsigned long>(controllerMillis() - flowStartMs));
#endif
                return true;
            }

            if (actions.isBleConnected != nullptr && actions.isBleConnected()) {
                return resumeFromBleReady(actions, "HTTP/LiveView unavailable");
            }

            if (actions.disconnectAllTransportsToBleScan != nullptr) {
                actions.disconnectAllTransportsToBleScan("HTTP/LiveView unavailable");
            }
            return false;
        }

        if (_state == AppState::WifiCredentialsReady) {
            return true;
        }

        if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
            return false;
        }
        if (actions.delayAndYield != nullptr) {
            actions.delayAndYield(actions.retryDelayMs);
        }
    }

    return false;
}

bool AppController::resumeFromBleReady(const AppFlowActions& actions, const char* reason) {
    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow("BLE_READY resume")) {
        return false;
    }

    if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
        if (actions.consumePowerOffDisconnect != nullptr &&
            actions.consumePowerOffDisconnect("BLE lost before BLE_READY resume")) {
            return false;
        }
        transitionTo(AppState::BleScan, "BLE lost before BLE_READY resume", controllerMillis());
        return false;
    }

    if (actions.disconnectWifiLiveViewToBleReady != nullptr) {
        actions.disconnectWifiLiveViewToBleReady(reason);
    }

    for (uint8_t attempt = 0; attempt < actions.wifiOpenAttempts; ++attempt) {
        if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
            return false;
        }

        if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
            if (actions.consumePowerOffDisconnect != nullptr &&
                actions.consumePowerOffDisconnect("BLE lost during WiFi retry")) {
                return false;
            }
            transitionTo(AppState::BleScan, "BLE lost during WiFi retry", controllerMillis());
            return false;
        }

        if (connectWifiAfterBleReady(actions)) {
            if (!_previewRequested) {
                if (actions.disconnectWifi != nullptr) {
                    actions.disconnectWifi();
                }
                transitionTo(AppState::WifiCredentialsReady,
                             "portrait requested after WiFi retry",
                             controllerMillis());
                return true;
            }
            if (httpProbeCamera(actions) &&
                startLiveViewFromProbe(actions)) {
                return true;
            }

            if (actions.disconnectWifiLiveViewToBleReady != nullptr) {
                actions.disconnectWifiLiveViewToBleReady("HTTP/LiveView retry from BLE_READY");
            }
        } else if (_state == AppState::WifiCredentialsReady) {
            return true;
        } else if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
            return false;
        }

        if (actions.delayAndYield != nullptr) {
            actions.delayAndYield(actions.retryDelayMs);
        }
    }

    return false;
}

bool AppController::resumeFromWifiCredentialsReady(const AppFlowActions& actions) {
    if (!_previewRequested) {
        return true;
    }
    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow("WiFi credentials resume")) {
        return false;
    }
    if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
        transitionTo(AppState::BleScan, "BLE lost before cached WiFi resume", controllerMillis());
        return false;
    }
    if (actions.hasUsableCachedWifiCredentials == nullptr ||
        !actions.hasUsableCachedWifiCredentials()) {
        transitionTo(AppState::BleReady, "cached WiFi params missing", controllerMillis());
        return false;
    }

    transitionTo(AppState::ConnectingWifi, "landscape resumes cached WiFi params", controllerMillis());
    if (actions.connectFreshWifiFromProfile == nullptr ||
        !actions.connectFreshWifiFromProfile()) {
        if (actions.disconnectWifi != nullptr) {
            actions.disconnectWifi();
        }
        transitionTo(_previewRequested ? AppState::BleReady : AppState::WifiCredentialsReady,
                     _previewRequested ? "WiFi resume failed" : "portrait cancelled WiFi resume",
                     controllerMillis());
        return false;
    }
    if (actions.onFreshWifiConnected != nullptr) {
        actions.onFreshWifiConnected();
    }
    if (httpProbeCamera(actions) && startLiveViewFromProbe(actions)) {
        return true;
    }

    if (actions.disconnectWifi != nullptr) {
        actions.disconnectWifi();
    }
    if (actions.isBleConnected != nullptr && actions.isBleConnected()) {
        transitionTo(AppState::WifiCredentialsReady,
                     "HTTP/LiveView failed after posture resume",
                     controllerMillis());
    } else {
        transitionTo(AppState::BleScan,
                     "BLE lost after posture resume",
                     controllerMillis());
    }
    return false;
}

bool AppController::connectWifiAfterBleReady(const AppFlowActions& actions) {
    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow("connect WiFi")) {
        return false;
    }

    if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
        if (actions.consumePowerOffDisconnect != nullptr &&
            actions.consumePowerOffDisconnect("BLE lost before WiFi open")) {
            return false;
        }
        transitionTo(AppState::BleScan, "BLE lost before WiFi open", controllerMillis());
        return false;
    }

    transitionTo(AppState::BleReady, "open WiFi via BLE", controllerMillis());
    if (actions.activateCameraWifiOverBle == nullptr || !actions.activateCameraWifiOverBle()) {
        return false;
    }

    if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
        if (actions.disconnectWifi != nullptr) {
            actions.disconnectWifi();
        }
        if (actions.consumePowerOffDisconnect != nullptr &&
            actions.consumePowerOffDisconnect("BLE lost after WiFi open")) {
            return false;
        }
        if (actions.clearBleDisconnectReason != nullptr) {
            actions.clearBleDisconnectReason();
        }
        transitionTo(AppState::BleScan, "BLE lost after WiFi open", controllerMillis());
        return false;
    }

    if (!_previewRequested) {
        bool credentialsReady = actions.hasUsableCachedWifiCredentials != nullptr &&
                                actions.hasUsableCachedWifiCredentials();
        if (actions.readFreshWifiCredentials != nullptr && actions.readFreshWifiCredentials()) {
            if (actions.applyFreshWifiCredentials != nullptr) {
                actions.applyFreshWifiCredentials();
            }
            credentialsReady = true;
        }
        if (!credentialsReady) {
            transitionTo(AppState::BleReady, "WiFi params unavailable while portrait", controllerMillis());
            return false;
        }
        transitionTo(AppState::WifiCredentialsReady,
                     "portrait cached WiFi params; connection paused",
                     controllerMillis());
        return false;
    }

    if (actions.hasUsableCachedWifiCredentials != nullptr &&
        actions.hasUsableCachedWifiCredentials()) {
        transitionTo(AppState::ConnectingWifi, "cached WiFi params", controllerMillis());
        if (actions.connectCachedWifiFromProfile != nullptr &&
            actions.connectCachedWifiFromProfile()) {
            if (actions.onCachedWifiConnected != nullptr) {
                actions.onCachedWifiConnected();
            }
            return true;
        }

        if (actions.onCachedWifiConnectFailed != nullptr) {
            actions.onCachedWifiConnectFailed();
        }
        if (actions.disconnectWifi != nullptr) {
            actions.disconnectWifi();
        }
        if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
            if (actions.consumePowerOffDisconnect != nullptr &&
                actions.consumePowerOffDisconnect("BLE lost during cached WiFi connect")) {
                return false;
            }
            if (actions.clearBleDisconnectReason != nullptr) {
                actions.clearBleDisconnectReason();
            }
            transitionTo(AppState::BleScan, "BLE lost during cached WiFi connect", controllerMillis());
            return false;
        }
        if (!_previewRequested) {
            transitionTo(AppState::WifiCredentialsReady,
                         "portrait cancelled cached WiFi connect",
                         controllerMillis());
            return false;
        }
    }

    if (actions.readFreshWifiCredentials == nullptr || !actions.readFreshWifiCredentials()) {
        if (actions.disconnectWifi != nullptr) {
            actions.disconnectWifi();
        }
        if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
            if (actions.consumePowerOffDisconnect != nullptr &&
                actions.consumePowerOffDisconnect("BLE lost waiting WiFi params")) {
                return false;
            }
            if (actions.clearBleDisconnectReason != nullptr) {
                actions.clearBleDisconnectReason();
            }
            transitionTo(AppState::BleScan, "BLE lost waiting WiFi params", controllerMillis());
            return false;
        }
        transitionTo(AppState::BleReady, "BLE WiFi params unavailable", controllerMillis());
        return false;
    }

    if (actions.applyFreshWifiCredentials != nullptr) {
        actions.applyFreshWifiCredentials();
    }

    transitionTo(AppState::ConnectingWifi, "BLE returned WiFi params", controllerMillis());
    if (actions.connectFreshWifiFromProfile == nullptr || !actions.connectFreshWifiFromProfile()) {
        if (actions.disconnectWifi != nullptr) {
            actions.disconnectWifi();
        }
        if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
            if (actions.consumePowerOffDisconnect != nullptr &&
                actions.consumePowerOffDisconnect("BLE lost during WiFi connect")) {
                return false;
            }
            if (actions.clearBleDisconnectReason != nullptr) {
                actions.clearBleDisconnectReason();
            }
            transitionTo(AppState::BleScan, "BLE lost during WiFi connect", controllerMillis());
            return false;
        }
        transitionTo(_previewRequested ? AppState::BleReady : AppState::WifiCredentialsReady,
                     _previewRequested ? "WiFi connect failed" : "portrait cancelled WiFi connect",
                     controllerMillis());
        return false;
    }

    if (actions.onFreshWifiConnected != nullptr) {
        actions.onFreshWifiConnected();
    }
    return true;
}

bool AppController::httpProbeCamera(const AppFlowActions& actions) {
    if (actions.isWifiConnected == nullptr || !actions.isWifiConnected()) {
        transitionTo(AppState::BleScan, "HTTP probe without WiFi", controllerMillis());
        return false;
    }

    transitionTo(AppState::HttpProbing, "WiFi connected", controllerMillis());
    if (actions.fetchCameraProps == nullptr || !actions.fetchCameraProps()) {
        if (actions.onHttpProbeFailed != nullptr) {
            actions.onHttpProbeFailed();
        }
        return false;
    }

    if (actions.onHttpProbeSucceeded != nullptr) {
        actions.onHttpProbeSucceeded();
    }
    return true;
}

bool AppController::startLiveViewFromProbe(const AppFlowActions& actions) {
    if (!actions.liveviewEnabled) {
        return true;
    }
    if (actions.isWifiConnected == nullptr || !actions.isWifiConnected()) {
        return false;
    }

    if (actions.showStartingLiveView != nullptr) {
        actions.showStartingLiveView();
    }
    transitionTo(AppState::PreviewStarting, "HTTP probe ready", controllerMillis());
    if (actions.openLiveView == nullptr || !actions.openLiveView()) {
        if (actions.onLiveViewOpenFailed != nullptr) {
            actions.onLiveViewOpenFailed();
        }
        return false;
    }

    if (actions.onLiveViewOpened != nullptr) {
        actions.onLiveViewOpened();
    }
    transitionTo(AppState::PreviewRunning, "LiveView opened", controllerMillis());
    return true;
}

void AppController::recoverCameraConnection(const AppFlowActions& actions, const char* reason) {
    const char* recoveryReason = reason != nullptr ? reason : "camera recovery";
    if (actions.cameraRecoveryInProgress != nullptr && actions.cameraRecoveryInProgress()) {
        return;
    }

    if (actions.setCameraRecoveryInProgress != nullptr) {
        actions.setCameraRecoveryInProgress(true);
    }
#if !defined(RVF_NATIVE_BUILD)
    Serial.printf("Camera recovery: %s\n", reason != nullptr ? reason : "manual");
#endif

    if ((actions.isBleConnected == nullptr || !actions.isBleConnected()) &&
        actions.consumePowerOffDisconnect != nullptr &&
        actions.consumePowerOffDisconnect(recoveryReason)) {
        if (actions.setCameraRecoveryInProgress != nullptr) {
            actions.setCameraRecoveryInProgress(false);
        }
        return;
    }

    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow(recoveryReason)) {
        if (actions.setCameraRecoveryInProgress != nullptr) {
            actions.setCameraRecoveryInProgress(false);
        }
        return;
    }

    bool recovered = false;
    const bool needsBleRescan = actions.reasonRequiresBleRescan == nullptr ||
                                actions.reasonRequiresBleRescan(reason);
    if (!needsBleRescan) {
        if (actions.showRecoveryBleReadyRetry != nullptr) {
            actions.showRecoveryBleReadyRetry(reason != nullptr ? reason : "reconnect");
        }
        recovered = resumeFromBleReady(actions, recoveryReason);
    }

    if (!recovered &&
        actions.cameraSleepGuardActive != nullptr &&
        actions.cameraSleepGuardActive()) {
        if (actions.onRecoveryGuardBlocked != nullptr) {
            actions.onRecoveryGuardBlocked();
        } else if (actions.setCameraRecoveryInProgress != nullptr) {
            actions.setCameraRecoveryInProgress(false);
        }
        return;
    }

    if (!recovered) {
        const bool bleLinkAlreadyLost = actions.isBleConnected == nullptr || !actions.isBleConnected();
        if (actions.showRecoveryBleScan != nullptr) {
            actions.showRecoveryBleScan(reason != nullptr ? reason : "reconnect");
        }
        if (actions.disconnectAllTransportsToBleScan != nullptr) {
            actions.disconnectAllTransportsToBleScan(recoveryReason);
        }
        if (bleLinkAlreadyLost) {
            if (actions.resetBleStackBeforeScanAfterLinkLoss != nullptr) {
                actions.resetBleStackBeforeScanAfterLinkLoss(recoveryReason);
            }
        } else if (actions.shortRecoveryDelay != nullptr) {
            actions.shortRecoveryDelay();
        }
        recovered = runCameraFlowOnce(actions, controllerMillis());
    }

    if (actions.onRecoveryFinished != nullptr) {
        actions.onRecoveryFinished(recovered);
    } else if (actions.setCameraRecoveryInProgress != nullptr) {
        actions.setCameraRecoveryInProgress(false);
    }
}

void AppController::serviceCameraFlowIfNeeded(const AppFlowActions& actions, uint32_t nowMs) {
    const uint32_t now = nowMs != 0 ? nowMs : controllerMillis();

    if (actions.consumePowerOffNotification != nullptr &&
        actions.consumePowerOffNotification("BLE power notify 0x00")) {
        return;
    }

    if (actions.cameraRecoveryInProgress != nullptr && actions.cameraRecoveryInProgress()) {
        return;
    }

    const bool bleConnected = actions.isBleConnected != nullptr && actions.isBleConnected();
    const bool wifiConnected = actions.isWifiConnected != nullptr && actions.isWifiConnected();
    const bool previewRequestChanged = _previewRequestChanged;
    _previewRequestChanged = false;

    if (!_previewRequested && (isPreviewActive() || wifiConnected)) {
        if (actions.disconnectWifi != nullptr) {
            actions.disconnectWifi();
        }
        transitionTo(bleConnected ? AppState::WifiCredentialsReady : AppState::BleScan,
                     "portrait disconnects camera WiFi",
                     now);
        if (actions.setLastCameraRecoveryAt != nullptr) {
            actions.setLastCameraRecoveryAt(now);
        }
        return;
    }

    if (isPreviewActive()) {
        return;
    }

    if (_state == AppState::WifiCredentialsReady && bleConnected) {
        if (!_previewRequested) {
            return;
        }
        const bool retryDue = actions.lastCameraRecoveryAt != nullptr &&
                              (now - actions.lastCameraRecoveryAt()) >= actions.bleScanRetryIntervalMs;
        if (!previewRequestChanged && !retryDue) {
            return;
        }
        if (actions.setLastCameraRecoveryAt != nullptr) {
            actions.setLastCameraRecoveryAt(now);
        }
        resumeFromWifiCredentialsReady(actions);
        return;
    }

    if (!bleConnected &&
        actions.consumePowerOffDisconnect != nullptr &&
        actions.consumePowerOffDisconnect("scheduled service")) {
        return;
    }

    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow("scheduled service")) {
        return;
    }

    if (actions.lastCameraRecoveryAt == nullptr || actions.setLastCameraRecoveryAt == nullptr) {
        return;
    }
    if ((now - actions.lastCameraRecoveryAt()) < actions.bleScanRetryIntervalMs) {
        return;
    }

    actions.setLastCameraRecoveryAt(now);
    const bool online = (isBleReady() && bleConnected)
                          ? resumeFromBleReady(actions, "BLE_READY scheduled retry")
                          : runCameraFlowOnce(actions, now);
    if (!online) {
        actions.setLastCameraRecoveryAt(0);
    }
}

void AppController::monitorWifi(const AppFlowActions& actions) {
    if (isPreviewActive() &&
        (actions.isWifiConnected == nullptr || !actions.isWifiConnected())) {
        recoverCameraConnection(actions, "WiFi disconnected");
    }
}

void AppController::monitorLiveView(const AppFlowActions& actions, uint32_t nowMs) {
    (void)nowMs;

    if (actions.consumePowerOffNotification != nullptr &&
        actions.consumePowerOffNotification("BLE power notify 0x00")) {
        return;
    }
    if (!isPreviewActive() || !actions.liveviewEnabled) {
        return;
    }
    if (actions.isBleConnected == nullptr || !actions.isBleConnected()) {
        if (actions.consumePowerOffDisconnect != nullptr &&
            actions.consumePowerOffDisconnect("BLE disconnected")) {
            return;
        }
        recoverCameraConnection(actions, "BLE disconnected");
        return;
    }
    if (actions.isWifiConnected == nullptr || !actions.isWifiConnected()) {
        recoverCameraConnection(actions, "WiFi disconnected");
        return;
    }
    if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
        if (actions.showCameraSleepGuardStatus != nullptr) {
            actions.showCameraSleepGuardStatus();
        }
        return;
    }

    if (actions.previewStreamRunning == nullptr || !actions.previewStreamRunning()) {
        recoverCameraConnection(actions, "LiveView closed");
        return;
    }

    if (actions.readAndProcessLiveViewFrame != nullptr &&
        !actions.readAndProcessLiveViewFrame()) {
        recoverCameraConnection(actions, "LiveView read failed");
        return;
    }

    if (actions.logPreviewStats != nullptr) {
        actions.logPreviewStats();
    }

    if (actions.lastFrameAt != nullptr) {
        const uint32_t stallCheckAt = controllerMillis();
        const uint32_t lastFrameAt = actions.lastFrameAt();
        const uint32_t frameIdleMs = elapsedSince(stallCheckAt, lastFrameAt);
        uint32_t activityIdleMs = frameIdleMs;
        if (actions.lastLiveViewActivityAt != nullptr) {
            const uint32_t liveViewActivityAt = actions.lastLiveViewActivityAt();
            activityIdleMs = elapsedSince(stallCheckAt, liveViewActivityAt);
        }

        const bool frameStalled = frameIdleMs > actions.liveViewStallTimeoutMs;
        const bool streamStalled = activityIdleMs > actions.liveViewStallTimeoutMs;
        if (frameStalled || streamStalled) {
#if !defined(RVF_NATIVE_BUILD)
            Serial.printf("LiveView stall: frame_idle_ms=%lu stream_idle_ms=%lu timeout_ms=%lu\n",
                          static_cast<unsigned long>(frameIdleMs),
                          static_cast<unsigned long>(activityIdleMs),
                          static_cast<unsigned long>(actions.liveViewStallTimeoutMs));
#endif
            recoverCameraConnection(actions,
                                    frameStalled
                                      ? "LiveView frame stall watchdog"
                                      : "LiveView stream stall watchdog");
        }
    }
}

void AppController::handleUserCommand(const AppFlowActions& actions, UserCommand command) {
    switch (command) {
        case UserCommand::Shoot:
            triggerShutterFromButtonA(actions);
            break;
        case UserCommand::ManualWake:
            if (actions.requestManualCameraWake != nullptr) {
                actions.requestManualCameraWake("manual wake");
            }
            break;
        default:
            break;
    }
}

void AppController::triggerShutterFromButtonA(const AppFlowActions& actions) {
    if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
        if (actions.requestManualCameraWake != nullptr) {
            actions.requestManualCameraWake("Button A manual wake");
        }
        return;
    }

    if (actions.shutterReady == nullptr || !actions.shutterReady()) {
        if (actions.showShutterBleNotReady != nullptr) {
            actions.showShutterBleNotReady();
        }
        recoverCameraConnection(actions, "Button A shutter BLE not ready");
        return;
    }

    if (actions.shootAutofocus != nullptr && actions.shootAutofocus()) {
        if (actions.onShutterOk != nullptr) {
            actions.onShutterOk();
        }
        return;
    }

    if (actions.onShutterFailed != nullptr) {
        actions.onShutterFailed();
    }

    if (actions.previewKeptAfterShutterFailure != nullptr &&
        actions.previewKeptAfterShutterFailure()) {
        return;
    }

    recoverCameraConnection(actions, "Button A BLE shutter failed");
}

void AppController::dispatch(const AppMessage& message) {
    switch (message.type) {
        case AppEventType::BleConnected:
            transitionTo(AppState::BleReady, "BLE event connected", message.timestampMs);
            break;
        case AppEventType::BleDisconnected:
            transitionTo(AppState::Disconnected, "BLE event disconnected", message.timestampMs);
            break;
        case AppEventType::CameraPowerOff:
            transitionTo(AppState::CameraPowerOff, "camera power off event", message.timestampMs);
            break;
        case AppEventType::WifiConnected:
            transitionTo(AppState::WifiConnecting, "WiFi event connected", message.timestampMs);
            break;
        case AppEventType::PreviewStarted:
            transitionTo(AppState::PreviewRunning, "preview event started", message.timestampMs);
            break;
        case AppEventType::ErrorRaised:
            transitionTo(AppState::Error, "error event", message.timestampMs);
            break;
        default:
            break;
    }
}

bool AppController::transitionTo(AppState nextState, const char* reason, uint32_t nowMs) {
    if (_state == nextState) {
        return false;
    }

    const uint32_t stamp = nowMs != 0 ? nowMs : controllerMillis();
#if !defined(RVF_NATIVE_BUILD)
    Serial.printf("Flow: %s -> %s (%s) uptime=%lums\n",
                  appStateName(_state),
                  appStateName(nextState),
                  reason != nullptr ? reason : "",
                  static_cast<unsigned long>(stamp));
#else
    (void)reason;
    (void)stamp;
#endif
    _state = nextState;
    return true;
}

void AppController::setPreviewRequested(bool requested) {
    if (_previewRequested == requested) {
        return;
    }
    _previewRequested = requested;
    _previewRequestChanged = true;
}

bool AppController::previewRequested() const {
    return _previewRequested;
}

AppState AppController::state() const {
    return _state;
}

bool AppController::isBleReady() const {
    return _state == AppState::BleReady || _state == AppState::WifiCredentialsReady;
}

bool AppController::isPowerProtectedFlowState() const {
    switch (_state) {
        case AppState::BleReady:
        case AppState::CheckingCameraPower:
        case AppState::ActivatingWifi:
        case AppState::WifiCredentialsReady:
        case AppState::WifiConnecting:
        case AppState::ConnectingWifi:
        case AppState::HttpProbe:
        case AppState::HttpProbing:
        case AppState::PreviewStarting:
        case AppState::LiveViewRunning:
        case AppState::PreviewRunning:
            return true;
        default:
            return false;
    }
}

bool AppController::isPreviewActive() const {
    return _state == AppState::LiveViewRunning || _state == AppState::PreviewRunning;
}

}  // namespace rvf
