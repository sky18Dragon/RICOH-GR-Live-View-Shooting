#include "RicohDriver.h"

#include <Arduino.h>

#include "../../ble_disconnect_policy.h"
#include "../../ble_link_mode_policy.h"
#include "../../ble_pairing_policy.h"
#include "../../ble_reconnect_policy.h"
#include "../../camera_identity.h"
#include "../../camera_sleep_policy.h"
#include "../../config.h"
#include "../../app/AppController.h"
#include "../../core/Logger.h"
#include "../../gr_api.h"
#include "../../gr2_provisioning_portal.h"
#include "../../gr_wifi.h"
#include "../../liveview_focus_policy.h"
#include "../../liveview_reso_policy.h"
#include "../../mjpeg_stream.h"
#include "../../ricoh_ble_client.h"
#include "../../services/BleCameraService.h"
#include "../../services/CameraPowerPolicy.h"
#include "../../services/PreviewFrameBuffer.h"
#include "../../services/WifiPreviewService.h"
#include "../../ui/UserCommand.h"

// The Ricoh workflow moved here verbatim from main.cpp (multi-brand plan,
// stage 2). It still runs on file-scope state; splitting it per model is
// stage 6.

namespace {

using CameraFlowState = rvf::AppState;

rvf::ICameraHost* g_host = nullptr;
CameraProfileStore* g_profileStore = nullptr;

GrWifi grWifi;
GrApi grApi;
Gr2ProvisioningPortal gr2Provisioning;
MjpegStream mjpeg;
CameraProfile cameraProfile;
RicohBleClient ricohBle;
rvf::BleCameraService bleCamera(ricohBle);
rvf::CameraPowerPolicy cameraPowerPolicy;
rvf::WifiPreviewService wifiPreview(grWifi, grApi, mjpeg);
rvf::PreviewFrameBuffer previewFrameBuffer;
rvf::AppController appController(CameraFlowState::BleScan);
rvf::AppTickPlan tickPlan;
uint8_t streamReadBuffer[STREAM_READ_BUFFER_SIZE];
CameraProps cameraProps;
RicohBleWifiCredentials pendingFreshWifiCredentials;
// Model the user confirmed in the pairing guide; first-boot pairing only
// accepts a camera of this generation.
RicohProtocolGeneration selectedPairingGeneration = RicohProtocolGeneration::Gr3Family;

bool liveviewEnabled = true;
uint32_t lastFrameAt = 0;
uint32_t lastLiveViewActivityAt = 0;
uint32_t lastCameraRecoveryAt = 0;
bool cameraRecoveryInProgress = false;
bool gr2ProvisioningPreparing = false;
bool setupCameraFlowActive = false;
bool pairingModelConfirmed = false;
bool key2PairingResetRequested = false;
RicohCameraPowerState cameraPowerState = RicohCameraPowerState::Unknown;
RicohCameraOperationMode cameraOperationMode = RicohCameraOperationMode::Unknown;
bool cameraWifiStartupModeProbe = false;
uint32_t lastPropsAt = 0;
uint32_t deferredPropsRefreshAfter = 0;
String liveViewSavedFocusSetting;
uint8_t liveViewStallRecycleCount = 0;
bool liveViewResolutionSelected = false;
int cameraBleBatteryShown = -1;
bool uiResettingPairing = false;
uint32_t lastStatusUiUpdateAt = 0;

// Camera power-off protection: blocks auto wake until the user presses a key.
struct CameraSleepGuardState {
  bool autoWakeBlocked = false;
  bool blockedFlowLogged = false;
  int disconnectReason = 0;
  uint32_t powerProbeBackoffUntil = 0;
  uint32_t enteredAt = 0;
};
CameraSleepGuardState sleepGuard;

// BLE connection-parameter mode applied while the preview streams.
struct BleLinkModeState {
  BleLinkMode applied = BleLinkMode::Normal;
  bool previewStreaming = false;
  uint32_t previewStreamingSinceMs = 0;
  uint32_t retryAfterMs = 0;
  uint32_t connectionSerial = 0;
};
BleLinkModeState bleLinkMode;

// Periodic LINK: log of BLE RSSI during preview.
struct LinkStatsState {
  uint32_t lastLogMs = 0;
  uint32_t rssiLastSampleMs = 0;
  int32_t rssiSum = 0;
  int32_t rssiMin = 0;
  uint16_t rssiSamples = 0;
};
LinkStatsState linkStats;

// Waits for the camera's BLE shot notification after a BLE shutter write.
struct ShotConfirmState {
  bool pending = false;
  uint32_t startedAt = 0;
  uint32_t eventSequenceBeforeShot = 0;
};
ShotConfirmState shotConfirm;

// Deferred re-read of the camera Wi-Fi credentials into the NVS cache.
struct WifiCacheRefreshState {
  bool pending = false;
  uint32_t after = 0;
};
WifiCacheRefreshState wifiCacheRefresh;

void requestManualCameraWake(const char* source);
void unparkBleIfNeeded(const char* reason);
void resetBlePairingFromKey2();
rvf::AppFlowActions makeAppFlowActions();
bool beginSelectedCameraPairing(RicohProtocolGeneration generation);

// --- Host glue: the moved code keeps calling these names. ---

void showStatusIfChanged(const String& line1,
                         const String& line2 = String(),
                         const String& line3 = String(),
                         const String& line4 = String(),
                         bool force = false) {
  g_host->showStatus(line1, line2, line3, line4, force);
}

void updateUi() {
  g_host->refreshUi();
}

void requestPairingGuide(RicohProtocolGeneration generation) {
  g_host->requestModelSelection(static_cast<rvf::CameraModelId>(generation));
}

bool serviceButtonsDuringBleOperation() {
  if (!g_host->pollInputDuringBlockingOperation()) {
    return false;
  }

  key2PairingResetRequested = true;
  Serial.println("BLE pairing reset: requested during BLE operation");
  return true;
}

int32_t pollPasskeyButtonEntry(RicohPasskeyPollAction action) {
  bool resetRequested = false;
  const int32_t result =
      g_host->pollPasskeyEntry(static_cast<rvf::PasskeyPollAction>(action), resetRequested);
  if (resetRequested) {
    key2PairingResetRequested = true;
  }
  return result;
}

bool isGr2WifiOnlyProfile() {
  return cameraProfile.protocolGenerationKnown &&
         cameraProfile.protocolGeneration == RicohProtocolGeneration::Gr2Family;
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

void restoreLiveViewFocusSettingIfNeeded() {
  if (liveViewSavedFocusSetting.length() == 0) {
    return;
  }
  // Spot is the landscape-session AF mode. Recycled MJPEG after a shot, or a
  // landscape recover that will open LiveView again, must not PUT the old
  // mode back while the camera is still writing the file (GR III 412).
  if (appController.previewRequested()) {
    return;
  }
  const String previous = liveViewSavedFocusSetting;
  liveViewSavedFocusSetting = "";
  if (!grWifi.isConnected()) {
    Serial.printf("HTTP: focusSetting restore skipped; WiFi down previous=%s\n", previous.c_str());
    return;
  }
  if (grApi.setFocusSetting(previous.c_str(), PROPS_TIMEOUT_MS)) {
    Serial.printf("HTTP: restored focusSetting=%s\n", previous.c_str());
    cameraProps.focusSetting = previous;
    return;
  }
  Serial.printf("HTTP: focusSetting restore skipped: %s\n", grApi.lastError().c_str());
}

void closeLiveView(const char* reason) {
  if (wifiPreview.isPreviewRunning()) {
    Serial.printf("LiveView: closing (%s)\n", reason != nullptr ? reason : "reset");
  }
  wifiPreview.stopPreview();
  deferredPropsRefreshAfter = 0;
  restoreLiveViewFocusSettingIfNeeded();
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

bool isCameraSleepDisconnectReason(int reason) {
  return ricohBleDisconnectMayIndicateCameraSleep(reason);
}

bool cameraSleepGuardActive() {
  return sleepGuard.autoWakeBlocked;
}

bool cameraPowerProbeBackoffActive() {
  return sleepGuard.autoWakeBlocked &&
         sleepGuard.powerProbeBackoffUntil != 0 &&
         !timeReached(sleepGuard.powerProbeBackoffUntil);
}

uint32_t cameraPowerProbeBackoffRemainingMs() {
  if (!cameraPowerProbeBackoffActive()) {
    return 0;
  }
  return sleepGuard.powerProbeBackoffUntil - millis();
}

void scheduleCameraPowerProbeBackoff(const char* source) {
  const CameraProtocolProfile& connectedProfile = bleCamera.protocolProfile();
  const CameraProtocolProfile& storedProfile = cameraProtocolProfile(
      cameraProfile.protocolGenerationKnown
        ? cameraProfile.protocolGeneration
        : RicohProtocolGeneration::Unknown);
  const uint32_t profileInterval = connectedProfile.standbyModeRequiresFreshReconnect
                                     ? connectedProfile.standbyProbeIntervalMs
                                     : storedProfile.standbyProbeIntervalMs;
  const uint32_t intervalMs = profileInterval > 0
                                ? profileInterval
                                : CAMERA_POWER_OFF_PROBE_BACKOFF_MS;
  sleepGuard.powerProbeBackoffUntil = millis() + intervalMs;
  Serial.printf("BLE guard: next power probe in %lums (%s)\n",
                static_cast<unsigned long>(intervalMs),
                source != nullptr ? source : "camera standby");
}

void showCameraSleepGuardStatus(bool force = false) {
  showStatusIfChanged("Camera standby",
                      "Press BtnA to scan",
                      "Auto power off: 30s",
                      preferredBleName(),
                      force);
}

void enterCameraSleepGuard(const char* source, int reason) {
  const char* guardSource = source != nullptr ? source : "camera standby";
  if (sleepGuard.autoWakeBlocked) {
    if (reason != 0 && sleepGuard.disconnectReason == 0) {
      sleepGuard.disconnectReason = reason;
    }
    cameraPowerState = RicohCameraPowerState::OffOrShuttingDown;
    cameraOperationMode = RicohCameraOperationMode::Unknown;
    scheduleCameraPowerProbeBackoff(guardSource);
    closeLiveView(guardSource);
    wifiPreview.disconnectWifi();
    unparkBleIfNeeded(guardSource);
    bleCamera.disconnect();
    setCameraFlowState(CameraFlowState::CameraPowerOff, guardSource);
    showCameraSleepGuardStatus(false);
    lastFrameAt = millis();
    lastCameraRecoveryAt = millis();
    return;
  }

  cameraPowerState = RicohCameraPowerState::OffOrShuttingDown;
  cameraOperationMode = RicohCameraOperationMode::Unknown;
  sleepGuard.autoWakeBlocked = true;
  sleepGuard.blockedFlowLogged = false;
  sleepGuard.enteredAt = millis();
  sleepGuard.disconnectReason = reason;

  closeLiveView(guardSource);
  wifiPreview.disconnectWifi();
  unparkBleIfNeeded(guardSource);
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::CameraPowerOff, guardSource);
  Serial.printf("BLE guard: remote disconnect reason=%d; scan paused, auto power off in %lums\n",
                reason,
                static_cast<unsigned long>(CAMERA_SLEEP_AUTO_POWER_OFF_MS));
  showCameraSleepGuardStatus(true);
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
  if (!isCameraSleepDisconnectReason(reason)) {
    return false;
  }

  enterCameraSleepGuard(source, reason);
  return true;
}

bool consumeCameraPowerOffDisconnectAfterReady(const char* source) {
  const int reason = bleCamera.consumeDisconnectReason();
  if (!isCameraSleepDisconnectReason(reason)) {
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
  if (sleepGuard.autoWakeBlocked) {
    Serial.printf("BLE guard: clearing guard (%s), previous disconnect reason=%d\n",
                  source != nullptr ? source : "manual",
                  sleepGuard.disconnectReason);
  }
  cameraPowerState = RicohCameraPowerState::Unknown;
  cameraOperationMode = RicohCameraOperationMode::Unknown;
  sleepGuard.autoWakeBlocked = false;
  sleepGuard.blockedFlowLogged = false;
  sleepGuard.enteredAt = 0;
  sleepGuard.powerProbeBackoffUntil = 0;
  sleepGuard.disconnectReason = 0;
  bleCamera.clearDisconnectReason();
}

bool cameraSleepGuardBlocksFlow(const char* action) {
  if (sleepGuard.autoWakeBlocked) {
    if (!sleepGuard.blockedFlowLogged) {
      Serial.printf("BLE guard: blocked %s; press BtnA to scan (further repeats suppressed)\n",
                    action != nullptr ? action : "automatic flow");
      sleepGuard.blockedFlowLogged = true;
    }
    showCameraSleepGuardStatus(false);
  }
  return sleepGuard.autoWakeBlocked;
}

bool hasUsableCachedWifiCredentials() {
  return cameraProfile.wifi.cached &&
         cameraProfile.wifi.credentialsValid &&
         cameraProfile.wifi.ssid.length() > 0 &&
         (isGr2WifiOnlyProfile() || cameraProfile.bleAddress.length() > 0);
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
  if (cameraProfile.wifi.ssid.length() == 0 ||
      !cameraProfile.wifi.credentialsValid) {
    return;
  }
  if (isGr2WifiOnlyProfile()) {
    cameraProfile.wifi.cached = true;
    if (g_profileStore->save(cameraProfile)) {
      Serial.printf("WiFi cache: saved manual GR II profile (%s) ssid='%s' bssid='%s' channel=%u\n",
                    source != nullptr ? source : "update",
                    cameraProfile.wifi.ssid.c_str(),
                    cameraProfile.wifi.bssid.c_str(),
                    static_cast<unsigned>(cameraProfile.wifi.channel));
    }
    return;
  }
  if (cameraProfile.bleAddress.length() == 0) {
    return;
  }
  if (g_profileStore->saveWifiCredentials(cameraProfile.bleAddress, cameraProfile.wifi)) {
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
  if (!grWifi.isConnected() || cameraProfile.wifi.ssid.length() == 0 ||
      (!isGr2WifiOnlyProfile() && cameraProfile.bleAddress.length() == 0)) {
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
  const uint8_t connectedChannel = grWifi.channel();
  if (connectedChannel >= 1 && connectedChannel <= 14) {
    cameraProfile.wifi.channel = connectedChannel;
    cameraProfile.wifi.frequencyMhz = connectedChannel == 14
                                        ? 2484
                                        : static_cast<uint16_t>(2407 + connectedChannel * 5);
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
  cameraProfile.wifi.credentialsValid = credentials.valid;
  cameraProfile.wifi.source = bleCamera.protocolProfile().wifiCredentialMethod ==
                                      WifiCredentialMethod::BleUuidCharacteristics
                                ? WifiCredentialSource::BleUuidCharacteristics
                                : WifiCredentialSource::BleFixedHandles;

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
  if (isGr2WifiOnlyProfile() || !bleCamera.isConnected() || cameraProfile.bleAddress.length() == 0) {
    return;
  }
  wifiCacheRefresh.pending = true;
  wifiCacheRefresh.after = millis() + WIFI_CACHE_REFRESH_DELAY_MS;
}

void refreshWifiCacheIfDue() {
  if (!wifiCacheRefresh.pending || !timeReached(wifiCacheRefresh.after)) {
    return;
  }
  wifiCacheRefresh.pending = false;
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
  if (!g_profileStore->load(cameraProfile)) {
    Serial.println("Profile: NVS open failed; using runtime defaults");
    cameraProfile = CameraProfile{};
  }

  if (cameraProfile.wifi.cameraIp.isEmpty()) {
    cameraProfile.wifi.cameraIp = GR_HOST;
  }
  wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);

  RicohSecurityProfileId securityProfile = cameraProfile.securityProfileKnown
                                             ? cameraProfile.securityProfile
                                             : securityProfileForGeneration(cameraProfile.protocolGeneration);
  if (cameraProfile.protocolGenerationKnown &&
      securityProfile != securityProfileForGeneration(cameraProfile.protocolGeneration)) {
    securityProfile = securityProfileForGeneration(cameraProfile.protocolGeneration);
    cameraProfile.securityProfile = securityProfile;
    cameraProfile.securityProfileKnown =
        securityProfile != RicohSecurityProfileId::Unknown;
    (void)g_profileStore->save(cameraProfile);
  }
  const rvf::Result profileResult = bleCamera.setSecurityProfile(securityProfile);
  if (profileResult.failed()) {
    Serial.printf("Profile: BLE security profile apply failed: %s\n", bleCamera.lastError().c_str());
  }
  bleCamera.setBindingState(cameraProfile.bleAddress.length() > 0
                              ? CameraBindingState::Locked
                              : CameraBindingState::Unpaired);

  Serial.printf("Profile: camera='%s' ble='%s' identity='%s' protocol=%s security=%s binding=%u ip='%s'\n",
                cameraProfile.cameraName.c_str(),
                cameraProfile.bleAddress.c_str(),
                cameraProfile.peerIdentityAddress.c_str(),
                ricohProtocolGenerationName(cameraProfile.protocolGeneration),
                ricohSecurityProfileName(securityProfile),
                static_cast<unsigned>(bleCamera.bindingState()),
                cameraProfile.wifi.cameraIp.c_str());
}

bool serviceButtonsDuringBleOperation();
void refreshPropsIfDue(bool force = false);

bool readCameraOperationModeWithRetries() {
  const uint8_t modeRetries = RICOH_BLE_OPERATION_MODE_READ_RETRIES == 0 ? 1 : RICOH_BLE_OPERATION_MODE_READ_RETRIES;
  for (uint8_t attempt = 0; attempt < modeRetries; ++attempt) {
    const rvf::Result operationModeResult = bleCamera.readOperationMode(cameraOperationMode);
    if (operationModeResult.ok()) {
      return true;
    }
    Serial.printf("BLE: operation mode read attempt %u/%u failed: %s\n",
                  static_cast<unsigned>(attempt + 1),
                  static_cast<unsigned>(modeRetries),
                  bleCamera.lastError().c_str());
    delay(80);
    yield();
  }
  return false;
}

enum class Gr3StartupModeWait : uint8_t {
  Ready,
  Probe,
  Aborted,
};

Gr3StartupModeWait waitForGr3StartupModeToClear(const char* source, bool& operationModeReadOk) {
  Serial.printf("BLE: GR3 operation mode=BLE_STARTUP while power=ON; keeping link, waiting up to %lums source=%s\n",
                static_cast<unsigned long>(RICOH_BLE_GR3_STARTUP_MODE_WAIT_MS),
                source != nullptr ? source : "");
  showStatusIfChanged("Camera not ready", "BLE_STARTUP", "Return camera to", "shooting screen", true);
  if (RICOH_BLE_GR3_REQUEST_CAPTURE_IN_STARTUP_MODE && bleCamera.requestCaptureOperationMode().failed()) {
    Serial.printf("BLE: capture mode request skipped: %s\n", bleCamera.lastError().c_str());
  }

  const uint32_t waitStartedAt = millis();
  while (millis() - waitStartedAt < RICOH_BLE_GR3_STARTUP_MODE_WAIT_MS) {
    const uint32_t pollStartedAt = millis();
    while (millis() - pollStartedAt < RICOH_BLE_GR3_STARTUP_MODE_POLL_MS) {
      if (serviceButtonsDuringBleOperation()) {
        return Gr3StartupModeWait::Aborted;
      }
      delay(20);
      yield();
    }
    if (!bleCamera.isConnected()) {
      Serial.println("BLE: link lost while waiting for GR3 startup mode to clear");
      return Gr3StartupModeWait::Aborted;
    }
    if (consumeCameraPowerOffNotification("BLE power notify during GR3 startup mode wait")) {
      return Gr3StartupModeWait::Aborted;
    }
    operationModeReadOk = readCameraOperationModeWithRetries();
    if (!operationModeWaitsForCameraReady(bleCamera.protocolProfile(), cameraOperationMode, operationModeReadOk)) {
      Serial.printf("BLE: GR3 startup mode cleared after %lums mode=%s read_ok=%d\n",
                    static_cast<unsigned long>(millis() - waitStartedAt),
                    cameraOperationModeName(cameraOperationMode),
                    operationModeReadOk ? 1 : 0);
      return Gr3StartupModeWait::Ready;
    }
  }

  Serial.printf("BLE: GR3 still BLE_STARTUP after %lums; probing NetworkType\n",
                static_cast<unsigned long>(millis() - waitStartedAt));
  return Gr3StartupModeWait::Probe;
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
  showStatusIfChanged("BLE_READY", "Checking power", cameraProfile.cameraName, "", true);
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
  cameraWifiStartupModeProbe = false;
  bool operationModeReadOk = false;
  if (cameraPowerPolicy.shouldReadOperationMode(readOk, toPolicyPowerStatus(cameraPowerState))) {
    operationModeReadOk = readCameraOperationModeWithRetries();

    if (operationModeWaitsForCameraReady(bleCamera.protocolProfile(), cameraOperationMode, operationModeReadOk)) {
      const Gr3StartupModeWait wait = waitForGr3StartupModeToClear(source, operationModeReadOk);
      if (wait == Gr3StartupModeWait::Aborted) {
        if (bleCamera.isConnected() && !sleepGuard.autoWakeBlocked) {
          setCameraFlowState(CameraFlowState::BleReady, "GR3 startup mode wait aborted");
        }
        return false;
      }
      cameraWifiStartupModeProbe = wait == Gr3StartupModeWait::Probe;
    }

    if (!cameraWifiStartupModeProbe &&
        cameraPowerPolicy.blocksStandbyOperationMode(operationModeReadOk, toPolicyOperationStatus(cameraOperationMode), false)) {
      Serial.printf("WiFi blocked: camera operation mode=%s while power=%s source=%s\n",
                    cameraOperationModeName(cameraOperationMode),
                    cameraPowerStateName(cameraPowerState),
                    source != nullptr ? source : "");
      showStatusIfChanged("Camera standby",
                          cameraOperationModeName(cameraOperationMode),
                          "Waiting power on",
                          "Auto scan active",
                          true);
      enterCameraSleepGuard("BLE operation mode standby", RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
      return false;
    }
  }

  const CameraProtocolProfile& protocol = bleCamera.protocolProfile();
  if (protocol.generation == RicohProtocolGeneration::Gr3Family) {
    const RicohBleSecurityState security = bleCamera.securityState();
    const bool safeMode = cameraWifiStartupModeProbe ||
                          operationModeAllowsWifi(protocol,
                                                  cameraOperationMode,
                                                  operationModeReadOk);
    if (!security.encrypted || !security.authenticated || !safeMode) {
      Serial.printf("WiFi blocked: profile=GR3_FAMILY encrypted=%d authenticated=%d operation_mode=%s read_ok=%d\n",
                    security.encrypted ? 1 : 0,
                    security.authenticated ? 1 : 0,
                    cameraOperationModeName(cameraOperationMode),
                    operationModeReadOk ? 1 : 0);
      showStatusIfChanged("Camera not ready",
                          cameraOperationModeName(cameraOperationMode),
                          "GR3 requires CAPTURE",
                          "BtnA: retry",
                          true);
      enterCameraSleepGuard("GR3 operation/security gate", RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
      return false;
    }
  }

  if (consumeCameraPowerOffNotification("BLE power notify before WiFi allow")) {
    return false;
  }

  if (cameraPowerPolicy.mayActivateWifi(toPolicyPowerStatus(cameraPowerState))) {
    if (sleepGuard.autoWakeBlocked) {
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
  showStatusIfChanged("Camera standby",
                      readOk ? "Power OFF" : "Power unknown",
                      "Waiting power on",
                      "Auto scan active",
                      true);
  enterCameraSleepGuard(reason, RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
  return false;
}

bool ensureCameraPowerReadyForController(const char* source) {
  if (isGr2WifiOnlyProfile()) {
    return true;
  }
  return ensureCameraPowerReadyForWifi(source != nullptr ? source : "camera power");
}

bool shutterUsesHttp();

// GATT discovery behind the status subscriptions took 4.3-4.8 s on a GR IIIx.
// Run from the per-tick service it landed right after LiveView opened, held
// the loop off the MJPEG stream and tripped the 5 s stall watchdog. Doing it
// here, before WLAN ON, keeps it off the stream and off a busy camera radio.
void subscribeCameraStatusNotifyIfNeeded(const char* when) {
  if (!RICOH_BLE_SUBSCRIBE_CAMERA_STATUS || !bleCamera.isConnected() || shutterUsesHttp()) {
    return;
  }
  if (bleCamera.statusNotifyAttempted()) {
    return;
  }
  Serial.printf("BLE: status notify subscribe (%s)\n", when != nullptr ? when : "");
  const rvf::Result result = bleCamera.enableCameraStatusNotify();
  if (result.failed() && bleCamera.statusNotifyAttempted()) {
    Serial.printf("BLE: status notify unavailable: %s\n", bleCamera.lastError().c_str());
  }
}

bool activateCameraWifiOverBle() {
  if (isGr2WifiOnlyProfile()) {
    // GR II has no BLE radio. The user enables Wi-Fi on the camera body before
    // the StickS3 joins the already-advertising access point.
    return true;
  }
  if (!RICOH_BLE_AUTO_WLAN_ON_BOOT) {
    return true;
  }
  if (cameraSleepGuardBlocksFlow("WiFi open")) {
    return false;
  }
  if (!bleCamera.isConnected()) {
    showStatusIfChanged("BLE not ready", "Cannot open WiFi", "", "", true);
    return false;
  }
  if (!ensureCameraPowerReadyForWifi("WiFi open")) {
    return false;
  }
  if (settleAndConsumeCameraPowerOffNotification("BLE power notify before WiFi open")) {
    return false;
  }

  subscribeCameraStatusNotifyIfNeeded("before WLAN ON");
  if (consumeCameraPowerOffNotification("BLE power notify after status subscribe")) {
    return false;
  }

  showStatusIfChanged("BLE_READY", "Opening WiFi", cameraProfile.cameraName, "", true);
  setCameraFlowState(CameraFlowState::ActivatingWifi, "BLE WiFi ON");
  const bool startupModeProbe = cameraWifiStartupModeProbe;
  cameraWifiStartupModeProbe = false;
  const rvf::Result openWifiResult = bleCamera.openWifi(startupModeProbe);
  if (openWifiResult.failed() && startupModeProbe) {
    Serial.printf("BLE: camera refused WLAN while BLE_STARTUP: %s; keeping link\n",
                  bleCamera.lastError().c_str());
    showStatusIfChanged("Camera not ready", "BLE_STARTUP", "Return camera to", "shooting screen", true);
    setCameraFlowState(CameraFlowState::BleReady, "GR3 startup mode WLAN probe refused");
    return false;
  }
  if (openWifiResult.failed()) {
    Serial.printf("BLE: Wi-Fi open failed: %s\n", bleCamera.lastError().c_str());
    showStatusIfChanged("BLE WiFi failed", bleCamera.lastError(), "", "", true);
    setCameraFlowState(CameraFlowState::BleReady, "BLE WiFi open failed");
    return false;
  }

  showStatusIfChanged("BLE WiFi sent", "Waiting WiFi params", cameraProfile.cameraName, "", true);
  if (RICOH_BLE_POST_WLAN_ON_WAIT_MS > 0) {
    delay(RICOH_BLE_POST_WLAN_ON_WAIT_MS);
  }
  return true;
}

bool deactivateCameraWifiOverBle() {
  const CameraProtocolProfile& protocol = bleCamera.protocolProfile();
  if (!protocolAllowsBleSideEffect(protocol, BleSideEffect::WifiDeactivation)) {
    return true;
  }
  if (!bleCamera.isConnected()) {
    Serial.println("BLE WLAN close skipped: BLE not connected");
    return false;
  }

  const rvf::Result closeWifiResult = bleCamera.closeWifi();
  if (closeWifiResult.failed()) {
    Serial.printf("BLE: Wi-Fi close failed: %s\n", bleCamera.lastError().c_str());
    return false;
  }
  return true;
}

bool reactivateCameraWifiForCachedResume() {
  const CameraProtocolProfile& protocol = bleCamera.protocolProfile();
  if (!protocolAllowsBleSideEffect(protocol, BleSideEffect::WifiDeactivation)) {
    return true;
  }
  return activateCameraWifiOverBle();
}

bool hasStoredBleIdentity() {
  return cameraProfile.peerIdentityAddress.length() > 0 ||
         cameraProfile.bleAddress.length() > 0 ||
         cameraProfile.lastSeenOtaAddress.length() > 0;
}

bool hasConfiguredCameraProfile() {
  return isGr2WifiOnlyProfile() ? hasUsableCachedWifiCredentials()
                                : hasStoredBleIdentity();
}

String storedBleTargetAddress() {
  if (cameraProfile.peerIdentityKnown && cameraProfile.peerIdentityAddress.length() > 0) {
    return cameraProfile.peerIdentityAddress;
  }
  if (cameraProfile.bleAddress.length() > 0) {
    return cameraProfile.bleAddress;
  }
  return cameraProfile.lastSeenOtaAddress;
}

bool storedBleTargetAddressType(uint8_t& addressType) {
  if (cameraProfile.peerIdentityKnown && cameraProfile.peerIdentityAddress.length() > 0) {
    addressType = cameraProfile.peerIdentityAddressType;
    return true;
  }
  if (cameraProfile.bleAddressTypeKnown && cameraProfile.bleAddress.length() > 0) {
    addressType = cameraProfile.bleAddressType;
    return true;
  }
  if (cameraProfile.lastSeenOtaAddress.length() > 0) {
    addressType = cameraProfile.lastSeenOtaAddressType;
    return true;
  }
  return false;
}

bool candidateMatchesBoundCamera(const RicohBleDeviceInfo& info) {
  const CameraBindingState state = bleCamera.bindingState();
  if (state == CameraBindingState::Unpaired ||
      state == CameraBindingState::Pairing ||
      state == CameraBindingState::BondInvalid) {
    return bindingStateAllowsCandidate(state, "", info.address.c_str());
  }
  const bool peerIdentityMatch =
      bindingStateAllowsCandidate(state,
                                  cameraProfile.peerIdentityAddress.c_str(),
                                  info.address.c_str());
  const bool legacyAddressMatch =
      bindingStateAllowsCandidate(state,
                                  cameraProfile.bleAddress.c_str(),
                                  info.address.c_str());
  const bool otaAddressMatch =
      bindingStateAllowsCandidate(state,
                                  cameraProfile.lastSeenOtaAddress.c_str(),
                                  info.address.c_str());
  return peerIdentityMatch || legacyAddressMatch || otaAddressMatch;
}

void enterBondInvalid(const char* reason) {
  bleCamera.setBindingState(CameraBindingState::BondInvalid);
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, reason != nullptr ? reason : "pairing invalid");
  showStatusIfChanged("Pairing expired",
                      "Remove StickS3",
                      "from camera first",
                      "Hold B to re-pair",
                      true);
  Serial.printf("BLE binding: state=BOND_INVALID reason=%s\n",
                reason != nullptr ? reason : "security request");
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
  scheduleCameraPowerProbeBackoff(errorText.c_str());
}

void saveConnectedBleIdentity(const String& connectedName,
                              const RicohBleDeviceInfo& info,
                              bool observedAdvertisement = true) {
  const RicohBleSecurityState security = bleCamera.securityState();
  cameraProfile.cameraName = connectedName;
  if (observedAdvertisement) {
    cameraProfile.bleAddress = info.address;
    cameraProfile.bleAddressType = info.addressType;
    cameraProfile.bleAddressTypeKnown = true;
  }
  cameraProfile.bleBonded = security.bonded || bleCamera.isBonded(info);
  cameraProfile.bleAuthenticated = security.authenticated;
  cameraProfile.protocolGeneration = bleCamera.protocolProfile().generation;
  cameraProfile.protocolGenerationKnown =
      cameraProfile.protocolGeneration != RicohProtocolGeneration::Unknown;
  cameraProfile.capabilityVersion = bleCamera.protocolProfile().capabilityVersion;
  cameraProfile.securityProfile =
      securityProfileForGeneration(cameraProfile.protocolGeneration);
  cameraProfile.securityProfileKnown =
      cameraProfile.securityProfile != RicohSecurityProfileId::Unknown;
  if (observedAdvertisement) {
    cameraProfile.lastSeenOtaAddress = info.address;
    cameraProfile.lastSeenOtaAddressType = info.addressType;
  }
  if (bleCamera.connectedIdentityKnown()) {
    cameraProfile.peerIdentityAddress = bleCamera.connectedIdentityAddress();
    cameraProfile.peerIdentityAddressType = bleCamera.connectedIdentityAddressType();
    cameraProfile.peerIdentityKnown = true;
  }
  cameraProfile.profileVersion = CAMERA_PROFILE_SCHEMA_VERSION;
  g_profileStore->saveBleIdentity(cameraProfile.cameraName,
                               cameraProfile.bleAddress,
                               cameraProfile.bleAddressType,
                               cameraProfile.bleBonded);
  g_profileStore->save(cameraProfile);
  bleCamera.setBindingState(CameraBindingState::Locked);
}

bool tryDirectStoredBleReconnect(bool force = false) {
  const String targetAddress = storedBleTargetAddress();
  uint8_t targetAddressType = 0;
  const bool targetTypeKnown = storedBleTargetAddressType(targetAddressType);
  const bool directReconnectRequested =
      force || (BLE_DIRECT_RECONNECT_ON_BOOT && setupCameraFlowActive);
  if (!shouldAttemptDirectBleReconnect(directReconnectRequested,
                                       cameraProfile.bleBonded,
                                       cameraProfile.protocolGenerationKnown,
                                       targetAddress.c_str(),
                                       targetTypeKnown)) {
    return false;
  }

  RicohBleDeviceInfo info;
  info.found = true;
  info.connectable = true;
  info.name = cameraProfile.cameraName;
  info.address = targetAddress;
  info.addressType = targetAddressType;
  if (!bleCamera.isBonded(info)) {
    Serial.printf("BLE fast path: stored peer is not in the local bond store addr=%s; scanning\n",
                  targetAddress.c_str());
    return false;
  }

  showStatusIfChanged("BLE fast reconnect",
                      cameraProfile.cameraName,
                      targetAddress,
                      "Direct bonded link",
                      true);
  setCameraFlowState(CameraFlowState::ConnectingBle, "saved BLE direct connect");

  RicohBleConnectOptions options;
  options.timeoutMs = BLE_FAST_CONNECT_TIMEOUT_MS;
  options.securityWaitMs = RICOH_BLE_BONDED_SECURITY_WAIT_MS;
  options.preConnectDelayMs = 0;
  options.connectRetries = 0;
  options.exchangeMtu = true;
  options.protocolHint = cameraProfile.protocolGeneration;

  const uint32_t startedAt = millis();
  Serial.printf("BLE fast path: direct connect addr=%s type=%u timeout=%lums\n",
                targetAddress.c_str(),
                static_cast<unsigned>(targetAddressType),
                static_cast<unsigned long>(options.timeoutMs));
  const rvf::Result result = bleCamera.connectCamera(info, options);
  if (result.failed()) {
    Serial.printf("BLE fast path: failed after %lums (%s); falling back to scan\n",
                  static_cast<unsigned long>(millis() - startedAt),
                  bleCamera.lastError().c_str());
    if (bleCamera.consumeBondInvalidRequest()) {
      enterBondInvalid(bleCamera.lastError().c_str());
      return false;
    }
    bleCamera.disconnect();
    return false;
  }

  if (cameraProfile.peerIdentityKnown &&
      bleCamera.connectedIdentityKnown() &&
      !cameraProfile.peerIdentityAddress.equalsIgnoreCase(bleCamera.connectedIdentityAddress())) {
    enterBondInvalid("direct-connected identity does not match saved identity");
    return false;
  }

  saveConnectedBleIdentity(displayBleName(info), info, false);
  showStatusIfChanged("BLE link ready", cameraProfile.cameraName, info.address, "WiFi via BLE", true);
  setCameraFlowState(CameraFlowState::BleReady, "BLE direct reconnect");
  Serial.printf("BLE fast path: ready in %lums\n",
                static_cast<unsigned long>(millis() - startedAt));
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
  if (isGr2WifiOnlyProfile()) {
    showStatusIfChanged("GR II WiFi", "Saved WLAN profile", cameraProfile.wifi.ssid,
                        "No Bluetooth", true);
    setCameraFlowState(CameraFlowState::BleReady, "GR II WiFi-only profile ready");
    return true;
  }
  if (cameraSleepGuardBlocksFlow("BLE discovery")) {
    return false;
  }
  if (bleCamera.bindingState() == CameraBindingState::BondInvalid) {
    enterBondInvalid("locked profile requested new pairing");
    return false;
  }
  const bool firstBootPairing = !hasStoredBleIdentity();
  if (firstBootPairing && !pairingModelConfirmed) {
    Serial.println("BLE pairing: waiting for camera model confirmation");
    return false;
  }
  bleCamera.setBindingState(firstBootPairing
                              ? CameraBindingState::Pairing
                              : CameraBindingState::Locked);
  const uint8_t configuredAttempts = firstBootPairing
                                       ? FIRST_BOOT_BLE_PAIRING_ATTEMPTS
                                       : (setupCameraFlowActive ? 1 : BLE_CONNECT_ATTEMPTS);
  const uint8_t attempts = configuredAttempts == 0 ? 1 : configuredAttempts;
  uint8_t consecutiveConnectFailures = 0;
  String retryPreferredAddress = storedBleTargetAddress();
  String retryPreferredName = preferredBleName();
  bool bondedFastSecurityAttempted = false;

  if (!firstBootPairing && tryDirectStoredBleReconnect()) {
    return true;
  }
  if (bleCamera.bindingState() == CameraBindingState::BondInvalid) {
    return false;
  }

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
    showStatusIfChanged(firstBootPairing ? "Pairing GR BLE" : "Scanning GR BLE",
                        cameraProfile.bleAddress,
                        bleName,
                        String("Attempt ") + attempt + "/" + attempts,
                        true);

    RicohBleDeviceInfo info = bleCamera.scanCamera(retryPreferredAddress, bleName, BLE_SCAN_SECONDS);
    if (resetBlePairingIfRequested()) {
      return false;
    }
    if (!info.found) {
      showStatusIfChanged("BLE not found", "Retrying...", "", "", true);
    } else if (!info.connectable) {
      Serial.printf("BLE: scan selected non-connectable candidate name='%s' addr=%s; retrying\n",
                    info.name.c_str(),
                    info.address.c_str());
      showStatusIfChanged("BLE not connectable", info.address, "Retrying...", "", true);
    } else if (!candidateMatchesBoundCamera(info)) {
      Serial.printf("BLE: ignoring non-stored camera addr=%s name='%s'; waiting for stored addr=%s\n",
                    info.address.c_str(),
                    info.name.c_str(),
                    cameraProfile.bleAddress.c_str());
      showStatusIfChanged("Waiting stored camera",
                          cameraProfile.cameraName,
                          cameraProfile.bleAddress,
                          "Other GR ignored",
                          true);
    } else if (shouldDelayStoredIdentityPowerProbe(info, firstBootPairing)) {
      Serial.printf("BLE: skipping standby power probe for %lums addr=%s name='%s'\n",
                    static_cast<unsigned long>(cameraPowerProbeBackoffRemainingMs()),
                    info.address.c_str(),
                    info.name.c_str());
      showStatusIfChanged("Camera standby", "Waiting power on", "Auto scan active", preferredBleName(), true);
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
      showStatusIfChanged("Camera standby", "Waiting power on", "Auto scan active", preferredBleName(), true);
    } else {
      const String connectedName = displayBleName(info);

      // The first scan may intentionally spend the full discovery window
      // choosing a camera. Once selected, remember it for this boot attempt so
      // retries stop as soon as that address advertises again.
      retryPreferredAddress = info.address;
      if (info.name.length() > 0) {
        retryPreferredName = info.name;
      }

      showStatusIfChanged("BLE camera found", connectedName, info.address, "Connecting...", true);
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
      options.exchangeMtu = true;
      options.protocolHint = !firstBootPairing && cameraProfile.protocolGenerationKnown
                               ? cameraProfile.protocolGeneration
                               : RicohProtocolGeneration::Unknown;
      options.expectedGeneration = firstBootPairing
                                     ? selectedPairingGeneration
                                     : RicohProtocolGeneration::Unknown;

      Serial.printf("BLE: connect policy bonded=%d fast_security=%d security_wait=%lums\n",
                    bonded ? 1 : 0,
                    useBondedFastSecurity ? 1 : 0,
                    static_cast<unsigned long>(options.securityWaitMs));

      const rvf::Result connectResult = bleCamera.connectCamera(info, options);
      if (resetBlePairingIfRequested()) {
        return false;
      }
      if (connectResult.ok()) {
        if (!firstBootPairing &&
            cameraProfile.peerIdentityKnown &&
            bleCamera.connectedIdentityKnown() &&
            !cameraProfile.peerIdentityAddress.equalsIgnoreCase(
                bleCamera.connectedIdentityAddress())) {
          enterBondInvalid("connected identity does not match saved identity");
          return false;
        }
        saveConnectedBleIdentity(connectedName, info);
        showStatusIfChanged("BLE link ready", cameraProfile.cameraName, info.address, "WiFi via BLE", true);
        setCameraFlowState(CameraFlowState::BleReady, "BLE connected");
        return true;
      }

      Serial.printf("BLE: connect attempt %u/%u failed: %s\n",
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(attempts),
                    bleCamera.lastError().c_str());
      if (bleCamera.consumeBondInvalidRequest()) {
        enterBondInvalid(bleCamera.lastError().c_str());
        return false;
      }
      if (shouldBackoffAfterStoredIdentityConnectFailure(info, bleCamera.lastError(), firstBootPairing)) {
        deferStoredIdentityPowerProbeAfterConnectFailure(bleCamera.lastError());
      }
      consumeCameraPowerOffDisconnectAfterReady("BLE connect failed");
      showStatusIfChanged("BLE connect failed", bleCamera.lastError(), "Retrying...", "", true);
      bleCamera.disconnect();
      const bool protocolDiscoveryIncomplete =
          bleCamera.lastError().startsWith("RICOH BLE protocol unknown");
      if (protocolDiscoveryIncomplete) {
        // The camera rejected or interrupted read-only GATT discovery. This is
        // a peer timing outcome, not evidence that the local NimBLE host is
        // unhealthy, so retry without paying for a full stack reset.
        consecutiveConnectFailures = 0;
        Serial.println("BLE: protocol discovery incomplete; retrying without stack reset");
      } else if (bleCamera.lastFailureWasResourceExhausted()) {
        Serial.println("BLE: host resources exhausted during connect; rebuild stack objects before retry");
        if (bleCamera.resetStack(true).failed()) {
          Serial.printf("BLE: stack rebuild failed: %s\n", bleCamera.lastError().c_str());
          return false;
        }
        consecutiveConnectFailures = 0;
        skipRetryDelay = true;
      } else {
        consecutiveConnectFailures++;
        if (BLE_STACK_RESET_AFTER_FAILURES > 0 && consecutiveConnectFailures >= BLE_STACK_RESET_AFTER_FAILURES) {
          if (bleCamera.resetStack().failed()) {
            Serial.printf("BLE: stack reset failed: %s\n", bleCamera.lastError().c_str());
            return false;
          }
          consecutiveConnectFailures = 0;
          skipRetryDelay = true;
        }
      }
    }

    if (attempt < attempts && !skipRetryDelay) {
      delay(bleRetryDelayMs(firstBootPairing,
                            BLE_CONNECT_RETRY_DELAY_MS,
                            BLE_FIRST_PAIRING_RETRY_DELAY_MS));
      yield();
      if (resetBlePairingIfRequested()) {
        return false;
      }
    }
  }

  showStatusIfChanged("BLE unavailable", "Return BLE scan", preferredBleName(), "", true);
  setCameraFlowState(CameraFlowState::BleScan, "BLE attempts exhausted");
  if (bleCamera.resetStack().failed()) {
    Serial.printf("BLE: final stack reset failed: %s\n", bleCamera.lastError().c_str());
  }
  return false;
}

// Wi-Fi and BLE share one radio, and holding the BLE link open while the
// preview streams costs more than half the available throughput (measured on
// a GR IIIx: 380 KiB/s with it up against 831 KiB/s without). The camera does
// not need BLE once its access point is running - the shutter, state and
// battery are all served over HTTP - so landscape runs on Wi-Fi alone: BLE only
// brings the access point up, is released as soon as the preview streams, and
// is brought back when the preview stops or has to be recovered.
//
// Parked means "released deliberately", which the flow must not confuse with
// "the camera vanished". While parked there is no BLE link to lose, and a
// camera that really goes away takes its access point with it, so the Wi-Fi
// and HTTP paths detect that instead.
bool bleParkedForPreview = false;
uint32_t bleParkedAtMs = 0;

// True when the BLE link is either up or deliberately parked. Everything that
// asks "is the camera still with us?" wants this - the supervisor, the UI, the
// Wi-Fi guard - because a parked link is not a lost one. Only code about to
// issue an actual BLE operation should ask bleCamera.isConnected().
bool bleLinkHealthy() {
  // For GR II this is the controller's logical prerequisite signal: there is
  // deliberately no BLE link to establish or monitor.
  return isGr2WifiOnlyProfile() || bleCamera.isConnected() || bleParkedForPreview;
}

bool bleStillConnectedForWifi() {
  return bleLinkHealthy();
}

// The BLE link is down in landscape, so fall back to the generation stored at
// pairing whenever the live link cannot say which camera this is.
const CameraProtocolProfile& knownCameraProtocol() {
  const CameraProtocolProfile& connected = bleCamera.protocolProfile();
  if (connected.generation != RicohProtocolGeneration::Unknown) {
    return connected;
  }
  return cameraProtocolProfile(cameraProfile.protocolGenerationKnown
                                   ? cameraProfile.protocolGeneration
                                   : RicohProtocolGeneration::Unknown);
}

bool cameraSupportsWifiOnlyLandscape() {
  const CameraCapabilities& capabilities = knownCameraProtocol().capabilities;
  return capabilities.supportsHttpShutter && capabilities.allowBlePreviewParking;
}

// Landscape shoots over HTTP only, including the moments where BLE is still
// up bringing the access point online.
bool shutterUsesHttp() {
  return isGr2WifiOnlyProfile() || bleParkedForPreview ||
         (appController.previewRequested() && cameraSupportsWifiOnlyLandscape());
}

void unparkBleIfNeeded(const char* reason) {
  if (!bleParkedForPreview) {
    return;
  }
  bleParkedForPreview = false;
  Serial.printf("BLE: unparking (%s)\n", reason != nullptr ? reason : "unspecified");
}

bool restoreBleAfterPreviewPark(const char* reason) {
  if (!bleParkedForPreview) {
    return bleCamera.isConnected();
  }
  if (bleCamera.isConnected()) {
    unparkBleIfNeeded("BLE link already restored");
    return true;
  }

  Serial.printf("BLE: restoring parked link (%s)\n",
                reason != nullptr ? reason : "preview stopped");
  // Keep the logical parking lease until the physical link is restored. This
  // prevents the controller from treating our intentional disconnect as an
  // unrelated link loss while direct reconnect is in progress.
  bleCamera.clearDisconnectReason();
  if (tryDirectStoredBleReconnect(true)) {
    bleCamera.clearDisconnectReason();
    unparkBleIfNeeded("parked BLE restored");
    if (!appController.previewRequested() && hasUsableCachedWifiCredentials()) {
      setCameraFlowState(CameraFlowState::WifiCredentialsReady,
                         "portrait BLE restored after preview park");
    }
    return true;
  }

  unparkBleIfNeeded("parked BLE direct reconnect failed");
  if (bleCamera.bindingState() != CameraBindingState::BondInvalid) {
    setCameraFlowState(CameraFlowState::BleScan, "restore BLE after preview park");
  }
  return false;
}

void restoreReleasedBleForRecovery(const char* reason) {
  if (!bleParkedForPreview) {
    return;
  }
  // The broken stream is torn down anyway; dropping it first leaves the radio
  // to the BLE reconnect.
  closeLiveView(reason);
  wifiPreview.disconnectWifi();
  (void)restoreBleAfterPreviewPark(reason);
}

bool cameraWifiConnectGuard() {
  // GrWifi polls this guard during a blocking STA connection attempt. Keep the
  // IMU/UI sampler alive so a stable turn to portrait can cancel the attempt.
  updateUi();
  return bleStillConnectedForWifi() && appController.previewRequested();
}

bool wifiStillConnectedForController() {
  return grWifi.isConnected();
}

bool connectWifiFromProfile(bool forceStatus, bool requireBleAnchor = false, uint32_t totalTimeoutMs = WIFI_CONNECT_TIMEOUT_MS, bool allowFullScanFallback = true) {
  wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
  showStatusIfChanged("Connecting WiFi", cameraProfile.wifi.ssid, cameraProfile.wifi.cameraIp, "", forceStatus);
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
                                        requireBleAnchor ? cameraWifiConnectGuard : nullptr)
                  .ok();

    if (allowFullScanFallback && !connected && (!requireBleAnchor || cameraWifiConnectGuard())) {
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
                                          requireBleAnchor ? cameraWifiConnectGuard : nullptr)
                    .ok();
    }
  } else {
    connected = wifiPreview.connectWifi(cameraProfile.wifi.ssid.c_str(),
                                        cameraProfile.wifi.passphrase.c_str(),
                                        cameraProfile.wifi.bssid.c_str(),
                                        0,
                                        totalTimeoutMs,
                                        requireBleAnchor ? cameraWifiConnectGuard : nullptr)
                  .ok();
  }

  if (connected) {
    showStatusIfChanged("WiFi connected", grWifi.localIPString(), cameraProfile.wifi.cameraIp, "", true);
    Serial.printf("WiFi: connected ip=%s rssi=%ld\n", grWifi.localIPString().c_str(), static_cast<long>(grWifi.rssi()));
    return true;
  }

  showStatusIfChanged("WiFi pending", grWifi.statusText(), cameraProfile.wifi.ssid, "", true);
  Serial.printf("WiFi: connect failed status=%s\n", grWifi.statusText().c_str());
  return false;
}

void showStartingLiveViewForController() {
  showStatusIfChanged("Starting LiveView", grWifi.localIPString(), cameraProps.model, cameraProps.battery, true);
}

void selectLiveViewResolutionFromProps() {
  String listText;
  for (size_t i = 0; i < cameraProps.resoCount; ++i) {
    if (i > 0) {
      listText += ' ';
    }
    listText += String(cameraProps.resoList[i].width) + "x" + cameraProps.resoList[i].height;
  }
  const LiveViewResolution chosen = chooseLiveViewResolution(cameraProps.resoList,
                                                             cameraProps.resoCount,
                                                             JPEG_DECODE_WIDTH,
                                                             JPEG_DECODE_HEIGHT,
                                                             LIVEVIEW_DEFAULT_WIDTH,
                                                             LIVEVIEW_DEFAULT_HEIGHT);
  if (!LIVEVIEW_REQUEST_RESO) {
    Serial.printf("HTTP: resoList=[%s] reso= disabled; candidate=%ux%u\n",
                  listText.c_str(),
                  static_cast<unsigned>(chosen.width),
                  static_cast<unsigned>(chosen.height));
    return;
  }
  grApi.setLiveViewResolution(chosen);
  if (chosen.valid()) {
    Serial.printf("HTTP: resoList=[%s] selected reso=%ux%u\n",
                  listText.c_str(),
                  static_cast<unsigned>(chosen.width),
                  static_cast<unsigned>(chosen.height));
  } else {
    Serial.printf("HTTP: resoList=[%s] keeping camera default LiveView size\n", listText.c_str());
  }
}

void applyLiveViewSpotFocusSetting() {
  if (!knownCameraProtocol().capabilities.supportsHttpFocusSetting) {
    return;
  }
  if (liveviewFocusSettingIsCenterPoint(cameraProps.focusSetting.c_str())) {
    Serial.printf("HTTP: LiveView focusSetting already %s\n", cameraProps.focusSetting.c_str());
    return;
  }
  if (!grApi.setFocusSetting(LIVEVIEW_SPOT_FOCUS_SETTING, PROPS_TIMEOUT_MS)) {
    Serial.printf("HTTP: LiveView focusSetting=spot skipped: %s\n", grApi.lastError().c_str());
    return;
  }
  if (liveviewFocusSettingAllowed(cameraProps.focusSetting.c_str())) {
    liveViewSavedFocusSetting = cameraProps.focusSetting;
  }
  cameraProps.focusSetting = LIVEVIEW_SPOT_FOCUS_SETTING;
  Serial.println("HTTP: LiveView focusSetting=spot");
}

bool openLiveViewForController() {
  grApi.setLiveViewMaxFps(isGr2WifiOnlyProfile() ? 0 : LIVEVIEW_MAX_FPS);
  if (!isGr2WifiOnlyProfile()) {
    if (!cameraProps.ok) {
      // One props read before the stream opens supplies resoList, model and
      // battery together, so nothing has to compete with the stream for them.
      refreshPropsIfDue(true);
    }
    if (cameraProps.ok && !liveViewResolutionSelected) {
      selectLiveViewResolutionFromProps();
      liveViewResolutionSelected = true;
    }
  }
  // R-Core: PUT focusSetting in the body before GET /v1/liveview. A second
  // HTTP command while MJPEG is flowing drops GR III's keep-alive.
  applyLiveViewSpotFocusSetting();
  const rvf::Result previewResult = wifiPreview.startPreview();
  if (previewResult.failed()) {
    return false;
  }
  return true;
}

void onLiveViewOpenFailedForController() {
  Serial.printf("LiveView: open failed: %s\n", wifiPreview.lastError().c_str());
  showStatusIfChanged("LiveView failed", wifiPreview.lastError(), "Back to BLE scan", "", true);
}

void onLiveViewOpenedForController() {
  lastFrameAt = millis();
  lastLiveViewActivityAt = lastFrameAt;
  liveViewStallRecycleCount = 0;
  previewFrameBuffer.resetRuntimeStats();
  if (cameraProps.ok) {
    deferredPropsRefreshAfter = 0;
    Serial.println("LiveView: connected; props already loaded");
    return;
  }
  deferredPropsRefreshAfter = lastFrameAt + INITIAL_PROPS_REFRESH_DELAY_MS;
  Serial.printf("LiveView: connected; props deferred until first frame + %lums\n",
                static_cast<unsigned long>(INITIAL_PROPS_REFRESH_DELAY_MS));
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

void showRecoveryBleReadyRetryForController(const char* reason) {
  showStatusIfChanged("Camera recovery", reason != nullptr ? reason : "reconnect", "BLE_READY retry", "", true);
}

void showRecoveryBleScanForController(const char* reason) {
  showStatusIfChanged("Camera recovery", reason != nullptr ? reason : "reconnect", "Back to BLE scan", "", true);
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
  return shutterUsesHttp() ? grWifi.isConnected() : bleCamera.shutterReady();
}

void showShutterBleNotReadyForController() {
  g_host->onShutterResult(false);
  const bool overHttp = shutterUsesHttp();
  const char* hint = "Back to BLE scan";
  if (isGr2WifiOnlyProfile()) {
    hint = "Enable GR II WiFi";
  } else if (overHttp) {
    hint = "Portrait: BLE shutter";
  }
  showStatusIfChanged("Button A shutter",
                      overHttp ? "WiFi not ready" : "BLE not ready",
                      hint,
                      "",
                      true);
}

bool retryShutterAfterGr3StartupMode() {
  if (!RICOH_BLE_GR3_REQUEST_CAPTURE_IN_STARTUP_MODE || !bleCamera.isConnected()) {
    return false;
  }
  const String shootError = bleCamera.lastError();
  RicohCameraOperationMode mode = RicohCameraOperationMode::Unknown;
  if (bleCamera.readOperationMode(mode).failed() ||
      !operationModeWaitsForCameraReady(bleCamera.protocolProfile(), mode, true)) {
    return false;
  }
  Serial.printf("Button A: shutter refused in BLE_STARTUP (%s); requesting CAPTURE\n", shootError.c_str());
  if (bleCamera.requestCaptureOperationMode().failed()) {
    Serial.printf("BLE: capture mode request skipped: %s\n", bleCamera.lastError().c_str());
    return false;
  }

  const uint32_t waitStartedAt = millis();
  while (millis() - waitStartedAt < RICOH_BLE_GR3_STARTUP_MODE_WAIT_MS) {
    delay(RICOH_BLE_GR3_STARTUP_MODE_POLL_MS / 4);
    yield();
    if (!bleCamera.isConnected() || bleCamera.readOperationMode(mode).failed()) {
      return false;
    }
    if (!operationModeWaitsForCameraReady(bleCamera.protocolProfile(), mode, true)) {
      Serial.printf("Button A: camera left BLE_STARTUP after %lums mode=%s; retrying shutter\n",
                    static_cast<unsigned long>(millis() - waitStartedAt),
                    cameraOperationModeName(mode));
      shotConfirm.eventSequenceBeforeShot = bleCamera.shotEventSequence();
      return bleCamera.shoot(true).ok();
    }
  }
  Serial.println("Button A: camera stayed in BLE_STARTUP after CAPTURE request");
  return false;
}

bool shootAutofocusForController() {
  g_host->onShutterStarted();
  if (isGr2WifiOnlyProfile()) {
    Serial.println("Button A: GR II shutter over official GR Remote HTTP sequence");
    return grApi.shootGr2(PROPS_TIMEOUT_MS);
  }
  if (shutterUsesHttp()) {
    // Failures are reported by onShutterFailedForController.
    Serial.printf("Button A: landscape shutter over HTTP body=%s (BLE %s)\n",
                  LIVEVIEW_SHOOT_AF_ON_BODY,
                  bleParkedForPreview ? "released" : "bringing up WiFi");
    return grApi.shoot(HTTP_SHOOT_TIMEOUT_MS);
  }
  shotConfirm.eventSequenceBeforeShot = bleCamera.shotEventSequence();
  const rvf::Result shootResult = bleCamera.shoot(true);
  if (shootResult.ok()) {
    return true;
  }
  return retryShutterAfterGr3StartupMode();
}

void onShutterOkForController() {
  g_host->onShutterResult(true);
  const bool overHttp = shutterUsesHttp();
  shotConfirm.pending = !overHttp && bleCamera.shotNotifyActive();
  shotConfirm.startedAt = millis();
  showStatusIfChanged("Button A shutter",
                      overHttp ? "HTTP SHOT OK" : (shotConfirm.pending ? "BLE SHOT SENT" : "BLE SHOT OK"),
                      cameraProps.model,
                      cameraProps.battery,
                      true);
  updateUi();
}

bool waitForLiveViewJpegFrame(uint32_t timeoutMs) {
  const uint32_t startedAt = millis();
  const uint32_t frameAtBefore = lastFrameAt;
  while (millis() - startedAt < timeoutMs) {
    if (!grWifi.isConnected() || !wifiPreview.isPreviewRunning()) {
      return false;
    }
    const int readLen = wifiPreview.readFrame(streamReadBuffer, sizeof(streamReadBuffer));
    if (readLen > 0) {
      lastLiveViewActivityAt = millis();
      wifiPreview.processFrameData(streamReadBuffer, static_cast<size_t>(readLen));
      if (lastFrameAt != frameAtBefore) {
        return true;
      }
    } else if (readLen < 0) {
      return false;
    }
    delay(10);
    yield();
  }
  return false;
}

bool reopenPreviewAfterHttpShutterForController() {
  if (!grWifi.isConnected()) {
    Serial.println("HTTP: LiveView reopen after shutter skipped; WiFi down");
    return false;
  }

  // af=on returns HTTP success in tens of milliseconds while AF/capture still
  // runs. GET /v1/liveview during that window is HTTP 200 with no JPEG, and
  // the stall watchdog then tears down BLE. Wait out the capture, then treat
  // a JPEG as success, not the headers.
  constexpr uint32_t kCaptureSettleMs = 2000;
  constexpr uint8_t kReopenAttempts = 4;
  constexpr uint32_t kJpegWaitMs = 1500;
  constexpr uint32_t kRetryDelayMs = 400;
  Serial.printf("HTTP: recycling LiveView after shutter; settle=%lums\n",
                static_cast<unsigned long>(kCaptureSettleMs));
  wifiPreview.stopPreview();
  delay(kCaptureSettleMs);
  yield();

  for (uint8_t attempt = 1; attempt <= kReopenAttempts; ++attempt) {
    if (!grWifi.isConnected()) {
      Serial.println("HTTP: LiveView reopen after shutter aborted; WiFi down");
      return false;
    }
    const rvf::Result previewResult = wifiPreview.startPreview();
    if (previewResult.ok()) {
      previewFrameBuffer.resetRuntimeStats();
      if (waitForLiveViewJpegFrame(kJpegWaitMs)) {
        liveViewStallRecycleCount = 0;
        Serial.printf("HTTP: LiveView JPEG after shutter attempt=%u\n",
                      static_cast<unsigned>(attempt));
        return true;
      }
      Serial.printf("HTTP: LiveView HTTP 200 but no JPEG after shutter attempt=%u\n",
                    static_cast<unsigned>(attempt));
      wifiPreview.stopPreview();
    } else {
      Serial.printf("HTTP: LiveView reopen after shutter attempt=%u failed: %s\n",
                    static_cast<unsigned>(attempt),
                    wifiPreview.lastError().c_str());
    }
    delay(kRetryDelayMs);
    yield();
  }
  return false;
}

bool recyclePreviewAfterStallForController() {
  if (!grWifi.isConnected() || !appController.previewRequested()) {
    return false;
  }
  constexpr uint8_t kMaxStallRecycles = 2;
  if (liveViewStallRecycleCount >= kMaxStallRecycles) {
    Serial.println("HTTP: LiveView stall recycle exhausted");
    return false;
  }
  liveViewStallRecycleCount++;
  Serial.printf("HTTP: LiveView stall recycle %u/%u\n",
                static_cast<unsigned>(liveViewStallRecycleCount),
                static_cast<unsigned>(kMaxStallRecycles));
  return reopenPreviewAfterHttpShutterForController();
}

void serviceShotConfirmation(uint32_t nowMs) {
  if (!shotConfirm.pending) {
    return;
  }
  const uint32_t elapsed = nowMs - shotConfirm.startedAt;
  if (bleCamera.shotEventSequence() != shotConfirm.eventSequenceBeforeShot) {
    shotConfirm.pending = false;
    Serial.printf("BLE: shot confirmed by camera notify after %lums\n", static_cast<unsigned long>(elapsed));
    showStatusIfChanged("Button A shutter", "SHOT CONFIRMED", cameraProps.model, cameraProps.battery, true);
    return;
  }
  if (!bleCamera.isConnected()) {
    shotConfirm.pending = false;
    return;
  }
  if (elapsed >= SHOT_CONFIRM_TIMEOUT_MS) {
    shotConfirm.pending = false;
    Serial.printf("BLE: shot not confirmed within %lums (no ShotCount/CaptureStatus notify)\n",
                  static_cast<unsigned long>(elapsed));
    showStatusIfChanged("Button A shutter", "Sent, not confirmed", "Check camera", "", true);
  }
}

// Status notifications are subscribed once per BLE connection, from the tick
// rather than the connect flow, because portrait resume with cached Wi-Fi
// credentials never passes through the power check that enables power notify.
void serviceCameraStatusNotify() {
  // Landscape is about to release the link; its battery comes from /v1/props.
  if (!RICOH_BLE_SUBSCRIBE_CAMERA_STATUS || !bleCamera.isConnected() || shutterUsesHttp()) {
    return;
  }
  // Normally done before WLAN ON. This fallback covers links that never went
  // through it (portrait with cached Wi-Fi), and never runs while the MJPEG
  // stream is open: the multi-second discovery would stall the preview.
  if (!wifiPreview.isPreviewRunning()) {
    subscribeCameraStatusNotifyIfNeeded("BLE shutter scene");
  }
  const int battery = bleCamera.batteryLevel();
  if (battery >= 0 && battery != cameraBleBatteryShown) {
    cameraBleBatteryShown = battery;
    if (battery == 0) {
      // A GR IIIx sent 0 about a second before locking up with charge left,
      // so 0 reads as a failing camera rather than an empty battery.
      Serial.println("BLE: camera reported battery 0; camera may be failing");
      return;
    }
    cameraProps.battery = battery <= 100 ? String(battery) + "%" : String("raw ") + battery;
  }
}

void onShutterFailedForController() {
  // The shot goes over BLE in portrait and over HTTP in landscape, so report
  // whichever one actually ran; bleCamera's error is stale in the HTTP case
  // and would send anyone reading the screen down the wrong path.
  const bool overHttp = shutterUsesHttp();
  const String& error = overHttp ? grApi.lastError() : bleCamera.lastError();
  Serial.printf("Button A: %s shutter failed: %s\n", overHttp ? "HTTP" : "BLE", error.c_str());
  g_host->onShutterResult(false);
  showStatusIfChanged(overHttp ? "Button A HTTP failed" : "Button A BLE failed",
                      error,
                      "Preview kept",
                      "",
                      true);
  updateUi();
}

bool previewKeptAfterShutterFailureForController() {
  return appController.isPreviewActive() && grWifi.isConnected() && wifiPreview.isPreviewRunning();
}

void showCameraSleepGuardStatusForController() {
  showCameraSleepGuardStatus(false);
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

void logLinkStatsIfDue(uint32_t nowMs) {
  if (grWifi.isConnected() && (nowMs - linkStats.rssiLastSampleMs) >= LINK_RSSI_SAMPLE_INTERVAL_MS) {
    linkStats.rssiLastSampleMs = nowMs;
    const int32_t rssi = grWifi.rssi();
    if (rssi != 0) {
      linkStats.rssiSum += rssi;
      linkStats.rssiMin = linkStats.rssiSamples == 0 ? rssi : (rssi < linkStats.rssiMin ? rssi : linkStats.rssiMin);
      ++linkStats.rssiSamples;
    }
  }
  if ((nowMs - linkStats.lastLogMs) < LINK_STATS_LOG_INTERVAL_MS) {
    return;
  }
  linkStats.lastLogMs = nowMs;

  const int rssiAvg = linkStats.rssiSamples > 0 ? static_cast<int>(linkStats.rssiSum / linkStats.rssiSamples) : 0;
  const int lastReason = grWifi.lastDisconnectReason();
  BleConnParams ble;
  const bool bleUp = bleCamera.readConnParams(ble);
  Serial.printf("LINK: rssi_avg=%d rssi_min=%d samples=%u ch=%u tx=%.2fdBm wifi_disc=%lu last_reason=%d(%s) "
                "ble=%s mode=%s itvl=%.2fms lat=%u to=%ums peer_req=%lu stall_recycle=%u\n",
                rssiAvg,
                static_cast<int>(linkStats.rssiMin),
                static_cast<unsigned>(linkStats.rssiSamples),
                static_cast<unsigned>(grWifi.channel()),
                grWifi.txPowerDbm(),
                static_cast<unsigned long>(grWifi.disconnectCount()),
                lastReason,
                GrWifi::disconnectReasonName(lastReason),
                bleUp ? "up" : "down",
                bleLinkModeName(bleLinkMode.applied),
                bleUp ? ble.maxInterval * 1.25f : 0.0f,
                bleUp ? static_cast<unsigned>(ble.latency) : 0U,
                bleUp ? static_cast<unsigned>(ble.supervisionTimeout) * 10U : 0U,
                static_cast<unsigned long>(bleCamera.peerConnParamRequests()),
                static_cast<unsigned>(liveViewStallRecycleCount));
  linkStats.rssiSum = 0;
  linkStats.rssiMin = 0;
  linkStats.rssiSamples = 0;
}

void logPreviewStatsForController() {
  logLinkStatsIfDue(millis());
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

bool reasonRequiresBleRescan(const char* reason) {
  if (isGr2WifiOnlyProfile()) {
    return true;
  }
  if (reason == nullptr) {
    return !bleCamera.isConnected();
  }
  const String text(reason);
  return !bleCamera.isConnected() || text.indexOf("BLE disconnected") >= 0 || text.indexOf("BLE not ready") >= 0;
}

void disconnectWifiLiveViewToBleReady(const char* reason) {
  closeLiveView(reason != nullptr ? reason : "BLE_READY reset");
  wifiPreview.disconnectWifi();
  (void)deactivateCameraWifiOverBle();
  if (bleCamera.isConnected()) {
    setCameraFlowState(CameraFlowState::BleReady, reason != nullptr ? reason : "BLE still connected");
  } else {
    setCameraFlowState(CameraFlowState::BleScan, reason != nullptr ? reason : "BLE lost");
  }
}

void disconnectAllTransportsToBleScan(const char* reason) {
  closeLiveView(reason != nullptr ? reason : "flow reset");
  wifiPreview.disconnectWifi();
  unparkBleIfNeeded(reason != nullptr ? reason : "flow reset");
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, reason);
}

bool resetBleStackBeforeScanAfterLinkLoss(const char* reason) {
  if (isGr2WifiOnlyProfile()) {
    (void)reason;
    return true;
  }
  Serial.printf("BLE recovery: reset stack (%s)\n", reason != nullptr ? reason : "link lost");
  showStatusIfChanged("BLE stack reset", "Camera link lost", preferredBleName(), "Scanning soon", true);
  if (bleCamera.resetStack().failed()) {
    Serial.printf("BLE recovery: stack reset failed: %s\n", bleCamera.lastError().c_str());
    return false;
  }
  delay(BLE_RECOVERY_STACK_RESET_GRACE_MS);
  yield();
  return true;
}

void delayAndYield(uint32_t delayMs) {
  delay(delayMs);
  yield();
}

void disconnectWifiForController() {
  wifiPreview.disconnectWifi();
}

bool finishCameraWifiOverHttpForController() {
  if (!grWifi.isConnected()) {
    return false;
  }
  closeLiveView("portrait WLAN finish");
  if (isGr2WifiOnlyProfile()) {
    // GR Remote's GR II /v1/device/finish endpoint powers the camera off; it
    // is not the GR III/IV WLAN handoff endpoint. Leave the manually enabled
    // AP running and only close the local stream when needed.
    return true;
  }
  const bool requested = grApi.finishWlan(WLAN_FINISH_TIMEOUT_MS);
  Serial.printf("HTTP WLAN finish: request=%s detail=%s\n",
                requested ? "SENT" : "FAILED",
                requested ? "camera disconnect expected" : grApi.lastError().c_str());
  return requested;
}

void clearBleDisconnectReasonForController() {
  bleCamera.clearDisconnectReason();
}

bool connectCachedWifiFromProfileForController() {
  if (isGr2WifiOnlyProfile()) {
    Serial.printf("GR II WiFi: joining manually enabled AP ssid='%s' timeout=%lums\n",
                  cameraProfile.wifi.ssid.c_str(),
                  static_cast<unsigned long>(WIFI_CONNECT_TIMEOUT_MS));
    return connectWifiFromProfile(true, true, WIFI_CONNECT_TIMEOUT_MS, true);
  }
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
  if (isGr2WifiOnlyProfile()) {
    pendingFreshWifiCredentials = RicohBleWifiCredentials{};
    pendingFreshWifiCredentials.ssid = cameraProfile.wifi.ssid;
    pendingFreshWifiCredentials.passphrase = cameraProfile.wifi.passphrase;
    pendingFreshWifiCredentials.bssid = cameraProfile.wifi.bssid;
    pendingFreshWifiCredentials.channel = cameraProfile.wifi.channel;
    pendingFreshWifiCredentials.frequencyMhz = cameraProfile.wifi.frequencyMhz;
    pendingFreshWifiCredentials.valid = cameraProfile.wifi.credentialsValid;
    return pendingFreshWifiCredentials.valid;
  }
  const rvf::Result wifiCredentialsResult = bleCamera.waitForWifiCredentials(pendingFreshWifiCredentials, RICOH_BLE_WIFI_CREDENTIAL_WAIT_MS);
  if (wifiCredentialsResult.failed()) {
    showStatusIfChanged("BLE WiFi params", bleCamera.lastError(), "Back to BLE_READY", "", true);
    return false;
  }
  return true;
}

void applyFreshWifiCredentialsForController() {
  if (isGr2WifiOnlyProfile()) {
    cameraProfile.wifi.source = WifiCredentialSource::ManualConfiguration;
    cameraProfile.wifi.cached = cameraProfile.wifi.credentialsValid;
    saveWifiCredentialCache("GR II web setup");
    return;
  }
  // Persist before any station connection attempt so portrait mode can stop
  // here and resume from the same parameters when the device turns landscape.
  applyBleWifiCredentials(pendingFreshWifiCredentials, "fresh BLE", true);
}

bool connectFreshWifiFromProfileForController() {
  return connectWifiFromProfile(true, !isGr2WifiOnlyProfile());
}

void onFreshWifiConnectedForController() {
  saveConnectedWifiBssidToCache("fresh BLE connect");
}

rvf::AppFlowActions makeAppFlowActions() {
  rvf::AppFlowActions actions;
  actions.cameraSleepGuardBlocksFlow = cameraSleepGuardBlocksFlow;
  actions.runBleDiscovery = runBleDiscoveryAtBoot;
  actions.cameraSleepGuardActive = cameraSleepGuardActive;
  actions.ensureCameraPowerReady = ensureCameraPowerReadyForController;
  actions.activateCameraWifiOverBle = activateCameraWifiOverBle;
  actions.finishCameraWifiOverHttp = finishCameraWifiOverHttpForController;
  actions.deactivateCameraWifiOverBle = deactivateCameraWifiOverBle;
  actions.reactivateCameraWifiForCachedResume = reactivateCameraWifiForCachedResume;
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
  actions.showStartingLiveView = showStartingLiveViewForController;
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
  actions.restoreReleasedBle = restoreReleasedBleForRecovery;
  actions.reasonRequiresBleRescan = reasonRequiresBleRescan;
  actions.showRecoveryBleReadyRetry = showRecoveryBleReadyRetryForController;
  actions.showRecoveryBleScan = showRecoveryBleScanForController;
  actions.resetBleStackBeforeScanAfterLinkLoss = resetBleStackBeforeScanAfterLinkLoss;
  actions.shortRecoveryDelay = shortRecoveryDelayForController;
  actions.onRecoveryGuardBlocked = onRecoveryGuardBlockedForController;
  actions.onRecoveryFinished = onRecoveryFinishedForController;
  actions.requestManualCameraWake = requestManualCameraWakeForController;
  actions.shutterReady = shutterReadyForController;
  actions.shutterOverWifi = shutterUsesHttp;
  actions.showShutterBleNotReady = showShutterBleNotReadyForController;
  actions.shootAutofocus = shootAutofocusForController;
  actions.onShutterOk = onShutterOkForController;
  actions.onShutterFailed = onShutterFailedForController;
  actions.previewKeptAfterShutterFailure = previewKeptAfterShutterFailureForController;
  actions.reopenPreviewAfterHttpShutter = reopenPreviewAfterHttpShutterForController;
  actions.recyclePreviewAfterStall = recyclePreviewAfterStallForController;
  actions.showCameraSleepGuardStatus = showCameraSleepGuardStatusForController;
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

void refreshPropsIfDue(bool force) {
  if (isGr2WifiOnlyProfile()) {
    // The official GR Remote uses /v1/params/device rather than /v1/props.
    // The current UI only needs a stable model label here; avoid generating a
    // guaranteed 404 once per minute on the GR II.
    cameraProps.ok = true;
    cameraProps.model = "RICOH GR II";
    cameraProps.battery = "";
    lastPropsAt = millis();
    deferredPropsRefreshAfter = 0;
    return;
  }
  const uint32_t now = millis();
  if (!force && deferredPropsRefreshAfter != 0) {
    const bool firstFrameRendered = previewFrameBuffer.stats().renderedFrames > 0;
    if (!firstFrameRendered || !timeReached(deferredPropsRefreshAfter)) {
      return;
    }
    deferredPropsRefreshAfter = 0;
  }
  if (!force && cameraProps.ok && (now - lastPropsAt) < PROPS_REFRESH_INTERVAL_MS) {
    return;
  }
  if (!force && cameraProps.ok && bleCamera.isConnected() && bleCamera.batteryLevel() >= 0) {
    // Battery arrives over BLE notify and the model does not change, so a
    // 16 KB /v1/props read would only take airtime from the stream.
    return;
  }
  if (!grWifi.isConnected()) {
    return;
  }

  CameraProps nextProps;
  if (wifiPreview.fetchProps(nextProps, PROPS_TIMEOUT_MS).ok()) {
    cameraProps = nextProps;
    lastPropsAt = now;
    cameraBleBatteryShown = -1;
  } else if (force) {
    Serial.printf("HTTP: props refresh failed: %s\n", wifiPreview.lastError().c_str());
  }
}

void resetBlePairingFromKey2() {
  static bool resettingPairing = false;
  if (resettingPairing) {
    return;
  }
  resettingPairing = true;
  const bool resettingGr2 = isGr2WifiOnlyProfile() || gr2Provisioning.active();
  if (gr2Provisioning.active()) {
    gr2Provisioning.stop();
  }
  uiResettingPairing = true;
  g_host->onPairingResetStarted();
  cameraRecoveryInProgress = true;
  g_host->releaseLiveViewLock();
  unparkBleIfNeeded("pairing reset");

  Serial.println(resettingGr2
                     ? "GR II WLAN reset: Button B / KEY2 long press"
                     : "BLE pairing reset: Button B / KEY2 long press");
  showStatusIfChanged(resettingGr2 ? "Reset GR II WLAN" : "Reset pairing",
                      resettingGr2 ? "Clearing profile..." : "Clearing BLE...",
                      "", "", true);

  closeLiveView("Reset pairing");
  wifiPreview.disconnectWifi();
  if (!resettingGr2) {
    bleCamera.disconnect();
  }
  clearCameraSleepGuard("Reset pairing");

  const bool profileCleared = g_profileStore->clearBlePairing();
  Serial.printf("Profile: BLE pairing keys cleared ok=%d\n", profileCleared ? 1 : 0);

  const String cameraIp = cameraProfile.wifi.cameraIp.length() > 0 ? cameraProfile.wifi.cameraIp : String(GR_HOST);
  cameraProfile = CameraProfile{};
  cameraProfile.wifi.cameraIp = cameraIp;
  wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
  pendingFreshWifiCredentials = RicohBleWifiCredentials{};
  wifiCacheRefresh.pending = false;
  cameraProps = CameraProps{};
  lastPropsAt = 0;
  deferredPropsRefreshAfter = 0;
  liveViewResolutionSelected = false;
  cameraBleBatteryShown = -1;
  grApi.setLiveViewResolution(LiveViewResolution{});
  liveviewEnabled = true;
  bleCamera.setBindingState(CameraBindingState::Unpaired);
  if (!resettingGr2) {
    const rvf::Result securityProfileResult =
        bleCamera.setSecurityProfile(RicohSecurityProfileId::Unknown);
    if (securityProfileResult.failed()) {
      Serial.printf("BLE pairing reset: security profile reset failed: %s\n", bleCamera.lastError().c_str());
      showStatusIfChanged("Reset pairing failed", bleCamera.lastError(), "Restart StickS3", "", true);
      cameraRecoveryInProgress = false;
      uiResettingPairing = false;
      resettingPairing = false;
      updateUi();
      return;
    }

    const rvf::Result deleteBondsResult = bleCamera.deleteAllBonds();
    if (deleteBondsResult.failed()) {
      Serial.printf("BLE pairing reset: NimBLE bond delete failed: %s\n", bleCamera.lastError().c_str());
    }
    bleCamera.clearDisconnectReason();
    if (bleCamera.resetStack(true).failed()) {
      Serial.printf("BLE pairing reset: stack rebuild failed: %s\n", bleCamera.lastError().c_str());
      showStatusIfChanged("Reset pairing failed", bleCamera.lastError(), "Restart StickS3", "", true);
      cameraRecoveryInProgress = false;
      uiResettingPairing = false;
      resettingPairing = false;
      updateUi();
      return;
    }
  } else {
    Serial.println("GR II reset: cleared WLAN profile without initializing BLE");
  }

  pairingModelConfirmed = false;
  requestPairingGuide(resettingGr2
                            ? RicohProtocolGeneration::Gr2Family
                            : RicohProtocolGeneration::Gr3Family);
  showStatusIfChanged("Select camera", "B: model", "A: confirm", "", true);
  setCameraFlowState(CameraFlowState::BleScan, "Reset pairing");
  lastFrameAt = millis();
  lastLiveViewActivityAt = lastFrameAt;
  lastCameraRecoveryAt = 0;
  cameraRecoveryInProgress = false;
  uiResettingPairing = false;
  resettingPairing = false;
  updateUi();
}

bool beginSelectedCameraPairing(RicohProtocolGeneration generation) {
  selectedPairingGeneration = generation;
  const RicohSecurityProfileId securityProfile = securityProfileForGeneration(generation);
  Serial.printf("Camera guide: confirmed model=%s security=%s\n",
                ricohProtocolGenerationName(generation),
                ricohSecurityProfileName(securityProfile));

  if (generation == RicohProtocolGeneration::Gr2Family) {
    gr2ProvisioningPreparing = true;
    appController.setPreviewRequested(false);
    setCameraFlowState(CameraFlowState::WifiCredentialsReady, "GR II setup preparing");
    showStatusIfChanged("GR II WiFi setup", "Scanning camera WiFi",
                        "Please wait...", "No Bluetooth", true);
    updateUi();

    cameraProfile = CameraProfile{};
    cameraProfile.cameraName = "RICOH GR II";
    cameraProfile.protocolGeneration = RicohProtocolGeneration::Gr2Family;
    cameraProfile.protocolGenerationKnown = true;
    cameraProfile.capabilityVersion =
        cameraProtocolProfile(RicohProtocolGeneration::Gr2Family).capabilityVersion;
    cameraProfile.wifi.cameraIp = GR_HOST;
    cameraProfile.wifi.cached = false;
    cameraProfile.wifi.credentialsValid = false;
    cameraProfile.wifi.source = WifiCredentialSource::ManualConfiguration;
    cameraProfile.profileVersion = CAMERA_PROFILE_SCHEMA_VERSION;
    if (!g_profileStore->save(cameraProfile)) {
      gr2ProvisioningPreparing = false;
      showStatusIfChanged("GR II profile failed", "Could not save NVS", "Retry selection", "", true);
      requestPairingGuide(generation);
      pairingModelConfirmed = false;
      updateUi();
      return false;
    }

    wifiPreview.disconnectWifi();
    const bool portalStarted = gr2Provisioning.begin();
    gr2ProvisioningPreparing = false;
    if (!portalStarted) {
      Serial.printf("GR II setup: portal start failed: %s\n",
                    gr2Provisioning.lastError().c_str());
      showStatusIfChanged("GR II setup failed", gr2Provisioning.lastError(),
                          "Retry selection", "", true);
      requestPairingGuide(generation);
      pairingModelConfirmed = false;
      updateUi();
      return false;
    }

    cameraProps.ok = true;
    cameraProps.model = "RICOH GR II";
    cameraProps.battery = "";
    pairingModelConfirmed = true;
    appController.setPreviewRequested(false);
    setCameraFlowState(CameraFlowState::WifiCredentialsReady, "GR II web setup");
    showStatusIfChanged("GR II WiFi setup", gr2Provisioning.accessPointSsid(),
                        String("http://") + gr2Provisioning.accessPointIp(),
                        String("Password: ") + gr2Provisioning.accessPointPassword(), true);
    updateUi();
    return true;
  }

  const rvf::Result securityResult = bleCamera.setSecurityProfile(securityProfile);
  if (securityResult.failed()) {
    Serial.printf("BLE pairing guide: security profile apply failed: %s\n",
                  bleCamera.lastError().c_str());
    requestPairingGuide(generation);
    pairingModelConfirmed = false;
    updateUi();
    return false;
  }

  pairingModelConfirmed = true;
  setCameraFlowState(CameraFlowState::BleScan, "camera model confirmed");
  showStatusIfChanged("Pairing GR BLE",
                      ricohProtocolGenerationName(generation),
                      "Scanning...",
                      "",
                      true);
  const bool online = appController.runCameraFlowOnce(makeAppFlowActions(), millis());
  lastFrameAt = millis();
  lastCameraRecoveryAt = online ? millis() : 0;
  return online;
}

// Returns true when the input was consumed; the caller then refreshes the
// UI with no button events and ends the tick.
bool handleGr2ProvisioningInput(bool resetRequested) {
  if (resetRequested) {
    Serial.println("GR II setup: cancelled by Button B hold");
    gr2Provisioning.stop();
    gr2ProvisioningPreparing = false;
    (void)g_profileStore->clearBlePairing();
    cameraProfile = CameraProfile{};
    cameraProfile.wifi.cameraIp = GR_HOST;
    pairingModelConfirmed = false;
    requestPairingGuide(RicohProtocolGeneration::Gr2Family);
    setCameraFlowState(CameraFlowState::BleScan, "GR II setup cancelled");
    showStatusIfChanged("Select camera", "B: model", "A: confirm", "", true);
    return true;
  }

  Gr2ProvisionedWifi provisioned;
  if (gr2Provisioning.takeCredentials(provisioned)) {
    cameraProfile.wifi.ssid = provisioned.ssid;
    cameraProfile.wifi.passphrase = provisioned.passphrase;
    cameraProfile.wifi.bssid = "";
    cameraProfile.wifi.channel = 0;
    cameraProfile.wifi.frequencyMhz = 0;
    cameraProfile.wifi.cameraIp = GR_HOST;
    cameraProfile.wifi.cached = true;
    cameraProfile.wifi.credentialsValid = true;
    cameraProfile.wifi.source = WifiCredentialSource::ManualConfiguration;
    if (!g_profileStore->save(cameraProfile)) {
      Serial.println("GR II setup: failed to save provisioned credentials to NVS");
      showStatusIfChanged("GR II save failed", "Could not save NVS",
                          "Submit again", "", true);
      return true;
    }

    Serial.printf("GR II setup: saved profile for ssid='%s'\n",
                  cameraProfile.wifi.ssid.c_str());
    gr2Provisioning.stop();
    grWifi.begin();
    wifiPreview.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
    appController.setPreviewRequested(true);
    setCameraFlowState(CameraFlowState::BleScan, "GR II web setup complete");
    showStatusIfChanged("Connecting GR II", cameraProfile.wifi.ssid,
                        "Camera WiFi", "No Bluetooth", true);
    const bool online = appController.runCameraFlowOnce(makeAppFlowActions(), millis());
    lastFrameAt = millis();
    lastLiveViewActivityAt = lastFrameAt;
    lastCameraRecoveryAt = online ? millis() : 0;
    return true;
  }

  return false;
}

void requestManualCameraWake(const char* source) {
  const char* wakeSource = source != nullptr ? source : "manual retry";
  g_host->onManualWake();
  clearCameraSleepGuard(wakeSource);
  liveviewEnabled = true;
  closeLiveView(wakeSource);
  wifiPreview.disconnectWifi();
  unparkBleIfNeeded(wakeSource);
  bleCamera.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, wakeSource);
  showStatusIfChanged("Manual retry", "Resetting BLE...", preferredBleName(), "", true);

  Serial.printf("BLE guard: manual retry BLE stack rebuild (%s)\n", wakeSource);
  if (bleCamera.resetStack(true).failed()) {
    Serial.printf("BLE guard: manual retry stack rebuild failed: %s\n", bleCamera.lastError().c_str());
    showStatusIfChanged("Manual retry failed", bleCamera.lastError(), "Restart StickS3", "", true);
    return;
  }
  delay(BLE_MANUAL_WAKE_REINIT_SETTLE_MS);
  yield();

  showStatusIfChanged("Manual retry", "Checking camera...", preferredBleName(), "", true);
  const bool online = appController.runCameraFlowOnce(makeAppFlowActions(), millis());
  lastFrameAt = millis();
  lastCameraRecoveryAt = online ? millis() : 0;
}

void updateStatusUiIfDue() {
  const uint32_t now = millis();
  if ((now - lastStatusUiUpdateAt) < UI_STATUS_INTERVAL_MS) {
    return;
  }
  lastStatusUiUpdateAt = now;

  if (cameraSleepGuardActive()) {
    showCameraSleepGuardStatus(false);
    return;
  }

  if (!wifiPreview.isPreviewRunning()) {
    showStatusIfChanged(grWifi.statusText(),
                        liveviewEnabled ? grWifi.localIPString() : "Preview paused",
                        cameraProps.model,
                        cameraProps.battery);
  }
}

// Releases the BLE link as soon as the landscape preview streams, and takes it
// back when the preview stops. The camera flow is synchronous, so by the time
// this tick runs the Wi-Fi and HTTP bring-up that needed BLE has finished.
void serviceBleParking(uint32_t nowMs) {
  // A disconnect is not instant, so do not read the link as "back up" in the
  // moment right after releasing it.
  constexpr uint32_t kParkSettleMs = 1000;

  if (bleParkedForPreview && bleCamera.isConnected() &&
      (nowMs - bleParkedAtMs) >= kParkSettleMs) {
    // Parked describes a link this code released and which is therefore down.
    // If it is up again the flag is stale, and staleness here is silent: the
    // preview keeps working while quietly paying the radio contention that
    // parking exists to avoid, and the shutter goes over HTTP for no reason.
    // Seen after a pairing reset, whose reconnect runs while the tick that
    // would have unparked is blocked inside the BLE flow.
    unparkBleIfNeeded("BLE link back up");
  }

  const bool previewStreaming =
      appController.isPreviewActive() && grWifi.isConnected() && wifiPreview.isPreviewRunning();

  if (!previewStreaming) {
    if (bleParkedForPreview) {
      restoreBleAfterPreviewPark(appController.previewRequested()
                                     ? "preview transport stopped"
                                     : "portrait handoff");
    }
    return;
  }

  if (bleParkedForPreview || !appController.previewRequested()) {
    // A portrait turn leaves the stream up for one tick before the flow
    // closes it; releasing BLE then would only force an extra reconnect.
    return;
  }
  if (!bleCamera.isConnected()) {
    return;  // Nothing to release.
  }
  if (!cameraSupportsWifiOnlyLandscape()) {
    // Releasing BLE moves the shutter onto POST /v1/camera/shoot. A
    // generation that has not opted in keeps BLE through the preview.
    return;
  }

  if (wifiCacheRefresh.pending) {
    // These cached parameters just joined the access point; their BLE re-read
    // is not worth holding the link open in landscape.
    wifiCacheRefresh.pending = false;
    Serial.println("WiFi cache: deferred BLE refresh skipped; landscape is WiFi-only");
  }
  Serial.println("BLE: releasing link; landscape preview runs on WiFi only");
  bleParkedForPreview = true;
  bleParkedAtMs = nowMs;
  bleCamera.disconnect();
}

void serviceBleLinkMode(uint32_t nowMs) {
  const bool streaming =
      appController.isPreviewActive() && grWifi.isConnected() && wifiPreview.isPreviewRunning();
  if (streaming && !bleLinkMode.previewStreaming) {
    bleLinkMode.previewStreamingSinceMs = nowMs;
  }
  bleLinkMode.previewStreaming = streaming;

  if (!BLE_LIVEVIEW_LOW_DUTY_ENABLED) {
    return;
  }
  if (!bleCamera.isConnected()) {
    // Every new link opens with the Normal connect-time parameters.
    bleLinkMode.applied = BleLinkMode::Normal;
    bleLinkMode.retryAfterMs = 0;
    return;
  }
  // A disconnect + reconnect can complete inside one blocking recovery call
  // without this function ever seeing the link down; the new link is on the
  // Normal connect-time parameters whatever was applied to the old one.
  const uint32_t connectionSerial = bleCamera.connectionSerial();
  if (connectionSerial != bleLinkMode.connectionSerial) {
    bleLinkMode.connectionSerial = connectionSerial;
    bleLinkMode.applied = BleLinkMode::Normal;
    bleLinkMode.retryAfterMs = 0;
  }

  BleLinkModeInputs inputs;
  inputs.bleConnected = true;
  inputs.previewRequested = appController.previewRequested();
  inputs.previewStreaming = streaming;
  inputs.streamingSinceMs = bleLinkMode.previewStreamingSinceMs;
  inputs.nowMs = nowMs;
  const BleLinkMode desired =
      desiredBleLinkMode(bleLinkMode.applied, inputs, BLE_LIVEVIEW_LOW_DUTY_SETTLE_MS);
  if (desired == bleLinkMode.applied) {
    return;
  }
  if (bleLinkMode.retryAfterMs != 0 && static_cast<int32_t>(nowMs - bleLinkMode.retryAfterMs) < 0) {
    return;
  }

  const rvf::Result result = bleCamera.applyConnParams(
      bleConnParamsFor(desired),
      desired == BleLinkMode::LiveViewLowDuty ? "LiveView streaming" : "LiveView left");
  if (result.ok()) {
    Serial.printf("BLE: link mode %s -> %s\n",
                  bleLinkModeName(bleLinkMode.applied),
                  bleLinkModeName(desired));
    bleLinkMode.applied = desired;
    bleLinkMode.retryAfterMs = 0;
    return;
  }
  Serial.printf("BLE: link mode %s failed: %s\n",
                bleLinkModeName(desired),
                bleCamera.lastError().c_str());
  bleLinkMode.retryAfterMs = nowMs + BLE_LINK_MODE_RETRY_MS;
  if (bleLinkMode.retryAfterMs == 0) {
    bleLinkMode.retryAfterMs = 1;
  }
}

void onMjpegFrame(const uint8_t* data, size_t len, void*) {
  lastFrameAt = millis();
  liveViewStallRecycleCount = 0;
  previewFrameBuffer.recordFrame(len);
  const rvf::PreviewRenderResult render = g_host->renderPreviewFrame(data, len);
  if (render.attempted) {
    wifiPreview.recordRenderedFrame(render.decodeMs, render.renderMs, render.width, render.height);
  }
  lastFrameAt = millis();
}

}  // namespace

namespace rvf {

RicohDriver::RicohDriver(CameraProfileStore& profileStore) {
  g_profileStore = &profileStore;
}

void RicohDriver::begin(ICameraHost& host) {
  g_host = &host;
  appController.begin(CameraFlowState::BleScan);
  ricohBle.setServiceCallback(serviceButtonsDuringBleOperation);
  ricohBle.setPasskeyPoller(pollPasskeyButtonEntry);
  grWifi.begin();

  applyDefaultProfile();
  if (!hasConfiguredCameraProfile()) {
    pairingModelConfirmed = false;
    requestPairingGuide(isGr2WifiOnlyProfile()
                            ? RicohProtocolGeneration::Gr2Family
                            : RicohProtocolGeneration::Gr3Family);
  }
  if (isGr2WifiOnlyProfile()) {
    appController.setPreviewRequested(true);
    cameraProps.ok = true;
    cameraProps.model = "RICOH GR II";
  }
}

bool RicohDriver::attachPreviewBuffer(uint8_t* buffer,
                                      size_t capacity,
                                      PreviewFrameBufferStorage storage) {
  if (!previewFrameBuffer.attach(buffer, capacity, storage)) {
    return false;
  }
  mjpeg.begin(previewFrameBuffer.data(), previewFrameBuffer.capacity(), onMjpegFrame, nullptr);
  return true;
}

void RicohDriver::runStartupFlow(bool modelSelectionActive) {
  setupCameraFlowActive = true;
  const bool startupOnline = modelSelectionActive
                                 ? false
                                 : appController.runCameraFlowOnce(makeAppFlowActions(), millis());
  setupCameraFlowActive = false;
  lastCameraRecoveryAt = startupOnline ? millis() : 0;
  lastFrameAt = millis();
  lastLiveViewActivityAt = lastFrameAt;
  lastStatusUiUpdateAt = 0;
}

void RicohDriver::onModelSelectionShown() {
  setCameraFlowState(CameraFlowState::BleScan, "pairing guide");
}

bool RicohDriver::beginTick(uint32_t nowMs) {
  serviceBleParking(nowMs);
  serviceBleLinkMode(nowMs);
  tickPlan = appController.planTick(nowMs);
  return tickPlan.handleButtons;
}

bool RicohDriver::autoPowerOffDue() const {
  return cameraSleepAutoPowerOffDue(cameraSleepGuardActive(),
                                    sleepGuard.enteredAt,
                                    millis(),
                                    CAMERA_SLEEP_AUTO_POWER_OFF_MS);
}

void RicohDriver::serviceFlowTick() {
  const uint32_t controllerNow = millis();
  const AppFlowActions actions = makeAppFlowActions();
  if (tickPlan.serviceCameraFlow) {
    appController.serviceCameraFlowIfNeeded(actions, controllerNow);
  }
  if (tickPlan.monitorWifi) {
    appController.monitorWifi(actions);
  }
  serviceCameraStatusNotify();
  serviceShotConfirmation(millis());
  if (tickPlan.refreshProps) {
    refreshPropsIfDue();
  }
  if (tickPlan.monitorLiveView) {
    appController.monitorLiveView(actions, controllerNow);
  }
}

void RicohDriver::serviceBackgroundTick() {
  if (tickPlan.refreshWifiCache) {
    refreshWifiCacheIfDue();
  }
  if (tickPlan.updateStatusUi) {
    updateStatusUiIfDue();
  }
}

void RicohDriver::setUiPreviewIntent(bool landscape, bool modelSelectionActive) {
  // GR II has no low-power BLE shutter plane. Keep its Wi-Fi/HTTP stream alive
  // in portrait too so Button A remains usable; the UI already skips JPEG
  // decode while portrait and resumes rendering immediately after rotation.
  appController.setPreviewRequested(!modelSelectionActive &&
                                    !gr2ProvisioningPreparing &&
                                    !gr2Provisioning.active() &&
                                    (isGr2WifiOnlyProfile() || landscape));
}

bool RicohDriver::confirmModel(CameraModelId model) {
  return beginSelectedCameraPairing(static_cast<RicohProtocolGeneration>(model));
}

void RicohDriver::resetPairing() {
  resetBlePairingFromKey2();
}

void RicohDriver::shoot() {
  appController.handleUserCommand(makeAppFlowActions(), UserCommand::Shoot);
}

void RicohDriver::shutdown() {
  cameraRecoveryInProgress = true;
  closeLiveView("power off");
  wifiPreview.disconnectWifi();
  bleCamera.disconnect();
}

bool RicohDriver::provisioningActive() const {
  return gr2Provisioning.active();
}

void RicohDriver::serviceProvisioningPortal() {
  gr2Provisioning.loop();
}

void RicohDriver::stopProvisioning() {
  gr2Provisioning.stop();
}

bool RicohDriver::handleProvisioningInput(bool resetRequested) {
  return handleGr2ProvisioningInput(resetRequested);
}

bool RicohDriver::previewRunning() const {
  return wifiPreview.isPreviewRunning();
}

bool RicohDriver::previewActive() const {
  return appController.isPreviewActive();
}

CameraStatus RicohDriver::status() const {
  CameraStatus status;
  status.appState = appController.state();
  status.linkHealthy = bleLinkHealthy();
  status.wifiConnected = grWifi.isConnected();
  status.previewRunning = wifiPreview.isPreviewRunning();
  status.shutterReady = shutterReadyForController();
  status.sleepGuardActive = cameraSleepGuardActive();
  status.resettingPairing = uiResettingPairing;
  status.provisioningActive = gr2ProvisioningPreparing || gr2Provisioning.active();
  status.provisioningPreparing = gr2ProvisioningPreparing;
  status.liveviewEnabled = liveviewEnabled;
  status.recoveryInProgress = cameraRecoveryInProgress;
  status.rssi = grWifi.rssi();
  status.droppedFrames = mjpeg.droppedFrames();
  status.lastFrameAt = lastFrameAt;
  status.lastLiveViewActivityAt = lastLiveViewActivityAt;
  status.liveViewStallTimeoutMs = LIVEVIEW_STALL_TIMEOUT_MS;
  status.cameraModel = cameraProps.model.c_str();
  status.provisioningSsid = gr2Provisioning.accessPointSsid().c_str();
  status.provisioningPassword = gr2Provisioning.accessPointPassword().c_str();
  static String provisioningUrl;
  provisioningUrl = String("http://") + gr2Provisioning.accessPointIp();
  status.provisioningUrl = provisioningUrl.c_str();
  return status;
}

}  // namespace rvf
