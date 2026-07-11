#include <Arduino.h>
#include <M5PM1.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include "ble_reconnect_policy.h"
#include "buttons.h"
#include "camera_identity.h"
#include "camera_profile_store.h"
#include "config.h"
#include "app/AppController.h"
#include "core/AppConfig.h"
#include "core/Logger.h"
#include "display/DisplaySurface.h"
#include "gr_api.h"
#include "gr_wifi.h"
#include "jpeg_decoder.h"
#include "mjpeg_stream.h"
#include "ricoh_ble_client.h"
#include "services/BleCameraService.h"
#include "services/CameraPowerPolicy.h"
#include "services/PreviewFrameBuffer.h"
#include "services/WifiPreviewService.h"
#include "supervisor/SystemSupervisor.h"
#include "ui/ButtonInput.h"
#include "ui/core/UiManager.h"
#include "ui/core/UiVariant.h"
#include "ui/presenter/UiPresenter.h"

namespace {

GrWifi grWifi;
GrApi grApi;
MjpegStream mjpeg;
rvf::display::M5DisplaySurface displaySurface;
rvf::ui::ActiveUiRenderer uiRenderer;
rvf::ui::UiPresenter uiPresenter;
rvf::ui::UiManager<rvf::ui::ActiveUiRenderer> uiManager(displaySurface, uiRenderer);
Buttons buttons;
JpegDecoder decoder;
CameraProfileStore profileStore;
CameraProfile cameraProfile;
RicohBleClient ricohBle;
rvf::BleCameraService bleCamera(ricohBle);
M5PM1 stickPower;
rvf::CameraPowerPolicy cameraPowerPolicy;
rvf::WifiPreviewService wifiPreview(grWifi, grApi, mjpeg);
rvf::PreviewFrameBuffer previewFrameBuffer;
rvf::SystemSupervisor systemSupervisor;
using CameraFlowState = rvf::AppState;
rvf::AppController appController(CameraFlowState::BleScan);
uint8_t* frameBuffer = nullptr;
uint8_t streamReadBuffer[rvf::AppConfig::Buffer::kStreamReadBufferSize];
CameraProps cameraProps;
CameraProps pendingCameraProps;
RicohBleWifiCredentials pendingFreshWifiCredentials;

bool liveviewEnabled = true;
uint32_t lastFrameAt = 0;
uint32_t lastLiveViewActivityAt = 0;
uint32_t lastStatusAt = 0;
uint32_t powerButtonLastPollAt = 0;
uint32_t powerButtonPressedSince = 0;
bool powerButtonHoldReported = false;
bool stickPowerReady = false;
uint32_t lastCameraRecoveryAt = 0;
bool cameraRecoveryInProgress = false;
bool setupCameraFlowActive = false;
bool key2PairingResetRequested = false;
bool cameraAutoWakeBlocked = false;
int cameraAutoWakeDisconnectReason = 0;
uint32_t cameraPowerProbeBackoffUntil = 0;
RicohCameraPowerState cameraPowerState = RicohCameraPowerState::Unknown;
RicohCameraOperationMode cameraOperationMode = RicohCameraOperationMode::Unknown;
uint32_t lastPropsAt = 0;
uint32_t decodedFrames = 0;
uint32_t fpsWindowStart = 0;
uint32_t fpsWindowFrames = 0;
float currentFps = 0.0f;
bool wifiCacheRefreshPending = false;
uint32_t wifiCacheRefreshAfter = 0;

String uiDetail = rvf::AppConfig::Ui::kBootMessage;
String uiLocalIp;
String uiErrorMessage;
int uiErrorCode = 0;
bool uiSurfaceReady = false;
bool uiRecovering = false;
bool uiPairingReset = false;
bool uiShutdownRequested = false;
bool uiErrorActive = false;
rvf::ui::UiShutterStatus uiShutterStatus = rvf::ui::UiShutterStatus::Idle;
uint32_t uiShutterStatusUntil = 0;

constexpr uint32_t UI_SHUTTER_SUCCESS_MS = 800;
constexpr uint32_t UI_SHUTTER_FAILURE_MS = 1500;

void requestManualCameraWake(const char* source);
void resetBlePairingFromKey2();
rvf::AppFlowActions makeAppFlowActions();
rvf::ui::UiRuntimeSnapshot makeUiRuntimeSnapshot();
void renderCurrentUi(bool force = false);
void onAppEventForUi(const rvf::AppMessage& message);

bool beginStickPower() {
  const int8_t sda = M5.getPin(m5::pin_name_t::in_i2c_sda);
  const int8_t scl = M5.getPin(m5::pin_name_t::in_i2c_scl);
  const m5pm1_err_t err = stickPower.begin(&Wire, M5PM1_DEFAULT_ADDR, sda, scl, M5PM1_I2C_FREQ_100K);
  stickPowerReady = (err == M5PM1_OK);
  if (!stickPowerReady) {
    LOGW("POWER", "Power: M5PM1 begin failed err=%d; fallback to M5Unified power API", static_cast<int>(err));
    return false;
  }

  stickPower.setDoubleOffDisable(false);
  stickPower.btnSetConfig(M5PM1_BTN_TYPE_DOUBLE, M5PM1_BTN_DOUBLE_CLICK_DELAY_500MS);

  bool pressed = false;
  stickPower.btnGetState(&pressed);
  LOGLINE_I("POWER", "Power: M5PM1 ready");
  return true;
}

bool isStickPowerButtonPressed() {
  if (stickPowerReady) {
    bool pressed = false;
    const m5pm1_err_t err = stickPower.btnGetState(&pressed);
    if (err == M5PM1_OK) {
      return pressed;
    }

    // A failed M5PM1 I2C transaction can block for about a second. Retrying it
    // on every loop starves the 2 KB LiveView reads and prevents the first JPEG
    // frame from completing. Keep the M5Unified button/power-off fallback, but
    // stop polling the unhealthy I2C path until the next reboot.
    stickPowerReady = false;
    LOGW("POWER",
         "Power: M5PM1 button read failed err=%d; disabling M5PM1 polling and using M5Unified",
         static_cast<int>(err));
  }
  return M5.BtnPWR.isPressed();
}

bool pollStickPowerHold() {
  const uint32_t now = millis();
  if ((now - powerButtonLastPollAt) < POWER_BUTTON_POLL_MS) {
    return false;
  }
  powerButtonLastPollAt = now;

  const bool pressed = isStickPowerButtonPressed();
  if (!pressed) {
    powerButtonPressedSince = 0;
    powerButtonHoldReported = false;
    return false;
  }

  if (powerButtonPressedSince == 0) {
    powerButtonPressedSince = now;
  }
  if (!powerButtonHoldReported && (now - powerButtonPressedSince) >= POWER_BUTTON_HOLD_MS) {
    powerButtonHoldReported = true;
    return true;
  }
  return false;
}

const char* cameraPowerStateName(RicohCameraPowerState state) {
  switch (state) {
    case RicohCameraPowerState::On:
      return "ON";
    case RicohCameraPowerState::OffOrShuttingDown:
      return "OFF";
    case RicohCameraPowerState::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* cameraOperationModeName(RicohCameraOperationMode mode) {
  switch (mode) {
    case RicohCameraOperationMode::Capture:
      return "CAPTURE";
    case RicohCameraOperationMode::Playback:
      return "PLAYBACK";
    case RicohCameraOperationMode::BleStartup:
      return "BLE_STARTUP";
    case RicohCameraOperationMode::Other:
      return "OTHER";
    case RicohCameraOperationMode::PowerOffTransfer:
      return "POWER_OFF_TRANSFER";
    case RicohCameraOperationMode::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

bool isCameraStandbyOperationMode(RicohCameraOperationMode mode) {
  return mode == RicohCameraOperationMode::BleStartup ||
         mode == RicohCameraOperationMode::PowerOffTransfer;
}

rvf::ui::UiRuntimeSnapshot makeUiRuntimeSnapshot() {
  rvf::ui::UiRuntimeSnapshot snapshot;
  snapshot.appState = appController.state();
  snapshot.bleConnected = bleCamera.isConnected();
  snapshot.wifiConnected = grWifi.isConnected();
  snapshot.previewRunning = wifiPreview.isPreviewRunning();
  snapshot.cameraStandby = cameraAutoWakeBlocked ||
                           snapshot.appState == CameraFlowState::CameraSleepGuard ||
                           snapshot.appState == CameraFlowState::CameraPowerOff ||
                           cameraPowerState == RicohCameraPowerState::OffOrShuttingDown ||
                           isCameraStandbyOperationMode(cameraOperationMode);
  snapshot.shutterReady = snapshot.bleConnected && bleCamera.shutterReady();
  snapshot.recovering = uiRecovering;
  snapshot.pairingReset = uiPairingReset;
  snapshot.shutdownRequested = uiShutdownRequested;
  snapshot.errorActive = uiErrorActive;
  const bool shutterFeedbackActive = uiShutterStatusUntil != 0 &&
                                     static_cast<int32_t>(millis() - uiShutterStatusUntil) < 0;
  snapshot.shutterStatus = shutterFeedbackActive
                             ? uiShutterStatus
                             : rvf::ui::UiShutterStatus::Idle;
  snapshot.wifiRssi = snapshot.wifiConnected ? grWifi.rssi() : 0;
  snapshot.fps = currentFps;
  snapshot.renderedFrames = decodedFrames;
  snapshot.droppedFrames = mjpeg.droppedFrames();
  snapshot.cameraName = cameraProfile.cameraName.c_str();
  snapshot.cameraModel = cameraProps.model.c_str();
  snapshot.battery = cameraProps.battery.c_str();
  snapshot.localIp = uiLocalIp.c_str();
  snapshot.detail = uiDetail.c_str();
  snapshot.errorCode = uiErrorCode;
  snapshot.errorMessage = uiErrorMessage.c_str();
  return snapshot;
}

void renderCurrentUi(bool force) {
  if (!uiSurfaceReady) {
    return;
  }
  const rvf::ui::UiModel model = uiPresenter.present(makeUiRuntimeSnapshot());
  uiManager.update(model, millis(), force);
}

void onAppEventForUi(const rvf::AppMessage& message) {
  uiDetail = message.detail != nullptr ? message.detail : "";
  switch (message.type) {
    case rvf::AppEventType::ShutterPressed:
      uiShutterStatus = rvf::ui::UiShutterStatus::Shooting;
      uiShutterStatusUntil = millis() + UI_SHUTTER_SUCCESS_MS;
      break;
    case rvf::AppEventType::ShutterSucceeded:
      uiShutterStatus = rvf::ui::UiShutterStatus::Succeeded;
      uiShutterStatusUntil = millis() + UI_SHUTTER_SUCCESS_MS;
      break;
    case rvf::AppEventType::ShutterFailed:
      uiShutterStatus = rvf::ui::UiShutterStatus::Failed;
      uiShutterStatusUntil = millis() + UI_SHUTTER_FAILURE_MS;
      break;
    case rvf::AppEventType::RecoveryStarted:
      uiRecovering = true;
      break;
    case rvf::AppEventType::RecoverySucceeded:
    case rvf::AppEventType::RecoveryFailed:
      uiRecovering = false;
      break;
    case rvf::AppEventType::PairingResetStarted:
      uiPairingReset = true;
      break;
    case rvf::AppEventType::PairingResetCompleted:
      uiPairingReset = false;
      break;
    case rvf::AppEventType::ShutdownRequested:
      uiShutdownRequested = true;
      break;
    case rvf::AppEventType::ErrorRaised:
      uiErrorActive = true;
      uiErrorCode = message.code;
      uiErrorMessage = message.detail != nullptr ? message.detail : "Unknown error";
      break;
    case rvf::AppEventType::StateChanged:
      if (appController.state() == CameraFlowState::Error) {
        uiErrorActive = true;
        uiErrorCode = message.code;
        uiErrorMessage = message.detail != nullptr ? message.detail : "Application error";
      } else {
        uiErrorActive = false;
        uiErrorCode = 0;
        uiErrorMessage = "";
      }
      break;
    default:
      break;
  }
  uiManager.invalidate();
  renderCurrentUi(true);
}

void publishLocalAppEvent(rvf::AppEventType type, int code, const char* detail) {
  rvf::AppMessage message;
  message.type = type;
  message.timestampMs = millis();
  message.code = code;
  message.detail = detail;
  onAppEventForUi(message);
}

void renderErrorUi(const char* message, int code = 0) {
  if (!uiSurfaceReady) {
    return;
  }
  rvf::ui::UiRuntimeSnapshot snapshot = makeUiRuntimeSnapshot();
  snapshot.appState = CameraFlowState::Error;
  snapshot.errorCode = code;
  snapshot.errorMessage = message != nullptr ? message : "Unknown error";
  uiManager.invalidate();
  uiManager.update(uiPresenter.present(snapshot), millis(), true);
}

rvf::CameraPowerStatus toPolicyPowerStatus(RicohCameraPowerState state) {
  switch (state) {
    case RicohCameraPowerState::On:
      return rvf::CameraPowerStatus::On;
    case RicohCameraPowerState::OffOrShuttingDown:
      return rvf::CameraPowerStatus::Off;
    case RicohCameraPowerState::Unknown:
      return rvf::CameraPowerStatus::Unknown;
  }
  return rvf::CameraPowerStatus::Unknown;
}

rvf::CameraOperationStatus toPolicyOperationStatus(RicohCameraOperationMode mode) {
  if (isCameraStandbyOperationMode(mode)) {
    return rvf::CameraOperationStatus::Standby;
  }
  if (mode == RicohCameraOperationMode::Unknown) {
    return rvf::CameraOperationStatus::Unknown;
  }
  return rvf::CameraOperationStatus::Ready;
}

void setCameraFlowState(CameraFlowState state, const char* reason) {
  appController.transitionTo(state, reason, millis());
}

void waitForSerialConsole() {
  const uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < rvf::AppConfig::SerialPort::kBootWaitMs) {
    delay(10);
  }
  Serial.setDebugOutput(false);
  Serial.println();
  LOGI("BOOT", "%s", rvf::AppConfig::Ui::kBanner);
}

void closeLiveView(const char* reason) {
  if (wifiPreview.isPreviewRunning()) {
    Serial.printf("LiveView: closing (%s)\n", reason != nullptr ? reason : "reset");
  }
  wifiPreview.stopPreview();
}

String preferredBleName() {
  if (cameraProfile.cameraName.length() > 0) {
    return cameraProfile.cameraName;
  }
  const std::string derivedName = deriveBleNameFromWifiSsid(cameraProfile.wifi.ssid.c_str());
  return String(derivedName.c_str());
}

bool timeReached(uint32_t deadlineMs) {
  return static_cast<int32_t>(millis() - deadlineMs) >= 0;
}

bool isCameraPowerOffDisconnectReason(int reason) {
  return reason == RICOH_BLE_DISCONNECT_REMOTE_USER ||
         reason == RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF;
}

bool cameraSleepGuardActive() {
  // Camera-off wait mode is observability only: it must not block the
  // controller from periodically scanning and probing BLE again.
  return false;
}

bool cameraPowerProbeBackoffActive() {
  return cameraAutoWakeBlocked &&
         cameraPowerProbeBackoffUntil != 0 &&
         !timeReached(cameraPowerProbeBackoffUntil);
}

uint32_t cameraPowerProbeBackoffRemainingMs() {
  if (!cameraPowerProbeBackoffActive()) {
    return 0;
  }
  return cameraPowerProbeBackoffUntil - millis();
}

void scheduleCameraPowerProbeBackoff(const char* source) {
  cameraPowerProbeBackoffUntil = millis() + CAMERA_POWER_OFF_PROBE_BACKOFF_MS;
  Serial.printf("BLE guard: next power probe in %lums (%s)\n",
                static_cast<unsigned long>(CAMERA_POWER_OFF_PROBE_BACKOFF_MS),
                source != nullptr ? source : "camera standby");
}

void enterCameraSleepGuard(const char* source, int reason) {
  const char* guardSource = source != nullptr ? source : "camera standby";
  if (cameraAutoWakeBlocked) {
    if (reason != 0 && cameraAutoWakeDisconnectReason == 0) {
      cameraAutoWakeDisconnectReason = reason;
    }
    cameraPowerState = RicohCameraPowerState::OffOrShuttingDown;
    cameraOperationMode = RicohCameraOperationMode::Unknown;
    scheduleCameraPowerProbeBackoff(guardSource);
    closeLiveView(guardSource);
    wifiPreview.disconnectWifi();
    bleCamera.disconnect();
    setCameraFlowState(CameraFlowState::CameraPowerOff, guardSource);
    lastFrameAt = millis();
    lastCameraRecoveryAt = millis();
    return;
  }

  cameraPowerState = RicohCameraPowerState::OffOrShuttingDown;
  cameraOperationMode = RicohCameraOperationMode::Unknown;
  cameraAutoWakeBlocked = true;
  cameraAutoWakeDisconnectReason = reason;
  scheduleCameraPowerProbeBackoff(guardSource);

  closeLiveView(guardSource);
  wifiPreview.disconnectWifi();
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::CameraPowerOff, guardSource);
  Serial.printf("BLE guard: remote disconnect reason=%d; waiting for camera power on, auto scan continues\n",
                reason);
  lastFrameAt = millis();
  lastCameraRecoveryAt = millis();
}

bool consumeCameraPowerOffNotification(const char* source) {
  if (!bleCamera.consumePowerOffNotification()) {
    return false;
  }

  enterCameraSleepGuard(source != nullptr ? source : "BLE power notify 0x00", RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
  return true;
}

bool settleAndConsumeCameraPowerOffNotification(const char* source) {
  if (RICOH_BLE_POWER_NOTIFY_SETTLE_MS > 0) {
    delay(RICOH_BLE_POWER_NOTIFY_SETTLE_MS);
    yield();
  }
  return consumeCameraPowerOffNotification(source);
}

bool consumeCameraPowerOffDisconnect(const char* source) {
  const int reason = bleCamera.consumeDisconnectReason();
  if (!isCameraPowerOffDisconnectReason(reason)) {
    return false;
  }

  enterCameraSleepGuard(source, reason);
  return true;
}

bool consumeCameraPowerOffDisconnectAfterReady(const char* source) {
  const int reason = bleCamera.consumeDisconnectReason();
  if (!isCameraPowerOffDisconnectReason(reason)) {
    return false;
  }

  if (appController.isPowerProtectedFlowState()) {
    enterCameraSleepGuard(source, reason);
    return true;
  }

  Serial.printf("BLE guard: ignoring pre-ready remote disconnect reason=%d source=%s\n",
                reason,
                source != nullptr ? source : "");
  return false;
}

void clearCameraSleepGuard(const char* source) {
  if (cameraAutoWakeBlocked) {
    Serial.printf("BLE guard: clearing guard (%s), previous disconnect reason=%d\n",
                  source != nullptr ? source : "manual",
                  cameraAutoWakeDisconnectReason);
  }
  cameraPowerState = RicohCameraPowerState::Unknown;
  cameraOperationMode = RicohCameraOperationMode::Unknown;
  cameraAutoWakeBlocked = false;
  cameraPowerProbeBackoffUntil = 0;
  cameraAutoWakeDisconnectReason = 0;
  bleCamera.clearDisconnectReason();
}

bool cameraSleepGuardBlocksFlow(const char* action) {
  (void)action;
  return false;
}

bool hasUsableCachedWifiCredentials() {
  return cameraProfile.wifi.cached && cameraProfile.wifi.ssid.length() > 0 && cameraProfile.bleAddress.length() > 0;
}

bool wifiCredentialsMatchProfile(const RicohBleWifiCredentials& credentials) {
  if (credentials.ssid != cameraProfile.wifi.ssid ||
      credentials.passphrase != cameraProfile.wifi.passphrase) {
    return false;
  }
  if (credentials.bssid.length() > 0 && credentials.bssid != cameraProfile.wifi.bssid) {
    return false;
  }
  if (credentials.channel != 0 && cameraProfile.wifi.channel != 0 && credentials.channel != cameraProfile.wifi.channel) {
    return false;
  }
  return true;
}

void saveWifiCredentialCache(const char* source) {
  if (cameraProfile.bleAddress.length() == 0 || cameraProfile.wifi.ssid.length() == 0) {
    return;
  }
  if (profileStore.saveWifiCredentials(cameraProfile.bleAddress, cameraProfile.wifi)) {
    cameraProfile.wifi.cached = true;
    Serial.printf("WiFi cache: saved (%s) ssid='%s' bssid='%s' channel=%u freq=%u\n",
                  source != nullptr ? source : "update",
                  cameraProfile.wifi.ssid.c_str(),
                  cameraProfile.wifi.bssid.c_str(),
                  static_cast<unsigned>(cameraProfile.wifi.channel),
                  static_cast<unsigned>(cameraProfile.wifi.frequencyMhz));
  }
}

void saveConnectedWifiBssidToCache(const char* source) {
  if (!grWifi.isConnected() || cameraProfile.bleAddress.length() == 0 || cameraProfile.wifi.ssid.length() == 0) {
    return;
  }

  const String connectedBssid = grWifi.bssidString();
  if (connectedBssid.length() > 0 && connectedBssid != String("00:00:00:00:00:00")) {
    if (cameraProfile.wifi.bssid != connectedBssid) {
      Serial.printf("WiFi cache: learned BSSID '%s' from connection (%s)\n",
                    connectedBssid.c_str(),
                    source != nullptr ? source : "connected");
    }
    cameraProfile.wifi.bssid = connectedBssid;
  }
  saveWifiCredentialCache(source != nullptr ? source : "connected");
}

void applyBleWifiCredentials(const RicohBleWifiCredentials& credentials, const char* source, bool persist) {
  const bool changed = credentials.ssid != cameraProfile.wifi.ssid ||
                       credentials.passphrase != cameraProfile.wifi.passphrase ||
                       credentials.bssid != cameraProfile.wifi.bssid ||
                       (credentials.frequencyMhz != 0 && credentials.frequencyMhz != cameraProfile.wifi.frequencyMhz) ||
                       (credentials.channel != 0 && credentials.channel != cameraProfile.wifi.channel);

  cameraProfile.wifi.ssid = credentials.ssid;
  cameraProfile.wifi.passphrase = credentials.passphrase;
  cameraProfile.wifi.bssid = credentials.bssid;
  cameraProfile.wifi.frequencyMhz = credentials.frequencyMhz;
  cameraProfile.wifi.channel = credentials.channel;

  Serial.printf("WiFi params: %s ssid='%s' channel=%u freq=%u changed=%d\n",
                source != nullptr ? source : "BLE",
                cameraProfile.wifi.ssid.c_str(),
                static_cast<unsigned>(cameraProfile.wifi.channel),
                static_cast<unsigned>(cameraProfile.wifi.frequencyMhz),
                changed ? 1 : 0);
  if (persist) {
    saveWifiCredentialCache(source);
  }
}

void scheduleWifiCacheRefresh() {
  if (!bleCamera.isConnected() || cameraProfile.bleAddress.length() == 0) {
    return;
  }
  wifiCacheRefreshPending = true;
  wifiCacheRefreshAfter = millis() + WIFI_CACHE_REFRESH_DELAY_MS;
}

void refreshWifiCacheIfDue() {
  if (!wifiCacheRefreshPending || !timeReached(wifiCacheRefreshAfter)) {
    return;
  }
  wifiCacheRefreshPending = false;
  if (!bleCamera.isConnected() || cameraSleepGuardActive()) {
    return;
  }

  RicohBleWifiCredentials fresh;
  const rvf::Result refreshCredentialsResult = bleCamera.waitForWifiCredentials(fresh, RICOH_BLE_WIFI_CREDENTIAL_WAIT_MS);
  if (refreshCredentialsResult.failed()) {
    Serial.printf("WiFi cache: deferred refresh failed: %s\n", bleCamera.lastError().c_str());
    return;
  }

  if (!wifiCredentialsMatchProfile(fresh)) {
    Serial.println("WiFi cache: BLE fresh params differ from cached/runtime params; updating cache for next boot");
  }
  applyBleWifiCredentials(fresh, "deferred BLE refresh", false);
  saveConnectedWifiBssidToCache("deferred BLE refresh");
}

void applyDefaultProfile() {
  if (!profileStore.load(cameraProfile)) {
    Serial.println("Profile: NVS open failed; using runtime defaults");
    cameraProfile = CameraProfile{};
  }

  if (cameraProfile.wifi.cameraIp.isEmpty()) {
    cameraProfile.wifi.cameraIp = GR_HOST;
  }
  wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);

  Serial.printf("Profile: camera='%s' ble='%s' ip='%s'\n",
                cameraProfile.cameraName.c_str(),
                cameraProfile.bleAddress.c_str(),
                cameraProfile.wifi.cameraIp.c_str());
}

bool ensureCameraPowerReadyForWifi(const char* source) {
  if (!cameraPowerPolicy.requiresPowerCheck()) {
    return true;
  }
  if (cameraSleepGuardBlocksFlow(source != nullptr ? source : "power guard")) {
    return false;
  }
  if (!bleCamera.isConnected()) {
    cameraPowerState = RicohCameraPowerState::Unknown;
    cameraOperationMode = RicohCameraOperationMode::Unknown;
    return false;
  }

  setCameraFlowState(CameraFlowState::CheckingCameraPower, source != nullptr ? source : "check camera power");
  RicohCameraPowerState nextState = RicohCameraPowerState::Unknown;
  bool readOk = false;
  const uint8_t retries = RICOH_BLE_POWER_READ_RETRIES == 0 ? 1 : RICOH_BLE_POWER_READ_RETRIES;
  for (uint8_t attempt = 0; attempt < retries; ++attempt) {
    const rvf::Result powerReadResult = bleCamera.readPowerState(nextState);
    if (powerReadResult.ok()) {
      readOk = true;
      break;
    }
    Serial.printf("BLE: power read attempt %u/%u failed: %s\n",
                  static_cast<unsigned>(attempt + 1),
                  static_cast<unsigned>(retries),
                  bleCamera.lastError().c_str());
    delay(120);
    yield();
  }

  cameraPowerState = readOk ? nextState : RicohCameraPowerState::Unknown;
  if (consumeCameraPowerOffNotification("BLE power notify during power check")) {
    return false;
  }

  cameraOperationMode = RicohCameraOperationMode::Unknown;
  bool operationModeReadOk = false;
  if (cameraPowerPolicy.shouldReadOperationMode(readOk, toPolicyPowerStatus(cameraPowerState))) {
    const uint8_t modeRetries = RICOH_BLE_OPERATION_MODE_READ_RETRIES == 0 ? 1 : RICOH_BLE_OPERATION_MODE_READ_RETRIES;
    for (uint8_t attempt = 0; attempt < modeRetries; ++attempt) {
      const rvf::Result operationModeResult = bleCamera.readOperationMode(cameraOperationMode);
      if (operationModeResult.ok()) {
        operationModeReadOk = true;
        break;
      }
      Serial.printf("BLE: operation mode read attempt %u/%u failed: %s\n",
                    static_cast<unsigned>(attempt + 1),
                    static_cast<unsigned>(modeRetries),
                    bleCamera.lastError().c_str());
      delay(80);
      yield();
    }

    if (cameraPowerPolicy.blocksStandbyOperationMode(operationModeReadOk, toPolicyOperationStatus(cameraOperationMode), false)) {
      Serial.printf("WiFi blocked: camera operation mode=%s while power=%s source=%s\n",
                    cameraOperationModeName(cameraOperationMode),
                    cameraPowerStateName(cameraPowerState),
                    source != nullptr ? source : "");
      enterCameraSleepGuard("BLE operation mode standby", RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
      return false;
    }
  }

  if (consumeCameraPowerOffNotification("BLE power notify before WiFi allow")) {
    return false;
  }

  if (cameraPowerPolicy.mayActivateWifi(toPolicyPowerStatus(cameraPowerState))) {
    if (cameraAutoWakeBlocked) {
      clearCameraSleepGuard("camera power on");
    }
    const rvf::Result powerNotifyResult = bleCamera.enablePowerStateNotify();
    if (powerNotifyResult.failed()) {
      Serial.printf("BLE: power notify subscribe failed: %s\n", bleCamera.lastError().c_str());
    }
    if (settleAndConsumeCameraPowerOffNotification("BLE power notify before WiFi allow")) {
      return false;
    }
    return true;
  }

  if (cameraPowerPolicy.allowsWifiWhenPowerUnknown(readOk)) {
    Serial.println("BLE: power state unknown; config allows WiFi open");
    return true;
  }

  const char* reason = cameraPowerPolicy.blockedReason(readOk, toPolicyPowerStatus(cameraPowerState));
  Serial.printf("WiFi blocked: camera power state=%s readOk=%d source=%s\n",
                cameraPowerStateName(cameraPowerState),
                readOk ? 1 : 0,
                source != nullptr ? source : "");
  enterCameraSleepGuard(reason, RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
  return false;
}

bool activateCameraWifiOverBle() {
  if (!RICOH_BLE_AUTO_WLAN_ON_BOOT) {
    return true;
  }
  if (cameraSleepGuardBlocksFlow("WiFi open")) {
    return false;
  }
  if (!bleCamera.isConnected()) {
    return false;
  }
  if (!ensureCameraPowerReadyForWifi("WiFi open")) {
    return false;
  }
  if (settleAndConsumeCameraPowerOffNotification("BLE power notify before WiFi open")) {
    return false;
  }

  setCameraFlowState(CameraFlowState::ActivatingWifi, "BLE WiFi ON");
  const rvf::Result openWifiResult = bleCamera.openWifi();
  if (openWifiResult.failed()) {
    Serial.printf("BLE: Wi-Fi open failed: %s\n", bleCamera.lastError().c_str());
    setCameraFlowState(CameraFlowState::BleReady, "BLE WiFi open failed");
    return false;
  }

  if (RICOH_BLE_POST_WLAN_ON_WAIT_MS > 0) {
    delay(RICOH_BLE_POST_WLAN_ON_WAIT_MS);
  }
  return true;
}

bool hasStoredBleIdentity() {
  return cameraProfile.bleAddress.length() > 0;
}

String displayBleName(const RicohBleDeviceInfo& info) {
  String connectedName = info.name.length() > 0 ? info.name : preferredBleName();
  if (connectedName.isEmpty()) {
    connectedName = "RICOH GR";
  }
  return connectedName;
}

bool hasAdvertisedCameraIdentity(const RicohBleDeviceInfo& info) {
  return info.name.length() > 0 ||
         info.hasInfoService ||
         info.hasCameraService ||
         info.hasShootingService ||
         info.hasControlService;
}

bool shouldSkipWeakStoredIdentityCandidate(const RicohBleDeviceInfo& info, bool firstBootPairing) {
  return !firstBootPairing &&
         hasStoredBleIdentity() &&
         info.address.equalsIgnoreCase(cameraProfile.bleAddress) &&
         !hasAdvertisedCameraIdentity(info);
}

bool shouldDelayStoredIdentityPowerProbe(const RicohBleDeviceInfo& info, bool firstBootPairing) {
  return !firstBootPairing &&
         hasStoredBleIdentity() &&
         info.address.equalsIgnoreCase(cameraProfile.bleAddress) &&
         cameraPowerProbeBackoffActive();
}

bool shouldBackoffAfterStoredIdentityConnectFailure(const RicohBleDeviceInfo& info,
                                                    const String& errorText,
                                                    bool firstBootPairing) {
  if (firstBootPairing || !hasStoredBleIdentity()) {
    return false;
  }
  if (!bleCandidateMatchesStoredIdentity(cameraProfile.bleAddress.c_str(), info.address.c_str())) {
    return false;
  }
  return errorText.indexOf("security") >= 0 ||
         errorText.indexOf("Security") >= 0 ||
         errorText.indexOf("BLE lost during security") >= 0;
}

void deferStoredIdentityPowerProbeAfterConnectFailure(const String& errorText) {
  cameraPowerState = RicohCameraPowerState::Unknown;
  cameraOperationMode = RicohCameraOperationMode::Unknown;
  cameraAutoWakeBlocked = true;
  if (cameraAutoWakeDisconnectReason == 0) {
    cameraAutoWakeDisconnectReason = RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF;
  }
  scheduleCameraPowerProbeBackoff(errorText.c_str());
}

void saveConnectedBleIdentity(const String& connectedName, const RicohBleDeviceInfo& info) {
  cameraProfile.cameraName = connectedName;
  cameraProfile.bleAddress = info.address;
  cameraProfile.bleAddressType = info.addressType;
  cameraProfile.bleAddressTypeKnown = true;
  cameraProfile.bleBonded = bleCamera.isBonded(info);
  profileStore.saveBleIdentity(cameraProfile.cameraName,
                               cameraProfile.bleAddress,
                               cameraProfile.bleAddressType,
                               cameraProfile.bleBonded);
}

bool serviceButtonsDuringBleOperation() {
  const ButtonEvents events = buttons.poll();
  if (!events.resetPairing) {
    return false;
  }

  key2PairingResetRequested = true;
  Serial.println("BLE pairing reset: requested during BLE operation");
  return true;
}

bool resetBlePairingIfRequested() {
  if (!key2PairingResetRequested) {
    return false;
  }
  key2PairingResetRequested = false;
  resetBlePairingFromKey2();
  return true;
}

bool runBleDiscoveryAtBoot() {
  if (cameraSleepGuardBlocksFlow("BLE discovery")) {
    return false;
  }
  const bool firstBootPairing = !hasStoredBleIdentity();
  const uint8_t configuredAttempts = firstBootPairing
                                       ? FIRST_BOOT_BLE_PAIRING_ATTEMPTS
                                       : (setupCameraFlowActive ? 1 : BLE_CONNECT_ATTEMPTS);
  const uint8_t attempts = configuredAttempts == 0 ? 1 : configuredAttempts;
  uint8_t consecutiveConnectFailures = 0;
  String retryPreferredAddress = cameraProfile.bleAddress;
  String retryPreferredName = preferredBleName();
  bool bondedFastSecurityAttempted = false;

  if (firstBootPairing) {
    Serial.printf("BLE: no stored identity; pairing scan up to %u rounds\n", static_cast<unsigned>(attempts));
  } else if (setupCameraFlowActive) {
    Serial.printf("BLE: stored identity; setup quick scan preferred address='%s' name='%s'\n",
                  cameraProfile.bleAddress.c_str(),
                  preferredBleName().c_str());
  } else {
    Serial.printf("BLE: stored identity; scanning preferred address='%s' name='%s'\n",
                  cameraProfile.bleAddress.c_str(),
                  preferredBleName().c_str());
  }

  for (uint8_t attempt = 1; attempt <= attempts; ++attempt) {
    if (resetBlePairingIfRequested()) {
      return false;
    }
    bool skipRetryDelay = false;
    const String bleName = retryPreferredName;
    setCameraFlowState(CameraFlowState::ScanningCamera, "BLE discovery");

    RicohBleDeviceInfo info = bleCamera.scanCamera(retryPreferredAddress, bleName, BLE_SCAN_SECONDS);
    if (resetBlePairingIfRequested()) {
      return false;
    }
    if (!info.found) {
      // Continue scanning within the existing retry budget.
    } else if (!info.connectable) {
      Serial.printf("BLE: scan selected non-connectable candidate name='%s' addr=%s; retrying\n",
                    info.name.c_str(),
                    info.address.c_str());
    } else if (!bleCandidateMatchesStoredIdentity(cameraProfile.bleAddress.c_str(), info.address.c_str())) {
      Serial.printf("BLE: ignoring non-stored camera addr=%s name='%s'; waiting for stored addr=%s\n",
                    info.address.c_str(),
                    info.name.c_str(),
                    cameraProfile.bleAddress.c_str());
    } else if (shouldDelayStoredIdentityPowerProbe(info, firstBootPairing)) {
      Serial.printf("BLE: skipping standby power probe for %lums addr=%s name='%s'\n",
                    static_cast<unsigned long>(cameraPowerProbeBackoffRemainingMs()),
                    info.address.c_str(),
                    info.name.c_str());
      // Waiting for the target camera's power-probe cooldown is not a failed
      // connection. Preserve the retry budget so a rebooting camera still gets
      // all configured connection attempts after the cooldown expires.
      --attempt;
    } else if (shouldSkipWeakStoredIdentityCandidate(info, firstBootPairing)) {
      Serial.printf("BLE: weak stored-address candidate addr=%s rssi=%d has_name=%d services=%d%d%d%d; waiting for camera power on\n",
                    info.address.c_str(),
                    info.rssi,
                    info.name.length() > 0 ? 1 : 0,
                    info.hasInfoService ? 1 : 0,
                    info.hasCameraService ? 1 : 0,
                    info.hasShootingService ? 1 : 0,
                    info.hasControlService ? 1 : 0);
    } else {
      const String connectedName = displayBleName(info);

      // The first scan may intentionally spend the full discovery window
      // choosing a camera. Once selected, remember it for this boot attempt so
      // retries stop as soon as that address advertises again.
      retryPreferredAddress = info.address;
      if (info.name.length() > 0) {
        retryPreferredName = info.name;
      }

      setCameraFlowState(CameraFlowState::ConnectingBle, "BLE scan candidate");
      RicohBleConnectOptions options;
      options.timeoutMs = BLE_CONNECT_TIMEOUT_MS;
      const bool bonded = bleCamera.isBonded(info);
      const bool useBondedFastSecurity = bonded && !bondedFastSecurityAttempted;
      if (useBondedFastSecurity) {
        bondedFastSecurityAttempted = true;
      }
      options.securityWaitMs = useBondedFastSecurity
                                 ? RICOH_BLE_BONDED_SECURITY_WAIT_MS
                                 : RICOH_BLE_SECURITY_WAIT_MS;
      options.preConnectDelayMs = BLE_SCAN_TO_CONNECT_DELAY_MS;
      options.exchangeMtu = false;

      Serial.printf("BLE: connect policy bonded=%d fast_security=%d security_wait=%lums\n",
                    bonded ? 1 : 0,
                    useBondedFastSecurity ? 1 : 0,
                    static_cast<unsigned long>(options.securityWaitMs));

      const rvf::Result connectResult = bleCamera.connectCamera(info, options);
      if (resetBlePairingIfRequested()) {
        return false;
      }
      if (connectResult.ok()) {
        saveConnectedBleIdentity(connectedName, info);
        setCameraFlowState(CameraFlowState::BleReady, "BLE connected");
        return true;
      }

      Serial.printf("BLE: connect attempt %u/%u failed: %s\n",
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(attempts),
                    bleCamera.lastError().c_str());
      if (shouldBackoffAfterStoredIdentityConnectFailure(info, bleCamera.lastError(), firstBootPairing)) {
        deferStoredIdentityPowerProbeAfterConnectFailure(bleCamera.lastError());
      }
      consumeCameraPowerOffDisconnectAfterReady("BLE connect failed");
      bleCamera.disconnect();
      if (bleCamera.lastFailureWasResourceExhausted()) {
        Serial.println("BLE: host resources exhausted during connect; rebuild stack objects before retry");
        bleCamera.resetStack(true);
        consecutiveConnectFailures = 0;
        skipRetryDelay = true;
      } else {
        consecutiveConnectFailures++;
        if (BLE_STACK_RESET_AFTER_FAILURES > 0 && consecutiveConnectFailures >= BLE_STACK_RESET_AFTER_FAILURES) {
          bleCamera.resetStack();
          consecutiveConnectFailures = 0;
          skipRetryDelay = true;
        }
      }
    }

    if (attempt < attempts && !skipRetryDelay) {
      delay(BLE_CONNECT_RETRY_DELAY_MS);
      yield();
      if (resetBlePairingIfRequested()) {
        return false;
      }
    }
  }

  setCameraFlowState(CameraFlowState::BleScan, "BLE attempts exhausted");
  bleCamera.resetStack();
  return false;
}

bool bleStillConnectedForWifi() {
  return bleCamera.isConnected();
}

bool wifiStillConnectedForController() {
  return grWifi.isConnected();
}

bool connectWifiFromProfile(bool forceStatus, bool requireBleAnchor = false, uint32_t totalTimeoutMs = WIFI_CONNECT_TIMEOUT_MS, bool allowFullScanFallback = true) {
  (void)forceStatus;
  wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
  Serial.printf("WiFi: connecting ssid='%s' bssid='%s' channel=%u freq=%u\n",
                cameraProfile.wifi.ssid.c_str(),
                cameraProfile.wifi.bssid.c_str(),
                static_cast<unsigned>(cameraProfile.wifi.channel),
                static_cast<unsigned>(cameraProfile.wifi.frequencyMhz));

  bool connected = false;
  const uint8_t channel = cameraProfile.wifi.channel;
  const uint32_t hintTimeout = WIFI_CHANNEL_HINT_CONNECT_TIMEOUT_MS < totalTimeoutMs
                                 ? WIFI_CHANNEL_HINT_CONNECT_TIMEOUT_MS
                                 : totalTimeoutMs;

  if (channel != 0) {
    connected = wifiPreview.connectWifi(cameraProfile.wifi.ssid.c_str(),
                                        cameraProfile.wifi.passphrase.c_str(),
                                        cameraProfile.wifi.bssid.c_str(),
                                        channel,
                                        hintTimeout,
                                        requireBleAnchor ? bleStillConnectedForWifi : nullptr)
                  .ok();

    if (allowFullScanFallback && !connected && (!requireBleAnchor || bleStillConnectedForWifi())) {
      uint32_t fallbackTimeout = totalTimeoutMs > hintTimeout ? totalTimeoutMs - hintTimeout : totalTimeoutMs;
      if (fallbackTimeout < 1000) {
        fallbackTimeout = totalTimeoutMs;
      }
      Serial.printf("WiFi: channel hint %u failed; fallback to full scan timeout=%lums\n",
                    static_cast<unsigned>(channel),
                    static_cast<unsigned long>(fallbackTimeout));
      connected = wifiPreview.connectWifi(cameraProfile.wifi.ssid.c_str(),
                                          cameraProfile.wifi.passphrase.c_str(),
                                          cameraProfile.wifi.bssid.c_str(),
                                          0,
                                          fallbackTimeout,
                                          requireBleAnchor ? bleStillConnectedForWifi : nullptr)
                    .ok();
    }
  } else {
    connected = wifiPreview.connectWifi(cameraProfile.wifi.ssid.c_str(),
                                        cameraProfile.wifi.passphrase.c_str(),
                                        cameraProfile.wifi.bssid.c_str(),
                                        0,
                                        totalTimeoutMs,
                                        requireBleAnchor ? bleStillConnectedForWifi : nullptr)
                  .ok();
  }

  if (connected) {
    uiLocalIp = grWifi.localIPString();
    Serial.printf("WiFi: connected ip=%s rssi=%ld\n", uiLocalIp.c_str(), static_cast<long>(grWifi.rssi()));
    return true;
  }

  Serial.printf("WiFi: connect failed status=%s\n", grWifi.statusText().c_str());
  return false;
}

bool fetchCameraPropsForController() {
  const rvf::Result propsResult = wifiPreview.fetchProps(pendingCameraProps, PROPS_TIMEOUT_MS);
  if (propsResult.failed()) {
    return false;
  }
  return true;
}

void onHttpProbeFailedForController() {
  Serial.printf("HTTP: props probe failed: %s\n", wifiPreview.lastError().c_str());
}

void onHttpProbeSucceededForController() {
  cameraProps = pendingCameraProps;
  lastPropsAt = millis();
  Serial.printf("HTTP: camera ready model='%s' battery='%s'\n", cameraProps.model.c_str(), cameraProps.battery.c_str());
}

bool openLiveViewForController() {
  const rvf::Result previewResult = wifiPreview.startPreview();
  if (previewResult.failed()) {
    return false;
  }
  return true;
}

void onLiveViewOpenFailedForController() {
  Serial.printf("LiveView: open failed: %s\n", wifiPreview.lastError().c_str());
}

void onLiveViewOpenedForController() {
  lastFrameAt = millis();
  lastLiveViewActivityAt = lastFrameAt;
  previewFrameBuffer.resetRuntimeStats();
  Serial.println("LiveView: connected");
}

bool cameraRecoveryInProgressForController() {
  return cameraRecoveryInProgress;
}

uint32_t lastCameraRecoveryAtForController() {
  return lastCameraRecoveryAt;
}

void setLastCameraRecoveryAtForController(uint32_t timestampMs) {
  lastCameraRecoveryAt = timestampMs;
}

void setCameraRecoveryInProgressForController(bool inProgress) {
  cameraRecoveryInProgress = inProgress;
}

void shortRecoveryDelayForController() {
  delay(100);
}

void onRecoveryGuardBlockedForController() {
  lastFrameAt = millis();
  lastCameraRecoveryAt = millis();
  cameraRecoveryInProgress = false;
}

void onRecoveryFinishedForController(bool recovered) {
  lastFrameAt = millis();
  lastCameraRecoveryAt = recovered ? millis() : 0;
  cameraRecoveryInProgress = false;
}

void requestManualCameraWakeForController(const char* source) {
  requestManualCameraWake(source);
}

bool shutterReadyForController() {
  return bleCamera.shutterReady();
}

bool shootAutofocusForController() {
  const rvf::Result shootResult = bleCamera.shoot(true);
  if (shootResult.failed()) {
    Serial.printf("Button A: BLE shutter failed: %s\n", bleCamera.lastError().c_str());
  }
  return shootResult.ok();
}

bool previewKeptAfterShutterFailureForController() {
  return appController.isPreviewActive() && grWifi.isConnected() && wifiPreview.isPreviewRunning();
}

bool previewStreamRunningForController() {
  return wifiPreview.isPreviewRunning();
}

bool readAndProcessLiveViewFrameForController() {
  const int readLen = wifiPreview.readFrame(streamReadBuffer, sizeof(streamReadBuffer));
  if (readLen > 0) {
    lastLiveViewActivityAt = millis();
    wifiPreview.processFrameData(streamReadBuffer, static_cast<size_t>(readLen));
  } else if (readLen < 0) {
    Serial.printf("LiveView: read failed: %s\n", wifiPreview.lastError().c_str());
    return false;
  }
  return true;
}

void logPreviewStatsForController() {
  wifiPreview.logStatsIfDue(millis());
  previewFrameBuffer.syncStreamStats(mjpeg.frames(), mjpeg.droppedFrames(), mjpeg.currentLength());
  previewFrameBuffer.logStatsIfDue(millis());
}

uint32_t lastFrameAtForController() {
  return lastFrameAt;
}

uint32_t lastLiveViewActivityAtForController() {
  return lastLiveViewActivityAt;
}

bool reasonRequiresBleRescan(rvf::RecoveryCause cause) {
  return !bleCamera.isConnected() ||
         cause == rvf::RecoveryCause::BleDisconnected ||
         cause == rvf::RecoveryCause::ShutterBleNotReady;
}

void disconnectWifiLiveViewToBleReady(const char* reason) {
  closeLiveView(reason != nullptr ? reason : "BLE_READY reset");
  wifiPreview.disconnectWifi();
  if (bleCamera.isConnected()) {
    setCameraFlowState(CameraFlowState::BleReady, reason != nullptr ? reason : "BLE still connected");
  } else {
    setCameraFlowState(CameraFlowState::BleScan, reason != nullptr ? reason : "BLE lost");
  }
}

void disconnectAllTransportsToBleScan(const char* reason) {
  closeLiveView(reason != nullptr ? reason : "flow reset");
  wifiPreview.disconnectWifi();
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, reason);
}

void resetBleStackBeforeScanAfterLinkLoss(const char* reason) {
  Serial.printf("BLE recovery: reset stack (%s)\n", reason != nullptr ? reason : "link lost");
  bleCamera.resetStack();
  delay(BLE_RECOVERY_STACK_RESET_GRACE_MS);
  yield();
}

void delayAndYield(uint32_t delayMs) {
  delay(delayMs);
  yield();
}

void disconnectWifiForController() {
  wifiPreview.disconnectWifi();
}

void clearBleDisconnectReasonForController() {
  bleCamera.clearDisconnectReason();
}

bool connectCachedWifiFromProfileForController() {
  if (WIFI_CACHED_CONNECT_GRACE_MS > 0) {
    Serial.printf("WiFi cache: waiting %lums for camera AP before cached connect\n",
                  static_cast<unsigned long>(WIFI_CACHED_CONNECT_GRACE_MS));
    delay(WIFI_CACHED_CONNECT_GRACE_MS);
    yield();
  }
  Serial.printf("WiFi cache: trying cached params ssid='%s' bssid='%s' channel=%u short_timeout=%lums\n",
                cameraProfile.wifi.ssid.c_str(),
                cameraProfile.wifi.bssid.c_str(),
                static_cast<unsigned>(cameraProfile.wifi.channel),
                static_cast<unsigned long>(WIFI_CACHED_CONNECT_TIMEOUT_MS));
  return connectWifiFromProfile(true, true, WIFI_CACHED_CONNECT_TIMEOUT_MS, false);
}

void onCachedWifiConnectedForController() {
  saveConnectedWifiBssidToCache("cached connect");
  scheduleWifiCacheRefresh();
}

void onCachedWifiConnectFailedForController() {
  Serial.println("WiFi cache: cached connect failed; reading fresh BLE params");
}

bool readFreshWifiCredentialsForController() {
  const rvf::Result wifiCredentialsResult = bleCamera.waitForWifiCredentials(pendingFreshWifiCredentials, RICOH_BLE_WIFI_CREDENTIAL_WAIT_MS);
  if (wifiCredentialsResult.failed()) {
    return false;
  }
  return true;
}

void applyFreshWifiCredentialsForController() {
  applyBleWifiCredentials(pendingFreshWifiCredentials, "fresh BLE", false);
}

bool connectFreshWifiFromProfileForController() {
  return connectWifiFromProfile(true, true);
}

void onFreshWifiConnectedForController() {
  saveConnectedWifiBssidToCache("fresh BLE connect");
}

rvf::AppFlowActions makeAppFlowActions() {
  rvf::AppFlowActions actions;
  actions.cameraSleepGuardBlocksFlow = cameraSleepGuardBlocksFlow;
  actions.runBleDiscovery = runBleDiscoveryAtBoot;
  actions.cameraSleepGuardActive = cameraSleepGuardActive;
  actions.activateCameraWifiOverBle = activateCameraWifiOverBle;
  actions.disconnectWifi = disconnectWifiForController;
  actions.clearBleDisconnectReason = clearBleDisconnectReasonForController;
  actions.hasUsableCachedWifiCredentials = hasUsableCachedWifiCredentials;
  actions.connectCachedWifiFromProfile = connectCachedWifiFromProfileForController;
  actions.onCachedWifiConnected = onCachedWifiConnectedForController;
  actions.onCachedWifiConnectFailed = onCachedWifiConnectFailedForController;
  actions.readFreshWifiCredentials = readFreshWifiCredentialsForController;
  actions.applyFreshWifiCredentials = applyFreshWifiCredentialsForController;
  actions.connectFreshWifiFromProfile = connectFreshWifiFromProfileForController;
  actions.onFreshWifiConnected = onFreshWifiConnectedForController;
  actions.isWifiConnected = wifiStillConnectedForController;
  actions.fetchCameraProps = fetchCameraPropsForController;
  actions.onHttpProbeSucceeded = onHttpProbeSucceededForController;
  actions.onHttpProbeFailed = onHttpProbeFailedForController;
  actions.openLiveView = openLiveViewForController;
  actions.onLiveViewOpened = onLiveViewOpenedForController;
  actions.onLiveViewOpenFailed = onLiveViewOpenFailedForController;
  actions.isBleConnected = bleStillConnectedForWifi;
  actions.consumePowerOffNotification = consumeCameraPowerOffNotification;
  actions.consumePowerOffDisconnect = consumeCameraPowerOffDisconnect;
  actions.disconnectWifiLiveViewToBleReady = disconnectWifiLiveViewToBleReady;
  actions.disconnectAllTransportsToBleScan = disconnectAllTransportsToBleScan;
  actions.cameraRecoveryInProgress = cameraRecoveryInProgressForController;
  actions.setCameraRecoveryInProgress = setCameraRecoveryInProgressForController;
  actions.reasonRequiresBleRescan = reasonRequiresBleRescan;
  actions.resetBleStackBeforeScanAfterLinkLoss = resetBleStackBeforeScanAfterLinkLoss;
  actions.shortRecoveryDelay = shortRecoveryDelayForController;
  actions.onRecoveryGuardBlocked = onRecoveryGuardBlockedForController;
  actions.onRecoveryFinished = onRecoveryFinishedForController;
  actions.requestManualCameraWake = requestManualCameraWakeForController;
  actions.shutterReady = shutterReadyForController;
  actions.shootAutofocus = shootAutofocusForController;
  actions.previewKeptAfterShutterFailure = previewKeptAfterShutterFailureForController;
  actions.previewStreamRunning = previewStreamRunningForController;
  actions.readAndProcessLiveViewFrame = readAndProcessLiveViewFrameForController;
  actions.logPreviewStats = logPreviewStatsForController;
  actions.delayAndYield = delayAndYield;
  actions.lastFrameAt = lastFrameAtForController;
  actions.lastLiveViewActivityAt = lastLiveViewActivityAtForController;
  actions.lastCameraRecoveryAt = lastCameraRecoveryAtForController;
  actions.setLastCameraRecoveryAt = setLastCameraRecoveryAtForController;
  actions.liveviewEnabled = liveviewEnabled;
  actions.wifiOpenAttempts = WIFI_OPEN_ATTEMPTS;
  actions.retryDelayMs = BLE_CONNECT_RETRY_DELAY_MS;
  actions.bleScanRetryIntervalMs = BLE_SCAN_RETRY_INTERVAL_MS;
  actions.liveViewStallTimeoutMs = LIVEVIEW_STALL_TIMEOUT_MS;
  return actions;
}

void refreshPropsIfDue(bool force = false) {
  const uint32_t now = millis();
  if (!force && cameraProps.ok && (now - lastPropsAt) < PROPS_REFRESH_INTERVAL_MS) {
    return;
  }
  if (!grWifi.isConnected()) {
    return;
  }

  CameraProps nextProps;
  if (wifiPreview.fetchProps(nextProps, PROPS_TIMEOUT_MS).ok()) {
    cameraProps = nextProps;
    lastPropsAt = now;
  } else if (force) {
    Serial.printf("HTTP: props refresh failed: %s\n", wifiPreview.lastError().c_str());
  }
}

void updateFps() {
  const uint32_t now = millis();
  fpsWindowFrames++;
  if (fpsWindowStart == 0) {
    fpsWindowStart = now;
  }
  const uint32_t elapsed = now - fpsWindowStart;
  if (elapsed >= 1000) {
    currentFps = (fpsWindowFrames * 1000.0f) / static_cast<float>(elapsed);
    fpsWindowFrames = 0;
    fpsWindowStart = now;
  }
}

void onJpegFrame(const uint8_t* data, size_t len, void*) {
  lastFrameAt = millis();
  decodedFrames++;
  updateFps();
  previewFrameBuffer.recordFrame(len);

  const uint32_t renderStartMs = millis();
  if (!decoder.drawFrame(&displaySurface.canvas(), data, len)) {
    Serial.printf("JPEG decode failed len=%u err=%s\n", static_cast<unsigned>(len), decoder.lastError().c_str());
    wifiPreview.recordRenderedFrame(decoder.lastDecodeMs(), millis() - renderStartMs);
  } else {
    const rvf::ui::UiModel model = uiPresenter.present(makeUiRuntimeSnapshot());
    if (model.screen == rvf::ui::UiScreen::LiveView) {
      uiManager.renderLiveViewOverlay(model);
      displaySurface.present();
    } else {
      // A semantic transition can race the final buffered JPEG. In that case
      // keep the status/error page authoritative and still present only once.
      uiManager.update(model, millis(), true);
    }
    wifiPreview.recordRenderedFrame(decoder.lastDecodeMs(), millis() - renderStartMs);
  }
  lastFrameAt = millis();
}

void resetBlePairingFromKey2() {
  static bool resettingPairing = false;
  if (resettingPairing) {
    return;
  }
  resettingPairing = true;
  cameraRecoveryInProgress = true;

  Serial.println("BLE pairing reset: Button B / KEY2 long press");
  publishLocalAppEvent(rvf::AppEventType::PairingResetStarted, 0, "Clearing BLE pairing");

  closeLiveView("Reset pairing");
  wifiPreview.disconnectWifi();
  bleCamera.disconnect();
  clearCameraSleepGuard("Reset pairing");

  const bool profileCleared = profileStore.clearBlePairing();
  Serial.printf("Profile: BLE pairing keys cleared ok=%d\n", profileCleared ? 1 : 0);

  const String cameraIp = cameraProfile.wifi.cameraIp.length() > 0 ? cameraProfile.wifi.cameraIp : String(GR_HOST);
  cameraProfile = CameraProfile{};
  cameraProfile.wifi.cameraIp = cameraIp;
  wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
  pendingFreshWifiCredentials = RicohBleWifiCredentials{};
  wifiCacheRefreshPending = false;
  cameraProps = CameraProps{};
  pendingCameraProps = CameraProps{};
  lastPropsAt = 0;
  liveviewEnabled = true;

  const rvf::Result deleteBondsResult = bleCamera.deleteAllBonds();
  if (deleteBondsResult.failed()) {
    Serial.printf("BLE pairing reset: NimBLE bond delete failed: %s\n", bleCamera.lastError().c_str());
  }
  bleCamera.clearDisconnectReason();
  bleCamera.resetStack(true);

  setCameraFlowState(CameraFlowState::BleScan, "Reset pairing");
  lastFrameAt = millis();
  lastLiveViewActivityAt = lastFrameAt;
  lastCameraRecoveryAt = 0;
  cameraRecoveryInProgress = false;
  resettingPairing = false;
  publishLocalAppEvent(rvf::AppEventType::PairingResetCompleted, 0, "Pairing mode");
}

void requestManualCameraWake(const char* source) {
  const char* wakeSource = source != nullptr ? source : "manual retry";
  clearCameraSleepGuard(wakeSource);
  liveviewEnabled = true;
  closeLiveView(wakeSource);
  wifiPreview.disconnectWifi();
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, wakeSource);

  Serial.printf("BLE guard: manual retry BLE stack rebuild (%s)\n", wakeSource);
  bleCamera.resetStack(true);
  delay(BLE_MANUAL_WAKE_REINIT_SETTLE_MS);
  yield();

  const bool online = appController.runCameraFlowOnce(makeAppFlowActions(), millis());
  lastFrameAt = millis();
  lastCameraRecoveryAt = online ? millis() : 0;
}

void shutdownStickS3() {
  static bool shuttingDown = false;
  if (shuttingDown) {
    return;
  }
  shuttingDown = true;
  cameraRecoveryInProgress = true;

  Serial.println("Power: shutdown requested");
  closeLiveView("power off");
  wifiPreview.disconnectWifi();
  bleCamera.disconnect();

  publishLocalAppEvent(rvf::AppEventType::ShutdownRequested, 0, "Release PWR...");
  const uint32_t startMs = millis();
  while (isStickPowerButtonPressed() && (millis() - startMs) < POWER_BUTTON_RELEASE_WAIT_MS) {
    M5.update();
    delay(20);
    yield();
  }

  publishLocalAppEvent(rvf::AppEventType::ShutdownRequested, 0, "Power off...");
  delay(200);
  if (stickPowerReady) {
    Serial.println("Power: M5PM1 shutdown command");
    Serial.flush();
    const m5pm1_err_t err = stickPower.shutdown();
    if (err != M5PM1_OK) {
      Serial.printf("Power: M5PM1 shutdown failed err=%d; fallback to M5Unified\n", static_cast<int>(err));
      Serial.flush();
    }
    delay(300);
  }
  M5.Power.powerOff();
  while (true) {
    delay(1000);
  }
}

void handleButtons() {
  const ButtonEvents events = buttons.poll();
  const rvf::UserCommand command = rvf::ButtonInput::commandFromEvents(events);

  if (command == rvf::UserCommand::PowerOff || pollStickPowerHold()) {
    shutdownStickS3();
    return;
  }

  if (command == rvf::UserCommand::ResetPairing) {
    resetBlePairingFromKey2();
    return;
  }

  if (command == rvf::UserCommand::Shoot) {
    appController.handleUserCommand(makeAppFlowActions(), command);
  }
}

const char* appEventTypeName(rvf::AppEventType type) {
  switch (type) {
    case rvf::AppEventType::None: return "None";
    case rvf::AppEventType::StateChanged: return "StateChanged";
    case rvf::AppEventType::BootCompleted: return "BootCompleted";
    case rvf::AppEventType::BleScanStarted: return "BleScanStarted";
    case rvf::AppEventType::BleDeviceFound: return "BleDeviceFound";
    case rvf::AppEventType::BleConnected: return "BleConnected";
    case rvf::AppEventType::BleDisconnected: return "BleDisconnected";
    case rvf::AppEventType::CameraPowerOn: return "CameraPowerOn";
    case rvf::AppEventType::CameraPowerOff: return "CameraPowerOff";
    case rvf::AppEventType::CameraPowerUnknown: return "CameraPowerUnknown";
    case rvf::AppEventType::WifiActivationRequested: return "WifiActivationRequested";
    case rvf::AppEventType::WifiConnected: return "WifiConnected";
    case rvf::AppEventType::WifiDisconnected: return "WifiDisconnected";
    case rvf::AppEventType::HttpProbeSucceeded: return "HttpProbeSucceeded";
    case rvf::AppEventType::HttpProbeFailed: return "HttpProbeFailed";
    case rvf::AppEventType::PreviewStarted: return "PreviewStarted";
    case rvf::AppEventType::PreviewStopped: return "PreviewStopped";
    case rvf::AppEventType::PreviewTimeout: return "PreviewTimeout";
    case rvf::AppEventType::ButtonShortPress: return "ButtonShortPress";
    case rvf::AppEventType::ButtonLongPress: return "ButtonLongPress";
    case rvf::AppEventType::ShutterPressed: return "ShutterPressed";
    case rvf::AppEventType::ShutterSucceeded: return "ShutterSucceeded";
    case rvf::AppEventType::ShutterFailed: return "ShutterFailed";
    case rvf::AppEventType::RecoveryStarted: return "RecoveryStarted";
    case rvf::AppEventType::RecoverySucceeded: return "RecoverySucceeded";
    case rvf::AppEventType::RecoveryFailed: return "RecoveryFailed";
    case rvf::AppEventType::PairingResetStarted: return "PairingResetStarted";
    case rvf::AppEventType::PairingResetCompleted: return "PairingResetCompleted";
    case rvf::AppEventType::ShutdownRequested: return "ShutdownRequested";
    case rvf::AppEventType::ErrorRaised: return "ErrorRaised";
  }
  return "Unknown";
}

rvf::SystemHealthSnapshot makeSystemHealthSnapshot() {
  rvf::SystemHealthSnapshot snapshot;
  snapshot.appState = appController.state();
  snapshot.bleConnected = bleCamera.isConnected();
  snapshot.wifiConnected = grWifi.isConnected();
  snapshot.previewRunning = wifiPreview.isPreviewRunning();
  snapshot.liveviewEnabled = liveviewEnabled;
  snapshot.cameraSleepGuardActive = cameraSleepGuardActive();
  snapshot.cameraRecoveryInProgress = cameraRecoveryInProgress;
  snapshot.lastFrameAt = lastFrameAt;
  snapshot.lastLiveViewActivityAt = lastLiveViewActivityAt;
  snapshot.liveViewStallTimeoutMs = LIVEVIEW_STALL_TIMEOUT_MS;
  return snapshot;
}

void serviceSystemSupervisor(uint32_t nowMs) {
  rvf::AppMessage message;
  if (!systemSupervisor.check(nowMs, makeSystemHealthSnapshot(), message)) {
    return;
  }

  Serial.printf("Supervisor: event=%s state=%s code=%d detail=%s\n",
                appEventTypeName(message.type),
                rvf::appStateName(appController.state()),
                message.code,
                message.detail != nullptr ? message.detail : "");
}

void updateStatusUiIfDue() {
  const uint32_t now = millis();
  if ((now - lastStatusAt) < UI_STATUS_INTERVAL_MS) {
    return;
  }
  lastStatusAt = now;
  renderCurrentUi(false);
}

void runAppTick() {
  const uint32_t now = millis();
  const rvf::AppTickPlan tickPlan = appController.planTick(now);

  if (tickPlan.handleButtons) {
    handleButtons();
  }
  const uint32_t controllerNow = millis();
  const rvf::AppFlowActions actions = makeAppFlowActions();
  if (tickPlan.serviceCameraFlow) {
    appController.serviceCameraFlowIfNeeded(actions, controllerNow);
  }
  if (tickPlan.monitorWifi) {
    appController.monitorWifi(actions);
  }
  if (tickPlan.refreshProps) {
    refreshPropsIfDue();
  }
  if (tickPlan.monitorLiveView) {
    appController.monitorLiveView(actions, controllerNow);
  }
  serviceSystemSupervisor(millis());
  if (tickPlan.refreshWifiCache) {
    refreshWifiCacheIfDue();
  }
  updateStatusUiIfDue();
  delay(1);
}

}  // namespace

void setup() {
  Serial.begin(rvf::AppConfig::SerialPort::kBaud);
  appController.begin(CameraFlowState::Booting);
  systemSupervisor.begin(millis());

  if (!displaySurface.begin()) {
    LOGLINE_E("DISPLAY", "Failed to allocate display canvas");
    while (true) {
      delay(1000);
    }
  }
  uiSurfaceReady = true;
  rvf::AppEventSink eventSink;
  eventSink.publish = onAppEventForUi;
  appController.setEventSink(eventSink);
  uiDetail = rvf::AppConfig::Ui::kBootMessage;
  renderCurrentUi(true);
  waitForSerialConsole();

  beginStickPower();
  buttons.begin();
  ricohBle.setServiceCallback(serviceButtonsDuringBleOperation);
  if (!decoder.begin(displaySurface.width(), displaySurface.height())) {
    LOGLINE_E("JPEG", "Failed to initialize JPEG decoder surface");
    renderErrorUi("JPEG decoder init failed");
    while (true) {
      delay(1000);
    }
  }
  grWifi.begin();

  applyDefaultProfile();

  if (!psramFound()) {
    LOGLINE_W("MEM", "PSRAM not found; JPEG buffer allocation may fail");
    renderErrorUi("PSRAM not found");
  }

  rvf::PreviewFrameBufferStorage frameBufferStorage = rvf::PreviewFrameBufferStorage::Psram;
  frameBuffer = static_cast<uint8_t*>(heap_caps_malloc(rvf::AppConfig::Buffer::kFrameBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frameBuffer == nullptr) {
    frameBufferStorage = rvf::PreviewFrameBufferStorage::InternalRam;
    frameBuffer = static_cast<uint8_t*>(heap_caps_malloc(rvf::AppConfig::Buffer::kFrameBufferSize, MALLOC_CAP_8BIT));
  }
  if (frameBuffer == nullptr) {
    LOGLINE_E("MEM", "Failed to allocate JPEG frame buffer");
    renderErrorUi("Frame buffer alloc failed");
    while (true) {
      delay(1000);
    }
  }

  if (!previewFrameBuffer.attach(frameBuffer, rvf::AppConfig::Buffer::kFrameBufferSize, frameBufferStorage)) {
    LOGLINE_E("FRAME", "Failed to attach JPEG frame buffer");
    renderErrorUi("Frame buffer attach failed");
    while (true) {
      delay(1000);
    }
  }
  mjpeg.begin(previewFrameBuffer.data(), previewFrameBuffer.capacity(), onJpegFrame, nullptr);

  appController.begin(CameraFlowState::BleScan);
  uiDetail = "Starting camera flow";
  uiManager.invalidate();
  renderCurrentUi(true);
  setupCameraFlowActive = true;
  const bool startupOnline = appController.runCameraFlowOnce(makeAppFlowActions(), millis());
  setupCameraFlowActive = false;
  lastCameraRecoveryAt = startupOnline ? millis() : 0;
  lastFrameAt = millis();
  lastLiveViewActivityAt = lastFrameAt;
  lastStatusAt = 0;
}

void loop() {
  runAppTick();
}
