#include <Arduino.h>
#include <M5PM1.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include "buttons.h"
#include "ble_reconnect_policy.h"
#include "camera_identity.h"
#include "camera_profile_store.h"
#include "config.h"
#include "display.h"
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
JpegDecoder decoder;
CameraProfileStore profileStore;
CameraProfile cameraProfile;
RicohBleClient ricohBle;
M5PM1 stickPower;

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
uint32_t powerButtonLastPollAt = 0;
uint32_t powerButtonPressedSince = 0;
bool powerButtonHoldReported = false;
bool stickPowerReady = false;
uint32_t lastCameraRecoveryAt = 0;
bool cameraRecoveryInProgress = false;
bool cameraAutoWakeBlocked = false;
uint32_t cameraAutoWakeCooldownUntil = 0;
int cameraAutoWakeDisconnectReason = 0;
RicohCameraPowerState cameraPowerState = RicohCameraPowerState::Unknown;
bool cameraManualWakeOverride = false;
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
bool wifiCacheRefreshPending = false;
uint32_t wifiCacheRefreshAfter = 0;

constexpr uint32_t STATUS_MIN_REDRAW_MS = 1500;
constexpr bool DRAW_LIVE_OVERLAY = true;

bool beginStickPower() {
  const int8_t sda = M5.getPin(m5::pin_name_t::in_i2c_sda);
  const int8_t scl = M5.getPin(m5::pin_name_t::in_i2c_scl);
  const m5pm1_err_t err = stickPower.begin(&Wire, M5PM1_DEFAULT_ADDR, sda, scl, M5PM1_I2C_FREQ_100K);
  stickPowerReady = (err == M5PM1_OK);
  if (!stickPowerReady) {
    Serial.printf("Power: M5PM1 begin failed err=%d; fallback to M5Unified power API\n", static_cast<int>(err));
    return false;
  }

  stickPower.setDoubleOffDisable(false);
  stickPower.btnSetConfig(M5PM1_BTN_TYPE_DOUBLE, M5PM1_BTN_DOUBLE_CLICK_DELAY_500MS);

  bool pressed = false;
  stickPower.btnGetState(&pressed);
  Serial.println("Power: M5PM1 ready");
  return true;
}

bool isStickPowerButtonPressed() {
  if (stickPowerReady) {
    bool pressed = false;
    if (stickPower.btnGetState(&pressed) == M5PM1_OK) {
      return pressed;
    }
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
    Serial.printf("Flow: %s -> %s (%s) uptime=%lums\n",
                  cameraFlowStateName(cameraFlowState),
                  cameraFlowStateName(state),
                  reason != nullptr ? reason : "",
                  static_cast<unsigned long>(millis()));
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
  showStatusIfChanged("Camera standby", line2, "Press Button A", preferredBleName(), force);
}

void enterCameraSleepGuard(const char* source, int reason) {
  cameraPowerState = RicohCameraPowerState::OffOrShuttingDown;
  cameraAutoWakeBlocked = true;
  cameraAutoWakeDisconnectReason = reason;
  cameraAutoWakeCooldownUntil = millis() + CAMERA_POWER_OFF_COOLDOWN_MS;

  closeLiveView(source != nullptr ? source : "camera standby");
  grWifi.disconnect();
  ricohBle.disconnect();
  setCameraFlowState(CameraFlowState::CameraSleepGuard, source != nullptr ? source : "camera standby");
  Serial.printf("BLE guard: remote disconnect reason=%d; auto wake paused for %lus, then manual wake required\n",
                reason,
                static_cast<unsigned long>(CAMERA_POWER_OFF_COOLDOWN_MS / 1000));
  showCameraSleepGuardStatus(true);
  lastFrameAt = millis();
  lastCameraRecoveryAt = millis();
}

bool consumeCameraPowerOffNotification(const char* source) {
  if (!ricohBle.consumePowerOffNotification()) {
    return false;
  }

  enterCameraSleepGuard(source != nullptr ? source : "BLE power notify 0x00", RICOH_BLE_DISCONNECT_REMOTE_POWER_OFF);
  return true;
}

bool consumeCameraPowerOffDisconnect(const char* source) {
  const int reason = ricohBle.consumeDisconnectReason();
  if (!isCameraPowerOffDisconnectReason(reason)) {
    return false;
  }

  enterCameraSleepGuard(source, reason);
  return true;
}

bool consumeCameraPowerOffDisconnectAfterReady(const char* source) {
  const int reason = ricohBle.consumeDisconnectReason();
  if (!isCameraPowerOffDisconnectReason(reason)) {
    return false;
  }

  if (cameraFlowState == CameraFlowState::BleReady ||
      cameraFlowState == CameraFlowState::WifiConnecting ||
      cameraFlowState == CameraFlowState::HttpProbe ||
      cameraFlowState == CameraFlowState::LiveViewRunning) {
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
    Serial.printf("BLE guard: manual wake requested (%s), previous disconnect reason=%d\n",
                  source != nullptr ? source : "manual",
                  cameraAutoWakeDisconnectReason);
  }
  cameraPowerState = RicohCameraPowerState::Unknown;
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
  if (!ricohBle.isConnected() || cameraProfile.bleAddress.length() == 0) {
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
  if (!ricohBle.isConnected() || cameraSleepGuardActive()) {
    return;
  }

  RicohBleWifiCredentials fresh;
  if (!ricohBle.waitForWifiCredentials(fresh, RICOH_BLE_WIFI_CREDENTIAL_WAIT_MS)) {
    Serial.printf("WiFi cache: deferred refresh failed: %s\n", ricohBle.lastError().c_str());
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
  grApi.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);

  Serial.printf("Profile: camera='%s' ble='%s' ip='%s'\n",
                cameraProfile.cameraName.c_str(),
                cameraProfile.bleAddress.c_str(),
                cameraProfile.wifi.cameraIp.c_str());
}

bool ensureCameraPowerReadyForWifi(const char* source) {
  if (!RICOH_BLE_REQUIRE_POWER_ON_BEFORE_WIFI) {
    return true;
  }
  if (cameraSleepGuardBlocksFlow(source != nullptr ? source : "power guard")) {
    return false;
  }
  if (!ricohBle.isConnected()) {
    cameraPowerState = RicohCameraPowerState::Unknown;
    return false;
  }

  showStatusIfChanged("BLE_READY", "Checking power", cameraProfile.cameraName, "", true);
  RicohCameraPowerState nextState = RicohCameraPowerState::Unknown;
  bool readOk = false;
  const uint8_t retries = RICOH_BLE_POWER_READ_RETRIES == 0 ? 1 : RICOH_BLE_POWER_READ_RETRIES;
  for (uint8_t attempt = 0; attempt < retries; ++attempt) {
    if (ricohBle.readPowerState(nextState)) {
      readOk = true;
      break;
    }
    Serial.printf("BLE: power read attempt %u/%u failed: %s\n",
                  static_cast<unsigned>(attempt + 1),
                  static_cast<unsigned>(retries),
                  ricohBle.lastError().c_str());
    delay(120);
    yield();
  }

  cameraPowerState = readOk ? nextState : RicohCameraPowerState::Unknown;
  if (readOk && cameraPowerState == RicohCameraPowerState::On) {
    if (!ricohBle.enablePowerStateNotify()) {
      Serial.printf("BLE: power notify subscribe failed: %s\n", ricohBle.lastError().c_str());
    }
    return true;
  }

  if (cameraManualWakeOverride) {
    Serial.printf("BLE: power state %s; manual wake override allows WiFi\n", cameraPowerStateName(cameraPowerState));
    if (ricohBle.isConnected() && !ricohBle.enablePowerStateNotify()) {
      Serial.printf("BLE: power notify subscribe failed: %s\n", ricohBle.lastError().c_str());
    }
    return true;
  }

  if (!readOk && RICOH_BLE_ALLOW_WIFI_WHEN_POWER_UNKNOWN) {
    Serial.println("BLE: power state unknown; config allows WiFi open");
    return true;
  }

  const char* reason = (readOk && cameraPowerState == RicohCameraPowerState::OffOrShuttingDown)
                         ? "BLE power state off"
                         : "BLE power state unknown";
  Serial.printf("WiFi blocked: camera power state=%s readOk=%d source=%s\n",
                cameraPowerStateName(cameraPowerState),
                readOk ? 1 : 0,
                source != nullptr ? source : "");
  showStatusIfChanged("Camera standby",
                      readOk ? "Power OFF" : "Power unknown",
                      "Auto WiFi blocked",
                      "Press Button A",
                      true);
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
  if (!ricohBle.isConnected()) {
    showStatusIfChanged("BLE not ready", "Cannot open WiFi", "", "", true);
    return false;
  }
  if (!ensureCameraPowerReadyForWifi("WiFi open")) {
    return false;
  }

  showStatusIfChanged("BLE_READY", "Opening WiFi", cameraProfile.cameraName, "", true);
  if (!ricohBle.openWifi()) {
    Serial.printf("BLE: Wi-Fi open failed: %s\n", ricohBle.lastError().c_str());
    showStatusIfChanged("BLE WiFi failed", ricohBle.lastError(), "", "", true);
    return false;
  }

  showStatusIfChanged("BLE WiFi sent", "Waiting WiFi params", cameraProfile.cameraName, "", true);
  if (RICOH_BLE_POST_WLAN_ON_WAIT_MS > 0) {
    delay(RICOH_BLE_POST_WLAN_ON_WAIT_MS);
  }
  return true;
}

bool hasStoredBleIdentity() {
  return cameraProfile.bleAddress.length() > 0;
}

bool hasDirectBleIdentity() {
  return hasDirectBleReconnectIdentity(cameraProfile.bleAddress.c_str(), cameraProfile.bleAddressTypeKnown);
}

String displayBleName(const RicohBleDeviceInfo& info) {
  String connectedName = info.name.length() > 0 ? info.name : preferredBleName();
  if (connectedName.isEmpty()) {
    connectedName = "RICOH GR";
  }
  return connectedName;
}

void saveConnectedBleIdentity(const String& connectedName, const RicohBleDeviceInfo& info) {
  cameraProfile.cameraName = connectedName;
  cameraProfile.bleAddress = info.address;
  cameraProfile.bleAddressType = info.addressType;
  cameraProfile.bleAddressTypeKnown = true;
  cameraProfile.bleBonded = ricohBle.isBonded(info);
  profileStore.saveBleIdentity(cameraProfile.cameraName,
                               cameraProfile.bleAddress,
                               cameraProfile.bleAddressType,
                               cameraProfile.bleBonded);
}

bool connectStoredBleIdentityFast() {
  if (!hasDirectBleIdentity()) {
    return false;
  }

  RicohBleDeviceInfo info;
  info.found = true;
  info.name = cameraProfile.cameraName;
  info.address = cameraProfile.bleAddress;
  info.addressType = cameraProfile.bleAddressType;
  info.connectable = true;

  RicohBleConnectOptions options;
  options.timeoutMs = BLE_FAST_CONNECT_TIMEOUT_MS;
  options.securityWaitMs = cameraProfile.bleBonded ? RICOH_BLE_BONDED_SECURITY_WAIT_MS : RICOH_BLE_SECURITY_WAIT_MS;
  options.preConnectDelayMs = 0;
  options.exchangeMtu = false;

  showStatusIfChanged("BLE fast connect", cameraProfile.cameraName, cameraProfile.bleAddress, "Direct reconnect", true);
  if (!ricohBle.connect(info, options)) {
    Serial.printf("BLE: fast direct connect failed: %s\n", ricohBle.lastError().c_str());
    return false;
  }

  const String connectedName = displayBleName(info);
  saveConnectedBleIdentity(connectedName, info);
  showStatusIfChanged("BLE link ready", cameraProfile.cameraName, info.address, "WiFi via BLE", true);
  setCameraFlowState(CameraFlowState::BleReady, "BLE direct connected");
  return true;
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
  } else if (connectStoredBleIdentityFast()) {
    return true;
  } else if (consumeCameraPowerOffDisconnectAfterReady("BLE fast connect failed")) {
    return false;
  } else if (ricohBle.lastFailureWasResourceExhausted()) {
    Serial.println("BLE: fast connect exhausted host resources; reset stack before scan retry");
    ricohBle.resetStack();
  } else {
    ricohBle.disconnect();
  }

  for (uint8_t attempt = 1; attempt <= attempts; ++attempt) {
    bool skipRetryDelay = false;
    const String bleName = preferredBleName();
    showStatusIfChanged(firstBootPairing ? "Pairing GR BLE" : "Scanning GR BLE",
                        cameraProfile.bleAddress,
                        bleName,
                        String("Attempt ") + attempt + "/" + attempts,
                        true);

    RicohBleDeviceInfo info = ricohBle.scanForCamera(cameraProfile.bleAddress, bleName, BLE_SCAN_SECONDS);
    if (!info.found) {
      showStatusIfChanged("BLE not found", "Retrying...", "", "", true);
    } else if (!info.connectable) {
      Serial.printf("BLE: scan selected non-connectable candidate name='%s' addr=%s; retrying\n",
                    info.name.c_str(),
                    info.address.c_str());
      showStatusIfChanged("BLE not connectable", info.address, "Retrying...", "", true);
    } else {
      const String connectedName = displayBleName(info);

      showStatusIfChanged("BLE camera found", connectedName, info.address, "Connecting...", true);
      RicohBleConnectOptions options;
      options.timeoutMs = BLE_CONNECT_TIMEOUT_MS;
      options.securityWaitMs = RICOH_BLE_SECURITY_WAIT_MS;
      options.preConnectDelayMs = BLE_SCAN_TO_CONNECT_DELAY_MS;
      options.exchangeMtu = false;

      if (ricohBle.connect(info, options)) {
        saveConnectedBleIdentity(connectedName, info);
        showStatusIfChanged("BLE link ready", cameraProfile.cameraName, info.address, "WiFi via BLE", true);
        setCameraFlowState(CameraFlowState::BleReady, "BLE connected");
        return true;
      }

      Serial.printf("BLE: connect attempt %u/%u failed: %s\n",
                    static_cast<unsigned>(attempt),
                    static_cast<unsigned>(attempts),
                    ricohBle.lastError().c_str());
      consumeCameraPowerOffDisconnectAfterReady("BLE connect failed");
      showStatusIfChanged("BLE connect failed", ricohBle.lastError(), "Retrying...", "", true);
      ricohBle.disconnect();
      if (ricohBle.lastFailureWasResourceExhausted()) {
        Serial.println("BLE: host resources exhausted during connect; reset stack before retry");
        ricohBle.resetStack();
        consecutiveConnectFailures = 0;
        skipRetryDelay = true;
      } else {
        consecutiveConnectFailures++;
        if (BLE_STACK_RESET_AFTER_FAILURES > 0 && consecutiveConnectFailures >= BLE_STACK_RESET_AFTER_FAILURES) {
          ricohBle.resetStack();
          consecutiveConnectFailures = 0;
          skipRetryDelay = true;
        }
      }
    }

    if (attempt < attempts && !skipRetryDelay) {
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

bool connectWifiFromProfile(bool forceStatus, bool requireBleAnchor = false, uint32_t totalTimeoutMs = WIFI_CONNECT_TIMEOUT_MS, bool allowFullScanFallback = true) {
  grApi.setEndpoint(cameraProfile.wifi.cameraIp.c_str(), GR_PORT);
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
    connected = requireBleAnchor
                  ? grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                   cameraProfile.wifi.passphrase.c_str(),
                                   cameraProfile.wifi.bssid.c_str(),
                                   channel,
                                   hintTimeout,
                                   bleStillConnectedForWifi)
                  : grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                   cameraProfile.wifi.passphrase.c_str(),
                                   cameraProfile.wifi.bssid.c_str(),
                                   channel,
                                   hintTimeout);

    if (allowFullScanFallback && !connected && (!requireBleAnchor || bleStillConnectedForWifi())) {
      uint32_t fallbackTimeout = totalTimeoutMs > hintTimeout ? totalTimeoutMs - hintTimeout : totalTimeoutMs;
      if (fallbackTimeout < 1000) {
        fallbackTimeout = totalTimeoutMs;
      }
      Serial.printf("WiFi: channel hint %u failed; fallback to full scan timeout=%lums\n",
                    static_cast<unsigned>(channel),
                    static_cast<unsigned long>(fallbackTimeout));
      connected = requireBleAnchor
                    ? grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                     cameraProfile.wifi.passphrase.c_str(),
                                     cameraProfile.wifi.bssid.c_str(),
                                     fallbackTimeout,
                                     bleStillConnectedForWifi)
                    : grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                     cameraProfile.wifi.passphrase.c_str(),
                                     cameraProfile.wifi.bssid.c_str(),
                                     fallbackTimeout);
    }
  } else {
    connected = requireBleAnchor
                  ? grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                   cameraProfile.wifi.passphrase.c_str(),
                                   cameraProfile.wifi.bssid.c_str(),
                                   totalTimeoutMs,
                                   bleStillConnectedForWifi)
                  : grWifi.connect(cameraProfile.wifi.ssid.c_str(),
                                   cameraProfile.wifi.passphrase.c_str(),
                                   cameraProfile.wifi.bssid.c_str(),
                                   totalTimeoutMs);
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

  if (hasUsableCachedWifiCredentials()) {
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
    setCameraFlowState(CameraFlowState::WifiConnecting, "cached WiFi params");
    if (connectWifiFromProfile(true, true, WIFI_CACHED_CONNECT_TIMEOUT_MS, false)) {
      saveConnectedWifiBssidToCache("cached connect");
      scheduleWifiCacheRefresh();
      return true;
    }
    Serial.println("WiFi cache: cached connect failed; reading fresh BLE params");
    grWifi.disconnect();
    if (!ricohBle.isConnected()) {
      if (consumeCameraPowerOffDisconnect("BLE lost during cached WiFi connect")) {
        return false;
      }
      ricohBle.clearDisconnectReason();
      setCameraFlowState(CameraFlowState::BleScan, "BLE lost during cached WiFi connect");
      return false;
    }
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

  applyBleWifiCredentials(wifiCredentials, "fresh BLE", false);

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

  saveConnectedWifiBssidToCache("fresh BLE connect");
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
      if (liveviewEnabled && startLiveViewFromProbe()) {
        return true;
      }
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
  const uint32_t flowStartMs = millis();
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
      if (liveviewEnabled && startLiveViewFromProbe()) {
        Serial.printf("Flow: camera online total_ms=%lu\n", static_cast<unsigned long>(millis() - flowStartMs));
        return true;
      }
      if (httpProbeCamera() && startLiveViewFromProbe()) {
        Serial.printf("Flow: camera online total_ms=%lu\n", static_cast<unsigned long>(millis() - flowStartMs));
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
  if (!force && cameraProps.ok && (now - lastPropsAt) < PROPS_REFRESH_INTERVAL_MS) {
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
  if (consumeCameraPowerOffNotification("BLE power notify 0x00")) {
    return;
  }
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
  cameraManualWakeOverride = true;
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
  cameraManualWakeOverride = false;
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
  grWifi.disconnect();
  ricohBle.disconnect();

  ui.showBoot("Release PWR...");
  const uint32_t startMs = millis();
  while (isStickPowerButtonPressed() && (millis() - startMs) < POWER_BUTTON_RELEASE_WAIT_MS) {
    M5.update();
    delay(20);
    yield();
  }

  ui.showBoot("Power off...");
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

void triggerShutterFromButtonA() {
  if (cameraSleepGuardActive()) {
    requestManualCameraWake("Button A manual wake");
    return;
  }

  if (!ricohBle.shutterReady()) {
    showStatusIfChanged("Button A shutter", "BLE not ready", "Back to BLE scan", "", true);
    recoverCameraConnection("Button A shutter BLE not ready");
    return;
  }

  showStatusIfChanged("Button A shutter", "BLE shooting...", cameraProps.model, cameraProps.battery, true);
  if (ricohBle.shoot(true)) {
    showStatusIfChanged("Button A shutter", "BLE SHOT OK", cameraProps.model, cameraProps.battery, true);
    return;
  }

  Serial.printf("Button A: BLE shutter failed: %s\n", ricohBle.lastError().c_str());
  showStatusIfChanged("Button A BLE failed", ricohBle.lastError(), "Preview kept", "", true);

  if (cameraFlowState == CameraFlowState::LiveViewRunning && grWifi.isConnected() && grApi.isLiveViewOpen()) {
    return;
  }

  recoverCameraConnection("Button A BLE shutter failed");
}

void handleButtons() {
  const ButtonEvents events = buttons.poll();
  if (events.powerOff || pollStickPowerHold()) {
    shutdownStickS3();
    return;
  }
  if (events.buttonA) {
    triggerShutterFromButtonA();
  }
}

void serviceCameraFlowIfNeeded() {
  if (consumeCameraPowerOffNotification("BLE power notify 0x00")) {
    return;
  }
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

  beginStickPower();
  buttons.begin();
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
  refreshWifiCacheIfDue();
  updateStatusUiIfDue();
  delay(1);
}
