#include <Arduino.h>
#include <esp_heap_caps.h>

#include "buttons.h"
#include "camera_profile_store.h"
#include "config.h"
#include "display.h"
#include "g11_button.h"
#include "gr_api.h"
#include "gr_wifi.h"
#include "jpeg_decoder.h"
#include "mjpeg_stream.h"
#include "ricoh_ble_client.h"

namespace {

GrWifi grWifi;
GrApi grApi;
MjpegStream mjpeg;
DisplayUi ui;
Buttons buttons;
G11Button g11Button;
JpegDecoder decoder;
CameraProfileStore profileStore;
CameraProfile cameraProfile;
RicohBleClient ricohBle;

enum class CameraFlowState {
  BleScan,
  CameraSleepGuard,
  BleReady,
  WifiConnecting,
  HttpProbe,
  LiveViewRunning,
};

CameraFlowState cameraFlowState = CameraFlowState::BleScan;
uint8_t* frameBuffer = nullptr;
uint8_t streamReadBuffer[STREAM_READ_BUFFER_SIZE];
CameraProps cameraProps;

bool liveviewEnabled = true;
uint32_t lastFrameAt = 0;
uint32_t lastStatusAt = 0;
uint32_t lastCameraRecoveryAt = 0;
bool cameraRecoveryInProgress = false;
bool cameraAutoWakeBlocked = false;
uint32_t cameraAutoWakeCooldownUntil = 0;
int cameraAutoWakeDisconnectReason = 0;
uint32_t lastPropsAt = 0;
uint32_t decodedFrames = 0;
uint32_t fpsWindowStart = 0;
uint32_t fpsWindowFrames = 0;
float currentFps = 0.0f;
uint32_t lastStatusDrawAt = 0;
String lastStatusLine1;
String lastStatusLine2;
String lastStatusLine3;
String lastStatusLine4;

constexpr uint32_t STATUS_MIN_REDRAW_MS = 1500;
constexpr bool DRAW_LIVE_OVERLAY = true;

const char* cameraFlowStateName(CameraFlowState state) {
  switch (state) {
    case CameraFlowState::BleScan:
      return "BLE_SCAN";
    case CameraFlowState::CameraSleepGuard:
      return "CAMERA_SLEEP_GUARD";
    case CameraFlowState::BleReady:
      return "BLE_READY";
    case CameraFlowState::WifiConnecting:
      return "WIFI_CONNECTING";
    case CameraFlowState::HttpProbe:
      return "HTTP_PROBE";
    case CameraFlowState::LiveViewRunning:
      return "LIVEVIEW_RUNNING";
  }
  return "UNKNOWN";
}

void setCameraFlowState(CameraFlowState state, const char* reason) {
  if (cameraFlowState != state) {
    Serial.printf("Flow: %s -> %s (%s)\n",
                  cameraFlowStateName(cameraFlowState),
                  cameraFlowStateName(state),
                  reason != nullptr ? reason : "");
  }
  cameraFlowState = state;
}

void showStatusIfChanged(const String& line1,
                         const String& line2 = String(),
                         const String& line3 = String(),
                         const String& line4 = String(),
                         bool force = false) {
  const uint32_t now = millis();
  const bool changed = force ||
                       line1 != lastStatusLine1 ||
                       line2 != lastStatusLine2 ||
                       line3 != lastStatusLine3 ||
                       line4 != lastStatusLine4;
  if (!changed) {
    return;
  }
  if (!force && (now - lastStatusDrawAt) < STATUS_MIN_REDRAW_MS) {
    return;
  }

  ui.showStatus(line1, line2, line3, line4);
  lastStatusLine1 = line1;
  lastStatusLine2 = line2;
  lastStatusLine3 = line3;
  lastStatusLine4 = line4;
  lastStatusDrawAt = now;
}

void waitForSerialConsole() {
  const uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < SERIAL_BOOT_WAIT_MS) {
    delay(10);
  }
  Serial.setDebugOutput(false);
  Serial.println();
  Serial.println("RICOH GR StickS3 Remote Viewfinder V2");
}

void closeLiveView(const char* reason) {
  if (grApi.isLiveViewOpen()) {
    Serial.printf("LiveView: closing (%s)\n", reason != nullptr ? reason : "reset");
  }
  grApi.closeLiveView();
  mjpeg.reset();
}

String deriveBleNameFromWifiSsid(const String& ssid) {
  if (!ssid.startsWith("GR_")) {
    return String();
  }

  const int prefixLen = ssid.startsWith("GR_H") ? 4 : 3;
  const String suffix = ssid.substring(prefixLen);
  if (suffix.length() == 0) {
    return ssid;
  }
  for (size_t i = 0; i < suffix.length(); ++i) {
    if (!isDigit(suffix[i])) {
      return ssid;
    }
  }

  const uint32_t serial = static_cast<uint32_t>(suffix.toInt());
  char name[20];
  snprintf(name,
           sizeof(name),
           "%s%0*lu",
           ssid.substring(0, prefixLen).c_str(),
           static_cast<int>(suffix.length()),
           static_cast<unsigned long>(serial + 1));
  return String(name);
}

String preferredBleName() {
  if (cameraProfile.cameraName.length() > 0) {
    return cameraProfile.cameraName;
  }
  return deriveBleNameFromWifiSsid(cameraProfile.wifi.ssid);
}

bool timeReached(uint32_t deadlineMs) {
  return static_cast<int32_t>(millis() - deadlineMs) >= 0;
}

bool isCameraPowerOffDisconnectReason(int reason) {
  return reason == RICOH_BLE_DISCONNECT_REMOTE_USER ||
         reason == RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF;
}

bool cameraSleepGuardActive() {
  return cameraAutoWakeBlocked;
}

bool cameraSleepGuardCooldownActive() {
  return cameraAutoWakeBlocked && cameraAutoWakeCooldownUntil != 0 && !timeReached(cameraAutoWakeCooldownUntil);
}

void showCameraSleepGuardStatus(bool force = false) {
  String line2 = "Auto wake paused";
  if (cameraSleepGuardCooldownActive()) {
    const uint32_t remainingSeconds = (cameraAutoWakeCooldownUntil - millis() + 999) / 1000;
    line2 = String("Cooldown ") + remainingSeconds + "s";
  }
  showStatusIfChanged("Camera standby", line2, "Press G11 or B", preferredBleName(), force);
}

void enterCameraSleepGuard(const char* source, int reason) {
  cameraAutoWakeBlocked = true;
  cameraAutoWakeDisconnectReason = reason;
  cameraAutoWakeCooldownUntil = millis() + CAMERA_POWER_OFF_COOLDOWN_MS;

  closeLiveView(source != nullptr ? source : "camera standby");
  grWifi.disconnect();
  setCameraFlowState(CameraFlowState::CameraSleepGuard, source != nullptr ? source : "camera standby");
  Serial.printf("BLE guard: remote disconnect reason=%d; auto wake paused for %lus, then manual wake required\n",
                reason,
                static_cast<unsigned long>(CAMERA_POWER_OFF_COOLDOWN_MS / 1000));
  showCameraSleepGuardStatus(true);
  lastFrameAt = millis();
  lastCameraRecoveryAt = millis();
}

bool consumeCameraPowerOffDisconnect(const char* source) {
  const int reason = ricohBle.consumeDisconnectReason();
  if (!isCameraPowerOffDisconnectReason(reason)) {
    return false;
  }

  enterCameraSleepGuard(source, reason);
  return true;
}

void clearCameraSleepGuard(const char* source) {
  if (cameraAutoWakeBlocked) {
    Serial.printf("BLE guard: manual wake requested (%s), previous disconnect reason=%d\n",
                  source != nullptr ? source : "manual",
                  cameraAutoWakeDisconnectReason);
  }
  cameraAutoWakeBlocked = false;
  cameraAutoWakeCooldownUntil = 0;
  cameraAutoWakeDisconnectReason = 0;
  ricohBle.clearDisconnectReason();
}

bool cameraSleepGuardBlocksFlow(const char* action) {
  (void)action;
  if (!cameraAutoWakeBlocked) {
    return false;
  }

  showCameraSleepGuardStatus(false);
  return true;
}

void applyDefaultProfile() {
  if (!profileStore.load(cameraProfile)) {
    Serial.println("Profile: NVS open failed; using runtime defaults");
    cameraProfile = CameraProfile{};
  }

  cameraProfile.wifi.ssid = String();
  cameraProfile.wifi.passphrase = String();
  cameraProfile.wifi.bssid = String();
  if (cameraProfile.wifi.cameraIp.isEmpty()) {
    cameraProfile.wifi.cameraIp = GR_HOST;
  }
  grApi.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);

  Serial.printf("Profile: camera='%s' ble='%s' ip='%s'\n",
                cameraProfile.cameraName.c_str(),
                cameraProfile.bleAddress.c_str(),
                cameraProfile.wifi.cameraIp.c_str());
}

bool activateCameraWifiOverBle() {
  if (!RICOH_BLE_AUTO_WLAN_ON_BOOT) {
    return true;
  }
  if (cameraSleepGuardBlocksFlow("WiFi open")) {
    return false;
  }
  if (!ricohBle.isConnected()) {
    showStatusIfChanged("BLE not ready", "Cannot open WiFi", "", "", true);
    return false;
  }

  showStatusIfChanged("BLE_READY", "Opening WiFi", cameraProfile.cameraName, "", true);
  if (!ricohBle.openWifi()) {
    Serial.printf("BLE: Wi-Fi open failed: %s\n", ricohBle.lastError().c_str());
    showStatusIfChanged("BLE WiFi failed", ricohBle.lastError(), "", "", true);
    return false;
  }

  showStatusIfChanged("BLE WiFi sent", "Waiting WiFi params", cameraProfile.cameraName, "", true);
  delay(RICOH_BLE_POST_WLAN_ON_WAIT_MS);
  return true;
}

bool hasStoredBleIdentity() {
  return cameraProfile.bleAddress.length() > 0;
}

bool runBleDiscoveryAtBoot() {
  if (cameraSleepGuardBlocksFlow("BLE discovery")) {
    return false;
  }
  const bool firstBootPairing = !hasStoredBleIdentity();
  const uint8_t configuredAttempts = firstBootPairing ? FIRST_BOOT_BLE_PAIRING_ATTEMPTS : BLE_CONNECT_ATTEMPTS;
  const uint8_t attempts = configuredAttempts == 0 ? 1 : configuredAttempts;
  uint8_t consecutiveConnectFailures = 0;

  if (firstBootPairing) {
    Serial.printf("BLE: no stored identity; pairing scan up to %u rounds\n", static_cast<unsigned>(attempts));
  }

  for (uint8_t attempt = 1; attempt <= attempts; ++attempt) {
    const String bleName = preferredBleName();
    showStatusIfChanged(firstBootPairing ? "Pairing GR BLE" : "Scanning GR BLE",
                        cameraProfile.bleAddress,
                        bleName,
                        String("Attempt ") + attempt + "/" + attempts,
                        true);

    RicohBleDeviceInfo info = ricohBle.scanForCamera(cameraProfile.bleAddress, bleName, BLE_SCAN_SECONDS);
    if (!info.found) {
      showStatusIfChanged("BLE not found", "Retrying...", "", "", true);
    } else {
      String connectedName = info.name.length() > 0 ? info.name : preferredBleName();
      if (connectedName.isEmpty()) {
        connectedName = "RICOH GR";
      }

      showStatusIfChanged("BLE camera found", connectedName, info.address, "Connecting...", true);
      if (ricohBle.connect(info, BLE_CONNECT_TIMEOUT_MS)) {
        cameraProfile.cameraName = connectedName;
        cameraProfile.bleAddress = info.address;
        profileStore.saveBleIdentity(cameraProfile.cameraName, cameraProfile.bleAddress);
        showStatusIfChanged("BLE link ready", cameraProfile.cameraName, info.address, "WiFi via BLE", true);
        setCameraFlowState(CameraFlowState::BleReady, "BLE connected");
        return true;
      }

      Serial.printf("BLE: connect attempt %u/%u failed: %s\n",
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(attempts),
                    ricohBle.lastError().c_str());
      if (consumeCameraPowerOffDisconnect("BLE connect failed")) {
        return false;
      }
      showStatusIfChanged("BLE connect failed", ricohBle.lastError(), "Retrying...", "", true);
      ricohBle.disconnect();
      consecutiveConnectFailures++;
      if (BLE_STACK_RESET_AFTER_FAILURES > 0 && consecutiveConnectFailures >= BLE_STACK_RESET_AFTER_FAILURES) {
        ricohBle.resetStack();
        consecutiveConnectFailures = 0;
      }
    }

    if (attempt < attempts) {
      delay(BLE_CONNECT_RETRY_DELAY_MS);
      yield();
    }
  }

  showStatusIfChanged("BLE unavailable", "Return BLE scan", preferredBleName(), "", true);
  setCameraFlowState(CameraFlowState::BleScan, "BLE attempts exhausted");
  ricohBle.resetStack();
  return false;
}

bool bleStillConnectedForWifi() {
  return ricohBle.isConnected();
}

bool connectWifiFromProfile(bool forceStatus, bool requireBleAnchor = false) {
  grApi.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
  showStatusIfChanged("Connecting WiFi", cameraProfile.wifi.ssid, cameraProfile.wifi.cameraIp, "", forceStatus);
  Serial.printf("WiFi: connecting ssid='%s' bssid='%s'\n",
                cameraProfile.wifi.ssid.c_str(),
                cameraProfile.wifi.bssid.c_str());

  const bool connected = requireBleAnchor
                           ? grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                            cameraProfile.wifi.passphrase.c_str(),
                                            cameraProfile.wifi.bssid.c_str(),
                                            WIFI_CONNECT_TIMEOUT_MS,
                                            bleStillConnectedForWifi)
                           : grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                            cameraProfile.wifi.passphrase.c_str(),
                                            cameraProfile.wifi.bssid.c_str(),
                                            WIFI_CONNECT_TIMEOUT_MS);
  if (connected) {
    showStatusIfChanged("WiFi connected", grWifi.localIPString(), cameraProfile.wifi.cameraIp, "", true);
    Serial.printf("WiFi: connected ip=%s rssi=%ld\n", grWifi.localIPString().c_str(), static_cast<long>(grWifi.rssi()));
    return true;
  }

  showStatusIfChanged("WiFi pending", grWifi.statusText(), cameraProfile.wifi.ssid, "", true);
  Serial.printf("WiFi: connect failed status=%s\n", grWifi.statusText().c_str());
  return false;
}

bool connectWifiAfterBleReady() {
  if (cameraSleepGuardBlocksFlow("connect WiFi")) {
    return false;
  }
  if (!ricohBle.isConnected()) {
    if (consumeCameraPowerOffDisconnect("BLE lost before WiFi open")) {
      return false;
    }
    setCameraFlowState(CameraFlowState::BleScan, "BLE lost before WiFi open");
    return false;
  }

  setCameraFlowState(CameraFlowState::BleReady, "open WiFi via BLE");
  if (!activateCameraWifiOverBle()) {
    return false;
  }

  if (!ricohBle.isConnected()) {
    grWifi.disconnect();
    if (consumeCameraPowerOffDisconnect("BLE lost after WiFi open")) {
      return false;
    }
    ricohBle.clearDisconnectReason();
    setCameraFlowState(CameraFlowState::BleScan, "BLE lost after WiFi open");
    return false;
  }

  RicohBleWifiCredentials wifiCredentials;
  if (!ricohBle.waitForWifiCredentials(wifiCredentials, RICOH_BLE_WIFI_CREDENTIAL_WAIT_MS)) {
    showStatusIfChanged("BLE WiFi params", ricohBle.lastError(), "Back to BLE_READY", "", true);
    grWifi.disconnect();
    if (!ricohBle.isConnected()) {
      if (consumeCameraPowerOffDisconnect("BLE lost waiting WiFi params")) {
        return false;
      }
      ricohBle.clearDisconnectReason();
      setCameraFlowState(CameraFlowState::BleScan, "BLE lost waiting WiFi params");
      return false;
    }
    setCameraFlowState(CameraFlowState::BleReady, "BLE WiFi params unavailable");
    return false;
  }

  cameraProfile.wifi.ssid = wifiCredentials.ssid;
  cameraProfile.wifi.passphrase = wifiCredentials.passphrase;
  cameraProfile.wifi.bssid = wifiCredentials.bssid;

  setCameraFlowState(CameraFlowState::WifiConnecting, "BLE returned WiFi params");
  if (!connectWifiFromProfile(true, true)) {
    grWifi.disconnect();
    if (!ricohBle.isConnected()) {
      if (consumeCameraPowerOffDisconnect("BLE lost during WiFi connect")) {
        return false;
      }
      ricohBle.clearDisconnectReason();
      setCameraFlowState(CameraFlowState::BleScan, "BLE lost during WiFi connect");
      return false;
    }
    setCameraFlowState(CameraFlowState::BleReady, "WiFi connect failed");
    return false;
  }

  return true;
}

bool httpProbeCamera() {
  if (!grWifi.isConnected()) {
    setCameraFlowState(CameraFlowState::BleScan, "HTTP probe without WiFi");
    return false;
  }

  setCameraFlowState(CameraFlowState::HttpProbe, "WiFi connected");
  CameraProps nextProps;
  if (!grApi.fetchProps(nextProps, PROPS_TIMEOUT_MS)) {
    Serial.printf("HTTP: props probe failed: %s\n", grApi.lastError().c_str());
    showStatusIfChanged("HTTP probe failed", grApi.lastError(), "Back to BLE scan", "", true);
    return false;
  }

  cameraProps = nextProps;
  lastPropsAt = millis();
  Serial.printf("HTTP: camera ready model='%s' battery='%s'\n", cameraProps.model.c_str(), cameraProps.battery.c_str());
  showStatusIfChanged("HTTP Probe OK", cameraProps.model, cameraProps.battery, "LiveView next", true);
  return true;
}

bool startLiveViewFromProbe() {
  if (!liveviewEnabled) {
    return true;
  }
  if (!grWifi.isConnected()) {
    return false;
  }

  showStatusIfChanged("Starting LiveView", grWifi.localIPString(), cameraProps.model, cameraProps.battery, true);
  if (!grApi.openLiveView()) {
    Serial.printf("LiveView: open failed: %s\n", grApi.lastError().c_str());
    showStatusIfChanged("LiveView failed", grApi.lastError(), "Back to BLE scan", "", true);
    return false;
  }

  lastFrameAt = millis();
  mjpeg.reset();
  setCameraFlowState(CameraFlowState::LiveViewRunning, "LiveView opened");
  Serial.println("LiveView: connected");
  return true;
}

bool reasonRequiresBleRescan(const char* reason) {
  if (reason == nullptr) {
    return !ricohBle.isConnected();
  }
  const String text(reason);
  return !ricohBle.isConnected() || text.indexOf("BLE disconnected") >= 0 || text.indexOf("BLE not ready") >= 0;
}

void disconnectWifiLiveViewToBleReady(const char* reason) {
  closeLiveView(reason != nullptr ? reason : "BLE_READY reset");
  grWifi.disconnect();
  if (ricohBle.isConnected()) {
    setCameraFlowState(CameraFlowState::BleReady, reason != nullptr ? reason : "BLE still connected");
  } else {
    setCameraFlowState(CameraFlowState::BleScan, reason != nullptr ? reason : "BLE lost");
  }
}

bool resumeFromBleReady(const char* reason) {
  if (cameraSleepGuardBlocksFlow("BLE_READY resume")) {
    return false;
  }
  if (!ricohBle.isConnected()) {
    if (consumeCameraPowerOffDisconnect("BLE lost before BLE_READY resume")) {
      return false;
    }
    setCameraFlowState(CameraFlowState::BleScan, "BLE lost before BLE_READY resume");
    return false;
  }

  disconnectWifiLiveViewToBleReady(reason);
  for (uint8_t attempt = 0; attempt < WIFI_OPEN_ATTEMPTS; ++attempt) {
    if (cameraSleepGuardActive()) {
      return false;
    }
    if (!ricohBle.isConnected()) {
      if (consumeCameraPowerOffDisconnect("BLE lost during WiFi retry")) {
        return false;
      }
      setCameraFlowState(CameraFlowState::BleScan, "BLE lost during WiFi retry");
      return false;
    }
    if (connectWifiAfterBleReady()) {
      if (httpProbeCamera() && startLiveViewFromProbe()) {
        return true;
      }
      disconnectWifiLiveViewToBleReady("HTTP/LiveView retry from BLE_READY");
    } else if (cameraSleepGuardActive()) {
      return false;
    }
    delay(BLE_CONNECT_RETRY_DELAY_MS);
    yield();
  }
  return false;
}

void disconnectAllTransportsToBleScan(const char* reason) {
  closeLiveView(reason != nullptr ? reason : "flow reset");
  grWifi.disconnect();
  ricohBle.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, reason);
}

void resetBleStackBeforeScanAfterLinkLoss(const char* reason) {
  Serial.printf("BLE recovery: reset stack (%s)\n", reason != nullptr ? reason : "link lost");
  showStatusIfChanged("BLE stack reset", "Camera link lost", preferredBleName(), "Scanning soon", true);
  ricohBle.resetStack();
  delay(BLE_RECOVERY_STACK_RESET_GRACE_MS);
  yield();
}

bool runCameraFlowOnce() {
  if (cameraSleepGuardBlocksFlow("camera flow")) {
    return false;
  }
  setCameraFlowState(CameraFlowState::BleScan, "enter BLE scan mode");
  if (!runBleDiscoveryAtBoot()) {
    return false;
  }

  for (uint8_t attempt = 0; attempt < WIFI_OPEN_ATTEMPTS; ++attempt) {
    if (cameraSleepGuardActive()) {
      return false;
    }
    if (connectWifiAfterBleReady()) {
      if (httpProbeCamera() && startLiveViewFromProbe()) {
        return true;
      }

      if (ricohBle.isConnected()) {
        return resumeFromBleReady("HTTP/LiveView unavailable");
      }
      disconnectAllTransportsToBleScan("HTTP/LiveView unavailable");
      return false;
    }

    if (cameraSleepGuardActive()) {
      return false;
    }
    delay(BLE_CONNECT_RETRY_DELAY_MS);
    yield();
  }

  return false;
}

void refreshPropsIfDue(bool force = false) {
  const uint32_t now = millis();
  if (!force && (now - lastPropsAt) < PROPS_REFRESH_INTERVAL_MS) {
    return;
  }
  if (!grWifi.isConnected()) {
    return;
  }

  CameraProps nextProps;
  if (grApi.fetchProps(nextProps, PROPS_TIMEOUT_MS)) {
    cameraProps = nextProps;
    lastPropsAt = now;
  } else if (force) {
    Serial.printf("HTTP: props refresh failed: %s\n", grApi.lastError().c_str());
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

  if (!decoder.drawFrame(ui.getCanvas(), data, len)) {
    Serial.printf("JPEG decode failed len=%u err=%s\n", static_cast<unsigned>(len), decoder.lastError().c_str());
  } else {
    if (DRAW_LIVE_OVERLAY) {
      ui.drawOverlay(grWifi.statusText(),
                     grApi.isLiveViewOpen() ? "LIVE" : "IDLE",
                     cameraProps.model,
                     cameraProps.battery,
                     currentFps,
                     grWifi.rssi(),
                     decodedFrames,
                     mjpeg.droppedFrames());
    }
    ui.pushCanvas();
  }
  lastFrameAt = millis();
}

void recoverCameraConnection(const char* reason) {
  if (cameraRecoveryInProgress) {
    return;
  }

  cameraRecoveryInProgress = true;
  Serial.printf("Camera recovery: %s\n", reason != nullptr ? reason : "manual");

  if (!ricohBle.isConnected() && consumeCameraPowerOffDisconnect(reason != nullptr ? reason : "camera recovery")) {
    cameraRecoveryInProgress = false;
    return;
  }
  if (cameraSleepGuardBlocksFlow(reason != nullptr ? reason : "camera recovery")) {
    cameraRecoveryInProgress = false;
    return;
  }

  bool recovered = false;
  if (!reasonRequiresBleRescan(reason)) {
    showStatusIfChanged("Camera recovery", reason != nullptr ? reason : "reconnect", "BLE_READY retry", "", true);
    recovered = resumeFromBleReady(reason != nullptr ? reason : "camera recovery");
  }

  if (!recovered && cameraSleepGuardActive()) {
    lastFrameAt = millis();
    lastCameraRecoveryAt = millis();
    cameraRecoveryInProgress = false;
    return;
  }

  if (!recovered) {
    const bool bleLinkAlreadyLost = !ricohBle.isConnected();
    showStatusIfChanged("Camera recovery", reason != nullptr ? reason : "reconnect", "Back to BLE scan", "", true);
    disconnectAllTransportsToBleScan(reason != nullptr ? reason : "camera recovery");
    if (bleLinkAlreadyLost) {
      resetBleStackBeforeScanAfterLinkLoss(reason != nullptr ? reason : "camera recovery");
    } else {
      delay(100);
    }
    recovered = runCameraFlowOnce();
  }

  lastFrameAt = millis();
  lastCameraRecoveryAt = recovered ? millis() : 0;
  cameraRecoveryInProgress = false;
}

void ensureWiFi() {
  if (cameraFlowState == CameraFlowState::LiveViewRunning && !grWifi.isConnected()) {
    recoverCameraConnection("WiFi disconnected");
  }
}

void ensureLiveView() {
  if (cameraFlowState != CameraFlowState::LiveViewRunning || !liveviewEnabled) {
    return;
  }
  if (!ricohBle.isConnected()) {
    if (consumeCameraPowerOffDisconnect("BLE disconnected")) {
      return;
    }
    recoverCameraConnection("BLE disconnected");
    return;
  }
  if (!grWifi.isConnected()) {
    recoverCameraConnection("WiFi disconnected");
    return;
  }
  if (cameraSleepGuardActive()) {
    showCameraSleepGuardStatus(false);
    return;
  }

  if (!grApi.isLiveViewOpen()) {
    recoverCameraConnection("LiveView closed");
    return;
  }

  const int readLen = grApi.readLiveView(streamReadBuffer, sizeof(streamReadBuffer));
  if (readLen > 0) {
    mjpeg.process(streamReadBuffer, static_cast<size_t>(readLen));
  } else if (readLen < 0) {
    Serial.printf("LiveView: read failed: %s\n", grApi.lastError().c_str());
    recoverCameraConnection("LiveView read failed");
    return;
  }

  const uint32_t now = millis();
  if ((now - lastFrameAt) > LIVEVIEW_STALL_TIMEOUT_MS) {
    recoverCameraConnection("LiveView stall watchdog");
  }
}

void requestManualCameraWake(const char* source) {
  if (cameraSleepGuardCooldownActive()) {
    Serial.printf("BLE guard: manual wake deferred until cooldown completes (%s)\n",
                  source != nullptr ? source : "manual");
    showCameraSleepGuardStatus(true);
    return;
  }

  const char* wakeSource = source != nullptr ? source : "manual wake";
  clearCameraSleepGuard(wakeSource);
  liveviewEnabled = true;
  closeLiveView(wakeSource);
  grWifi.disconnect();
  ricohBle.disconnect();
  setCameraFlowState(CameraFlowState::BleScan, wakeSource);
  showStatusIfChanged("Manual wake", "Resetting BLE...", preferredBleName(), "", true);

  Serial.printf("BLE guard: manual wake BLE stack rebuild (%s)\n", wakeSource);
  ricohBle.resetStack(true);
  delay(BLE_MANUAL_WAKE_REINIT_SETTLE_MS);
  yield();

  showStatusIfChanged("Manual wake", "Reconnecting...", preferredBleName(), "", true);
  const bool online = runCameraFlowOnce();
  lastFrameAt = millis();
  lastCameraRecoveryAt = online ? millis() : 0;
}

void triggerShutterFromG11() {
  if (cameraSleepGuardActive()) {
    requestManualCameraWake("G11 manual wake");
    return;
  }

  if (!ricohBle.shutterReady()) {
    showStatusIfChanged("G11 shutter", "BLE not ready", "Back to BLE scan", "", true);
    recoverCameraConnection("G11 shutter BLE not ready");
    return;
  }

  showStatusIfChanged("G11 shutter", "BLE shooting...", cameraProps.model, cameraProps.battery, true);
  if (ricohBle.shoot(true)) {
    showStatusIfChanged("G11 shutter", "BLE SHOT OK", cameraProps.model, cameraProps.battery, true);
    return;
  }

  Serial.printf("G11: BLE shutter failed: %s\n", ricohBle.lastError().c_str());
  showStatusIfChanged("G11 BLE failed", ricohBle.lastError(), "Preview kept", "", true);

  if (cameraFlowState == CameraFlowState::LiveViewRunning && grWifi.isConnected() && grApi.isLiveViewOpen()) {
    return;
  }

  recoverCameraConnection("G11 BLE shutter failed");
}

void handleButtons() {
  const ButtonEvents events = buttons.poll();
  if (events.buttonA) {
    showStatusIfChanged("Button A", "Reserved", cameraProps.model, cameraProps.battery, true);
  }

  if (events.buttonB) {
    if (cameraSleepGuardActive()) {
      requestManualCameraWake("Button B manual wake");
      return;
    }
    liveviewEnabled = !liveviewEnabled;
    if (!liveviewEnabled) {
      closeLiveView("button B pause");
      showStatusIfChanged("LiveView paused", "Press B to resume", cameraProps.model, cameraProps.battery, true);
    } else {
      closeLiveView("button B reconnect");
      showStatusIfChanged("LiveView enabled", "Reconnecting...", cameraProps.model, cameraProps.battery, true);
    }
  }

  const G11ButtonEvents g11 = g11Button.poll();
  if (g11.pressed) {
    triggerShutterFromG11();
  }
}

void serviceCameraFlowIfNeeded() {
  if (cameraRecoveryInProgress || cameraFlowState == CameraFlowState::LiveViewRunning) {
    return;
  }

  if (!ricohBle.isConnected() && consumeCameraPowerOffDisconnect("scheduled service")) {
    return;
  }
  if (cameraSleepGuardBlocksFlow("scheduled service")) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastCameraRecoveryAt) < BLE_SCAN_RETRY_INTERVAL_MS) {
    return;
  }
  lastCameraRecoveryAt = now;
  const bool online = (cameraFlowState == CameraFlowState::BleReady && ricohBle.isConnected())
                        ? resumeFromBleReady("BLE_READY scheduled retry")
                        : runCameraFlowOnce();
  if (!online) {
    lastCameraRecoveryAt = 0;
  }
}

void updateStatusUiIfDue() {
  const uint32_t now = millis();
  if ((now - lastStatusAt) < UI_STATUS_INTERVAL_MS) {
    return;
  }
  lastStatusAt = now;

  if (cameraSleepGuardActive()) {
    showCameraSleepGuardStatus(false);
    return;
  }

  if (!grApi.isLiveViewOpen()) {
    showStatusIfChanged(grWifi.statusText(),
                        liveviewEnabled ? grWifi.localIPString() : "Preview paused",
                        cameraProps.model,
                        cameraProps.battery);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  ui.begin();
  ui.showBoot("V2 booting...");
  waitForSerialConsole();

  buttons.begin();
  g11Button.begin();
  decoder.begin();
  grWifi.begin();

  applyDefaultProfile();

  if (!psramFound()) {
    Serial.println("PSRAM not found; JPEG buffer allocation may fail");
    ui.showError("PSRAM not found");
  }

  frameBuffer = static_cast<uint8_t*>(heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frameBuffer == nullptr) {
    frameBuffer = static_cast<uint8_t*>(heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_8BIT));
  }
  if (frameBuffer == nullptr) {
    Serial.println("Failed to allocate JPEG frame buffer");
    ui.showError("Frame buffer alloc failed");
    while (true) {
      delay(1000);
    }
  }

  mjpeg.begin(frameBuffer, FRAME_BUFFER_SIZE, onJpegFrame, nullptr);

  const bool startupOnline = runCameraFlowOnce();
  lastCameraRecoveryAt = startupOnline ? millis() : 0;
  lastFrameAt = millis();
  lastStatusAt = 0;
}

void loop() {
  handleButtons();
  serviceCameraFlowIfNeeded();
  ensureWiFi();
  refreshPropsIfDue();
  ensureLiveView();
  updateStatusUiIfDue();
  delay(1);
}
