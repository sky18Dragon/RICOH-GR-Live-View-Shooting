#include "AppController.h"

#include <Arduino.h>

namespace rvf {

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
}

void AppController::setEventSink(AppEventSink sink) {
    _eventSink = sink;
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
    const uint32_t flowStartMs = nowMs != 0 ? nowMs : millis();
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
            if (httpProbeCamera(actions) &&
                startLiveViewFromProbe(actions)) {
                Serial.printf("Flow: camera online total_ms=%lu\n",
                              static_cast<unsigned long>(millis() - flowStartMs));
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
        transitionTo(AppState::BleScan, "BLE lost before BLE_READY resume", millis());
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
            transitionTo(AppState::BleScan, "BLE lost during WiFi retry", millis());
            return false;
        }

        if (connectWifiAfterBleReady(actions)) {
            if (httpProbeCamera(actions) &&
                startLiveViewFromProbe(actions)) {
                return true;
            }

            if (actions.disconnectWifiLiveViewToBleReady != nullptr) {
                actions.disconnectWifiLiveViewToBleReady("HTTP/LiveView retry from BLE_READY");
            }
        } else if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
            return false;
        }

        if (actions.delayAndYield != nullptr) {
            actions.delayAndYield(actions.retryDelayMs);
        }
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
        transitionTo(AppState::BleScan, "BLE lost before WiFi open", millis());
        return false;
    }

    transitionTo(AppState::BleReady, "open WiFi via BLE", millis());
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
        transitionTo(AppState::BleScan, "BLE lost after WiFi open", millis());
        return false;
    }

    if (actions.hasUsableCachedWifiCredentials != nullptr &&
        actions.hasUsableCachedWifiCredentials()) {
        transitionTo(AppState::ConnectingWifi, "cached WiFi params", millis());
        if (actions.connectCachedWifiFromProfile != nullptr &&
            actions.connectCachedWifiFromProfile()) {
            if (actions.onCachedWifiConnected != nullptr) {
                actions.onCachedWifiConnected();
            }
            publish(AppEventType::WifiConnected, 0, "cached WiFi connected", millis());
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
            transitionTo(AppState::BleScan, "BLE lost during cached WiFi connect", millis());
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
            transitionTo(AppState::BleScan, "BLE lost waiting WiFi params", millis());
            return false;
        }
        transitionTo(AppState::BleReady, "BLE WiFi params unavailable", millis());
        return false;
    }

    if (actions.applyFreshWifiCredentials != nullptr) {
        actions.applyFreshWifiCredentials();
    }

    transitionTo(AppState::ConnectingWifi, "BLE returned WiFi params", millis());
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
            transitionTo(AppState::BleScan, "BLE lost during WiFi connect", millis());
            return false;
        }
        transitionTo(AppState::BleReady, "WiFi connect failed", millis());
        return false;
    }

    if (actions.onFreshWifiConnected != nullptr) {
        actions.onFreshWifiConnected();
    }
    publish(AppEventType::WifiConnected, 0, "fresh WiFi connected", millis());
    return true;
}

bool AppController::httpProbeCamera(const AppFlowActions& actions) {
    if (actions.isWifiConnected == nullptr || !actions.isWifiConnected()) {
        transitionTo(AppState::BleScan, "HTTP probe without WiFi", millis());
        return false;
    }

    transitionTo(AppState::HttpProbing, "WiFi connected", millis());
    if (actions.fetchCameraProps == nullptr || !actions.fetchCameraProps()) {
        if (actions.onHttpProbeFailed != nullptr) {
            actions.onHttpProbeFailed();
        }
        publish(AppEventType::HttpProbeFailed, 0, "camera HTTP probe failed", millis());
        return false;
    }

    if (actions.onHttpProbeSucceeded != nullptr) {
        actions.onHttpProbeSucceeded();
    }
    publish(AppEventType::HttpProbeSucceeded, 0, "camera HTTP probe succeeded", millis());
    return true;
}

bool AppController::startLiveViewFromProbe(const AppFlowActions& actions) {
    if (!actions.liveviewEnabled) {
        return true;
    }
    if (actions.isWifiConnected == nullptr || !actions.isWifiConnected()) {
        return false;
    }

    transitionTo(AppState::PreviewStarting, "HTTP probe ready", millis());
    if (actions.openLiveView == nullptr || !actions.openLiveView()) {
        if (actions.onLiveViewOpenFailed != nullptr) {
            actions.onLiveViewOpenFailed();
        }
        publish(AppEventType::PreviewStopped, 0, "LiveView open failed", millis());
        return false;
    }

    if (actions.onLiveViewOpened != nullptr) {
        actions.onLiveViewOpened();
    }
    transitionTo(AppState::PreviewRunning, "LiveView opened", millis());
    publish(AppEventType::PreviewStarted, 0, "LiveView opened", millis());
    return true;
}

void AppController::recoverCameraConnection(const AppFlowActions& actions, RecoveryCause cause) {
    const char* recoveryReason = recoveryCauseName(cause);
    if (actions.cameraRecoveryInProgress != nullptr && actions.cameraRecoveryInProgress()) {
        return;
    }

    if (actions.setCameraRecoveryInProgress != nullptr) {
        actions.setCameraRecoveryInProgress(true);
    }
    publish(AppEventType::RecoveryStarted,
            static_cast<int>(cause),
            recoveryReason,
            millis());
    Serial.printf("Camera recovery: %s\n", recoveryReason);

    if ((actions.isBleConnected == nullptr || !actions.isBleConnected()) &&
        actions.consumePowerOffDisconnect != nullptr &&
        actions.consumePowerOffDisconnect(recoveryReason)) {
        if (actions.setCameraRecoveryInProgress != nullptr) {
            actions.setCameraRecoveryInProgress(false);
        }
        publish(AppEventType::RecoveryFailed,
                static_cast<int>(cause),
                recoveryReason,
                millis());
        return;
    }

    if (actions.cameraSleepGuardBlocksFlow != nullptr &&
        actions.cameraSleepGuardBlocksFlow(recoveryReason)) {
        if (actions.setCameraRecoveryInProgress != nullptr) {
            actions.setCameraRecoveryInProgress(false);
        }
        publish(AppEventType::RecoveryFailed,
                static_cast<int>(cause),
                recoveryReason,
                millis());
        return;
    }

    bool recovered = false;
    const bool needsBleRescan = actions.reasonRequiresBleRescan == nullptr ||
                                actions.reasonRequiresBleRescan(cause);
    if (!needsBleRescan) {
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
        publish(AppEventType::RecoveryFailed,
                static_cast<int>(cause),
                recoveryReason,
                millis());
        return;
    }

    if (!recovered) {
        const bool bleLinkAlreadyLost = actions.isBleConnected == nullptr || !actions.isBleConnected();
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
        recovered = runCameraFlowOnce(actions, millis());
    }

    if (actions.onRecoveryFinished != nullptr) {
        actions.onRecoveryFinished(recovered);
    } else if (actions.setCameraRecoveryInProgress != nullptr) {
        actions.setCameraRecoveryInProgress(false);
    }
    publish(recovered ? AppEventType::RecoverySucceeded : AppEventType::RecoveryFailed,
            static_cast<int>(cause),
            recoveryReason,
            millis());
}

void AppController::serviceCameraFlowIfNeeded(const AppFlowActions& actions, uint32_t nowMs) {
    const uint32_t now = nowMs != 0 ? nowMs : millis();

    if (actions.consumePowerOffNotification != nullptr &&
        actions.consumePowerOffNotification("BLE power notify 0x00")) {
        return;
    }

    if ((actions.cameraRecoveryInProgress != nullptr && actions.cameraRecoveryInProgress()) ||
        isPreviewActive()) {
        return;
    }

    const bool bleConnected = actions.isBleConnected != nullptr && actions.isBleConnected();
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
        recoverCameraConnection(actions, RecoveryCause::WifiDisconnected);
    }
}

void AppController::monitorLiveView(const AppFlowActions& actions, uint32_t nowMs) {
    const uint32_t now = nowMs != 0 ? nowMs : millis();

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
        recoverCameraConnection(actions, RecoveryCause::BleDisconnected);
        return;
    }
    if (actions.isWifiConnected == nullptr || !actions.isWifiConnected()) {
        recoverCameraConnection(actions, RecoveryCause::WifiDisconnected);
        return;
    }
    if (actions.cameraSleepGuardActive != nullptr && actions.cameraSleepGuardActive()) {
        return;
    }

    if (actions.previewStreamRunning == nullptr || !actions.previewStreamRunning()) {
        recoverCameraConnection(actions, RecoveryCause::PreviewClosed);
        return;
    }

    if (actions.readAndProcessLiveViewFrame != nullptr &&
        !actions.readAndProcessLiveViewFrame()) {
        recoverCameraConnection(actions, RecoveryCause::PreviewReadFailed);
        return;
    }

    if (actions.logPreviewStats != nullptr) {
        actions.logPreviewStats();
    }

    if (actions.lastFrameAt != nullptr) {
        const uint32_t stallCheckAt = millis();
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
            Serial.printf("LiveView stall: frame_idle_ms=%lu stream_idle_ms=%lu timeout_ms=%lu\n",
                          static_cast<unsigned long>(frameIdleMs),
                          static_cast<unsigned long>(activityIdleMs),
                          static_cast<unsigned long>(actions.liveViewStallTimeoutMs));
            recoverCameraConnection(actions,
                                    frameStalled
                                      ? RecoveryCause::FrameStalled
                                      : RecoveryCause::StreamStalled);
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
        publish(AppEventType::ShutterFailed,
                static_cast<int>(RecoveryCause::ShutterBleNotReady),
                recoveryCauseName(RecoveryCause::ShutterBleNotReady),
                millis());
        recoverCameraConnection(actions, RecoveryCause::ShutterBleNotReady);
        return;
    }

    publish(AppEventType::ShutterPressed, 0, "autofocus shutter", millis());
    if (actions.shootAutofocus != nullptr && actions.shootAutofocus()) {
        publish(AppEventType::ShutterSucceeded, 0, "autofocus shutter", millis());
        return;
    }

    publish(AppEventType::ShutterFailed,
            static_cast<int>(RecoveryCause::ShutterFailed),
            recoveryCauseName(RecoveryCause::ShutterFailed),
            millis());

    if (actions.previewKeptAfterShutterFailure != nullptr &&
        actions.previewKeptAfterShutterFailure()) {
        return;
    }

    recoverCameraConnection(actions, RecoveryCause::ShutterFailed);
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

    const uint32_t stamp = nowMs != 0 ? nowMs : millis();
    Serial.printf("Flow: %s -> %s (%s) uptime=%lums\n",
                  appStateName(_state),
                  appStateName(nextState),
                  reason != nullptr ? reason : "",
                  static_cast<unsigned long>(stamp));
    _state = nextState;
    publish(AppEventType::StateChanged,
            static_cast<int>(nextState),
            reason,
            stamp);
    return true;
}

void AppController::publish(AppEventType type,
                            int code,
                            const char* detail,
                            uint32_t nowMs) const {
    if (_eventSink.publish == nullptr) {
        return;
    }
    AppMessage message;
    message.type = type;
    message.timestampMs = nowMs;
    message.code = code;
    message.detail = detail;
    _eventSink.publish(message);
}

AppState AppController::state() const {
    return _state;
}

bool AppController::isBleReady() const {
    return _state == AppState::BleReady;
}

bool AppController::isPowerProtectedFlowState() const {
    switch (_state) {
        case AppState::BleReady:
        case AppState::CheckingCameraPower:
        case AppState::ActivatingWifi:
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
