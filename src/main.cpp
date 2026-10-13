#include <Arduino.h>
#include <M5PM1.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include "ble_pairing_policy.h"
#include "buttons.h"
#include "camera_profile_store.h"
#include "charging_indicator_policy.h"
#include "config.h"
#include "core/Logger.h"
#include "display.h"
#include "jpeg_decoder.h"
#include "app/AppRuntime.h"
#include "brands/ricoh/RicohDriver.h"
#include "camera/ICameraDriver.h"
#include "services/PreviewFrameBuffer.h"
#include "supervisor/SystemSupervisor.h"
#include "ui/ButtonInput.h"
#include "ui/OrientationTracker.h"
#include "ui/PairingGuide.h"
#include "ui/UiCoordinator.h"
#include "ui/UiSoundPlayer.h"
#include "ui/UiTheme.h"

namespace {

DisplayUi ui;
Buttons buttons;
JpegDecoder decoder;
CameraProfileStore profileStore;
M5PM1 stickPower;
rvf::RicohDriver ricohDriver(profileStore);
rvf::ICameraDriver& camera = ricohDriver;

// StickS3 side of the app: buttons, screen, power and serial log.
class AppPlatform : public rvf::IAppPlatform {
public:
  uint32_t nowMs() override;
  ButtonEvents pollButtons() override;
  bool pollPowerButtonHold() override;
  void shutdown() override;
  void serviceDevicePower() override;
  void refreshUi() override;
  void refreshUi(const ButtonEvents& input) override;
  void superviseSystem() override;
  void toggleDisplayMirror() override;
  void idle() override;
  void log(const char* text) override;
  void logLine(const char* text) override;
};

AppPlatform appPlatform;
rvf::AppRuntime runtime(camera, appPlatform);
rvf::SystemSupervisor systemSupervisor;
rvf::UiCoordinator uiCoordinator;
rvf::OrientationTracker orientationTracker(rvf::UiOrientation::Portrait);
rvf::UiSoundPlayer uiSound;
uint8_t* frameBuffer = nullptr;

uint32_t lastOrientationSampleAt = 0;
bool imuAvailable = false;
ButtonEvents currentUiInput;
int16_t jpegViewportWidth = 0;
int16_t jpegViewportHeight = 0;

// M5PM1 power-management chip and its PWR button long-press tracking.
struct StickPowerState {
  uint32_t buttonLastPollAt = 0;
  uint32_t buttonPressedSince = 0;
  bool buttonHoldReported = false;
  bool ready = false;
};
StickPowerState pmic;

// Decoded-frame counters for the preview FPS overlay.
struct FpsState {
  uint32_t decodedFrames = 0;
  uint32_t windowStart = 0;
  uint32_t windowFrames = 0;
  float current = 0.0f;
};
FpsState fpsStats;

// Throttles redraws of the text status screen.
struct StatusUiState {
  uint32_t lastDrawAt = 0;
  String line1;
  String line2;
  String line3;
  String line4;
};
StatusUiState statusUi;

// Button-driven passkey entry shown during BLE pairing.
struct PasskeyUiState {
  PasskeyButtonEntry entry;
  bool active = false;
  uint8_t shownSeconds = 0;
};
PasskeyUiState passkeyUi;

// StickS3 battery level and charging indicator sampling.
struct DevicePowerState {
  int8_t batteryPercent = -1;
  bool charging = false;
  bool sampleFrameBoundary = false;
  bool sampleChargingNext = true;
  bool samplePending = true;
  uint32_t lastSampleAt = 0;
};
DevicePowerState devicePower;

constexpr uint32_t STATUS_MIN_REDRAW_MS = 1500;
constexpr uint32_t DEVICE_POWER_NORMAL_SAMPLE_MS = 2000;
constexpr uint32_t DEVICE_POWER_LIVE_SAMPLE_MS = 1500;
// Raw charging must stay off this long before the indicator clears; see
// charging_indicator_policy.h.
constexpr uint32_t DEVICE_CHARGING_OFF_HOLD_MS = 12000;
ChargingIndicatorFilter deviceChargingFilter(DEVICE_CHARGING_OFF_HOLD_MS);
void shutdownStickS3();
void updateUi();
void updateUi(const ButtonEvents& input);
void toggleDisplayMirror();

void sampleDeviceBatteryLevel() {
  const int32_t battery = M5.Power.getBatteryLevel();
  if (battery >= 0) {
    devicePower.batteryPercent = static_cast<int8_t>(battery > 100 ? 100 : battery);
  }
}

void sampleDeviceCharging() {
  const m5::Power_Class::is_charging_t state = M5.Power.isCharging();
  const bool rawCharging = state == m5::Power_Class::is_charging;
  if (deviceChargingFilter.update(rawCharging, millis())) {
    Serial.printf("Power indicator: charging=%d\n", deviceChargingFilter.charging() ? 1 : 0);
  }
  devicePower.charging = deviceChargingFilter.charging();
}

void serviceDevicePowerIndicator(uint32_t nowMs) {
  const bool previewRunning = camera.previewRunning();
  const uint32_t intervalMs = previewRunning
                                  ? DEVICE_POWER_LIVE_SAMPLE_MS
                                  : DEVICE_POWER_NORMAL_SAMPLE_MS;
  if ((!devicePower.samplePending && (nowMs - devicePower.lastSampleAt) < intervalMs) ||
      (previewRunning && !devicePower.sampleFrameBoundary)) {
    return;
  }

  devicePower.samplePending = false;
  devicePower.sampleFrameBoundary = false;
  devicePower.lastSampleAt = nowMs;
  if (!previewRunning) {
    sampleDeviceCharging();
    sampleDeviceBatteryLevel();
    return;
  }

  // LiveView performs at most one PMIC operation per completed-frame opportunity.
  if (devicePower.sampleChargingNext) {
    sampleDeviceCharging();
  } else {
    sampleDeviceBatteryLevel();
  }
  devicePower.sampleChargingNext = !devicePower.sampleChargingNext;
}

bool beginStickPower() {
  const int8_t sda = M5.getPin(m5::pin_name_t::in_i2c_sda);
  const int8_t scl = M5.getPin(m5::pin_name_t::in_i2c_scl);
  const m5pm1_err_t err = stickPower.begin(&Wire, M5PM1_DEFAULT_ADDR, sda, scl, M5PM1_I2C_FREQ_100K);
  pmic.ready = (err == M5PM1_OK);
  if (!pmic.ready) {
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
  if (pmic.ready) {
    bool pressed = false;
    const m5pm1_err_t err = stickPower.btnGetState(&pressed);
    if (err == M5PM1_OK) {
      return pressed;
    }

    // A failed M5PM1 I2C transaction can block for about a second. Retrying it
    // on every loop starves the 2 KB LiveView reads and prevents the first JPEG
    // frame from completing. Keep the M5Unified button/power-off fallback, but
    // stop polling the unhealthy I2C path until the next reboot.
    pmic.ready = false;
    LOGW("POWER",
         "Power: M5PM1 button read failed err=%d; disabling M5PM1 polling and using M5Unified",
         static_cast<int>(err));
  }
  return M5.BtnPWR.isPressed();
}

bool pollStickPowerHold() {
  const uint32_t now = millis();
  if ((now - pmic.buttonLastPollAt) < POWER_BUTTON_POLL_MS) {
    return false;
  }
  pmic.buttonLastPollAt = now;

  const bool pressed = isStickPowerButtonPressed();
  if (!pressed) {
    pmic.buttonPressedSince = 0;
    pmic.buttonHoldReported = false;
    return false;
  }

  if (pmic.buttonPressedSince == 0) {
    pmic.buttonPressedSince = now;
  }
  if (!pmic.buttonHoldReported && (now - pmic.buttonPressedSince) >= POWER_BUTTON_HOLD_MS) {
    pmic.buttonHoldReported = true;
    return true;
  }
  return false;
}

void showStatusIfChanged(const String& line1,
                         const String& line2 = String(),
                         const String& line3 = String(),
                         const String& line4 = String(),
                         bool force = false) {
  const uint32_t now = millis();
  const bool changed = force ||
                       line1 != statusUi.line1 ||
                       line2 != statusUi.line2 ||
                       line3 != statusUi.line3 ||
                       line4 != statusUi.line4;
  if (!changed) {
    return;
  }
  if (!force && (now - statusUi.lastDrawAt) < STATUS_MIN_REDRAW_MS) {
    return;
  }

  statusUi.line1 = line1;
  statusUi.line2 = line2;
  statusUi.line3 = line3;
  statusUi.line4 = line4;
  statusUi.lastDrawAt = now;
  updateUi();
}

void waitForSerialConsole() {
  const uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < SERIAL_BOOT_WAIT_MS) {
    delay(10);
  }
  Serial.setDebugOutput(false);
  Serial.println();
  LOGI("BOOT", "%s", FIRMWARE_BANNER);
}

void restoreDisplaySettings() {
  bool mirrored = false;
  if (profileStore.loadDisplayMirror(mirrored)) {
    ui.setMirrored(mirrored);
  }
  Serial.printf("Display: restored mirrored=%d\n", ui.mirrored() ? 1 : 0);
}

void toggleDisplayMirror() {
  const bool mirrored = ui.toggleMirror();
  const bool persisted = profileStore.saveDisplayMirror(mirrored);
  Serial.printf("Display: mirrored=%d persisted=%d\n",
                mirrored ? 1 : 0,
                persisted ? 1 : 0);
}

uint8_t passkeyRemainingSeconds() {
  const uint32_t remainingMs = passkeyUi.entry.remainingMs(millis());
  const uint32_t seconds = (remainingMs + 999U) / 1000U;
  return static_cast<uint8_t>(seconds > 255U ? 255U : seconds);
}

void showPasskeyEntryUi() {
  passkeyUi.shownSeconds = passkeyRemainingSeconds();
  ui.showPasskeyEntry(passkeyUi.entry.digits(), passkeyUi.entry.activeIndex(), passkeyUi.shownSeconds);
}

int32_t pollPasskeyButtonEntry(rvf::PasskeyPollAction action, bool& resetRequested) {
  if (action == rvf::PasskeyPollAction::Cancel) {
    passkeyUi.entry.reset();
    passkeyUi.active = false;
    return -1;
  }
  if (action == rvf::PasskeyPollAction::Start || !passkeyUi.active) {
    passkeyUi.entry.start(millis(), RICOH_BLE_PASSKEY_ENTRY_WAIT_MS);
    passkeyUi.active = true;
    showPasskeyEntryUi();
    return -1;
  }

  const ButtonEvents events = buttons.poll();
  if (events.resetPairing) {
    resetRequested = true;
    passkeyUi.entry.reset();
    passkeyUi.active = false;
    Serial.println("BLE pairing: canceled by pairing reset request");
    return -2;
  }
  if (passkeyUi.entry.status(millis()) == PasskeyEntryStatus::TimedOut) {
    passkeyUi.entry.reset();
    passkeyUi.active = false;
    Serial.println("BLE pairing: passkey entry timed out");
    return -2;
  }

  bool changed = false;
  if (M5.BtnA.wasClicked()) {
    passkeyUi.entry.shortPress();
    changed = true;
  } else if (M5.BtnB.wasClicked()) {
    changed = true;
    if (passkeyUi.entry.confirmDigit() == PasskeyEntryStatus::Complete) {
      showPasskeyEntryUi();
      return passkeyUi.entry.code();
    }
  } else if (M5.BtnA.wasHold()) {
    changed = true;
    if (passkeyUi.entry.submit() == PasskeyEntryStatus::Complete) {
      showPasskeyEntryUi();
      return passkeyUi.entry.code();
    }
  }
  if (changed || passkeyRemainingSeconds() != passkeyUi.shownSeconds) {
    showPasskeyEntryUi();
  }
  return -1;
}

bool pollButtonsDuringBlockingOperation() {
  const ButtonEvents events = buttons.poll();
  updateUi(events);
  return events.resetPairing;
}

void updateFps() {
  const uint32_t now = millis();
  fpsStats.windowFrames++;
  if (fpsStats.windowStart == 0) {
    fpsStats.windowStart = now;
  }
  const uint32_t elapsed = now - fpsStats.windowStart;
  if (elapsed >= 1000) {
    fpsStats.current = (fpsStats.windowFrames * 1000.0f) / static_cast<float>(elapsed);
    fpsStats.windowFrames = 0;
    fpsStats.windowStart = now;
  }
}

rvf::UiSnapshot makeUiSnapshot() {
  const rvf::CameraStatus status = camera.status();
  rvf::UiSnapshot snapshot;
  snapshot.cameraPhase = status.phase;
  snapshot.bleConnected = status.linkHealthy;
  snapshot.wifiConnected = status.wifiConnected;
  snapshot.previewRunning = status.previewRunning;
  snapshot.shutterReady = status.shutterReady;
  snapshot.cameraSleepLike = status.sleepGuardActive ||
                             status.phase == rvf::CameraPhase::Sleeping;
  snapshot.resettingPairing = status.resettingPairing;
  snapshot.pairingGuideActive = runtime.pairingGuide().active();
  snapshot.pairingGuideSelectedIndex = static_cast<uint8_t>(runtime.pairingGuide().selectedIndex());
  snapshot.wifiProvisioningActive = status.provisioningActive;
  snapshot.wifiProvisioningPreparing = status.provisioningPreparing;
  snapshot.hasFrame = fpsStats.decodedFrames > 0;
  snapshot.fps = fpsStats.current;
  snapshot.rssi = status.rssi;
  snapshot.decodedFrames = fpsStats.decodedFrames;
  snapshot.droppedFrames = status.droppedFrames;
  snapshot.deviceBatteryPercent = devicePower.batteryPercent;
  snapshot.deviceCharging = devicePower.charging;
  snapshot.cameraModel = status.cameraModel;
  snapshot.provisioningSsid = status.provisioningSsid;
  snapshot.provisioningPassword = status.provisioningPassword;
  snapshot.provisioningUrl = status.provisioningUrl;
  snapshot.errorTitle = statusUi.line1.c_str();
  snapshot.errorDetail = statusUi.line2.c_str();
  return snapshot;
}

rvf::UiOrientation sampleUiOrientation(const rvf::UiSnapshot& snapshot, uint32_t nowMs) {
  (void)snapshot;
  if (!rvf::UiTheme::kOrientationEnabled || !imuAvailable) {
    // Without posture input, preserve the historical full connection flow.
    return rvf::UiOrientation::Landscape;
  }
  if ((nowMs - lastOrientationSampleAt) >= rvf::UiTheme::kOrientationSampleMs) {
    lastOrientationSampleAt = nowMs;
    M5.Imu.update();
    float accelX = 0.0f;
    float accelY = 0.0f;
    float accelZ = 0.0f;
    if (M5.Imu.getAccel(&accelX, &accelY, &accelZ)) {
      orientationTracker.update(accelX, accelY, accelZ, nowMs);
    }
  }
  return orientationTracker.orientation();
}

void updateUi(const ButtonEvents& input) {
  currentUiInput = input;
  const uint32_t nowMs = millis();
  const rvf::UiSnapshot snapshot = makeUiSnapshot();
  const rvf::UiOrientation sensedOrientation = sampleUiOrientation(snapshot, nowMs);
  const rvf::UiOrientation orientation = runtime.applyPreviewOrientation(sensedOrientation);
  uiCoordinator.update(snapshot, input, orientation, nowMs);
  ui.render(uiCoordinator.viewModel());
  uiSound.play(uiCoordinator.consumeSound(), nowMs);
  uiSound.update(nowMs);
  currentUiInput.buttonA = false;
  currentUiInput.buttonADown = false;
  currentUiInput.buttonAReleased = false;
  currentUiInput.resetPairing = false;
  currentUiInput.buttonBClicked = false;
  currentUiInput.buttonBDoubleClicked = false;
  currentUiInput.toggleDisplayMirror = false;
  currentUiInput.toggleLiveViewLock = false;
  currentUiInput.powerOff = false;
}

void updateUi() {
  updateUi(currentUiInput);
}

bool syncJpegViewport(LovyanGFX* canvas) {
  if (canvas == nullptr) {
    return false;
  }
  const int16_t width = canvas->width();
  const int16_t height = canvas->height();
  if (width == jpegViewportWidth && height == jpegViewportHeight) {
    return true;
  }

  // JpegDecoder caches M5.Display dimensions in begin(). Refresh that cache
  // after DisplayUi rotates and recreates its Canvas, before the first frame.
  if (!decoder.begin()) {
    Serial.println("JPEG: viewport sync failed");
    return false;
  }
  jpegViewportWidth = width;
  jpegViewportHeight = height;
  Serial.printf("JPEG: viewport synced %dx%d\n", jpegViewportWidth, jpegViewportHeight);
  return true;
}

rvf::PreviewRenderResult renderLivePreviewFrame(const uint8_t* data, size_t len) {
  fpsStats.decodedFrames++;
  updateFps();
  // Only mark a safe opportunity here; PMIC I/O runs later in the main loop.
  devicePower.sampleFrameBoundary = true;

  rvf::PreviewRenderResult result;
  if (!uiCoordinator.shouldRenderLivePreview()) {
    // The stream remains active and all watchdog/frame timestamps continue to
    // advance while portrait UI intentionally skips JPEG decode and display.
    return result;
  }

  const uint32_t renderStartMs = millis();
  LovyanGFX* canvas = ui.beginLiveFrame();
  if (canvas == nullptr) {
    return result;
  }
  if (!syncJpegViewport(canvas)) {
    ui.finishLiveFrame(false);
    return result;
  }
  if (!decoder.drawFrame(canvas, data, len, ui.mirrored())) {
    Serial.printf("JPEG decode failed len=%u err=%s\n", static_cast<unsigned>(len), decoder.lastError().c_str());
    ui.finishLiveFrame(false);
  } else {
    ui.renderLiveFrameOverlay(uiCoordinator.viewModel());
    ui.finishLiveFrame(true);
  }
  result.attempted = true;
  result.decodeMs = decoder.lastDecodeMs();
  result.renderMs = millis() - renderStartMs;
  result.width = decoder.lastWidth();
  result.height = decoder.lastHeight();
  return result;
}

void shutdownStickS3() {
  static bool shuttingDown = false;
  if (shuttingDown) {
    return;
  }
  shuttingDown = true;

  Serial.println("Power: shutdown requested");
  camera.shutdown();

  ui.showBoot("Release PWR...");
  const uint32_t startMs = millis();
  while (isStickPowerButtonPressed() && (millis() - startMs) < POWER_BUTTON_RELEASE_WAIT_MS) {
    M5.update();
    delay(20);
    yield();
  }

  ui.showBoot("Power off...");
  delay(200);
  if (pmic.ready) {
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

const char* appEventTypeName(rvf::AppEventType type) {
  switch (type) {
    case rvf::AppEventType::None: return "None";
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
    case rvf::AppEventType::ErrorRaised: return "ErrorRaised";
  }
  return "Unknown";
}

rvf::SystemHealthSnapshot makeSystemHealthSnapshot(const rvf::CameraStatus& status) {
  rvf::SystemHealthSnapshot snapshot;
  snapshot.cameraPhase = status.phase;
  snapshot.bleConnected = status.linkHealthy;
  snapshot.wifiConnected = status.wifiConnected;
  snapshot.previewRunning = status.previewRunning;
  snapshot.liveviewEnabled = status.liveviewEnabled;
  snapshot.cameraSleepGuardActive = status.sleepGuardActive;
  snapshot.cameraRecoveryInProgress = status.recoveryInProgress;
  snapshot.lastFrameAt = status.lastFrameAt;
  snapshot.lastLiveViewActivityAt = status.lastLiveViewActivityAt;
  snapshot.liveViewStallTimeoutMs = status.liveViewStallTimeoutMs;
  return snapshot;
}

void serviceSystemSupervisor(uint32_t nowMs) {
  rvf::AppMessage message;
  const rvf::CameraStatus cameraStatus = camera.status();
  if (!systemSupervisor.check(nowMs, makeSystemHealthSnapshot(cameraStatus), message)) {
    return;
  }

  Serial.printf("Supervisor: event=%s state=%s code=%d detail=%s\n",
                appEventTypeName(message.type),
                cameraStatus.stateName,
                message.code,
                message.detail != nullptr ? message.detail : "");
}

class AppCameraHost : public rvf::ICameraHost {
public:
  void showStatus(const char* line1,
                  const char* line2,
                  const char* line3,
                  const char* line4,
                  bool force) override {
    showStatusIfChanged(line1, line2, line3, line4, force);
  }

  void refreshUi() override { updateUi(); }

  bool pollInputDuringBlockingOperation() override {
    return pollButtonsDuringBlockingOperation();
  }

  int32_t pollPasskeyEntry(rvf::PasskeyPollAction action, bool& resetRequested) override {
    return pollPasskeyButtonEntry(action, resetRequested);
  }

  void requestModelSelection(rvf::CameraModelId defaultModel) override {
    runtime.requestModelSelection(defaultModel);
  }

  void onShutterStarted() override { uiCoordinator.notifyShutterStarted(millis()); }

  void onShutterResult(bool success) override {
    uiCoordinator.notifyShutterResult(success, millis());
  }

  void onManualWake() override {
    uiCoordinator.notifyManualWake(millis());
    updateUi();
  }

  void onPairingResetStarted() override {
    uiCoordinator.notifyResetTriggered(millis());
    updateUi();
  }

  void releaseLiveViewLock() override { runtime.releaseLiveViewLock(); }

  rvf::PreviewRenderResult renderPreviewFrame(const uint8_t* data, size_t len) override {
    return renderLivePreviewFrame(data, len);
  }
};

AppCameraHost cameraHost;

uint32_t AppPlatform::nowMs() { return millis(); }
ButtonEvents AppPlatform::pollButtons() { return buttons.poll(); }
bool AppPlatform::pollPowerButtonHold() { return pollStickPowerHold(); }
void AppPlatform::shutdown() { shutdownStickS3(); }
void AppPlatform::serviceDevicePower() { serviceDevicePowerIndicator(millis()); }
void AppPlatform::refreshUi() { updateUi(); }
void AppPlatform::refreshUi(const ButtonEvents& input) { updateUi(input); }
void AppPlatform::superviseSystem() { serviceSystemSupervisor(millis()); }
void AppPlatform::toggleDisplayMirror() { ::toggleDisplayMirror(); }
void AppPlatform::idle() { delay(1); }
void AppPlatform::log(const char* text) { Serial.print(text); }
void AppPlatform::logLine(const char* text) { Serial.println(text); }


}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  systemSupervisor.begin(millis());

  if (!ui.begin()) {
    Serial.println("UI: initial Canvas creation failed");
  }
  uiCoordinator.begin(millis());
  orientationTracker.reset(rvf::UiOrientation::Portrait, millis());
  imuAvailable = rvf::UiTheme::kOrientationEnabled && M5.Imu.isEnabled();
  orientationTracker.setAvailable(imuAvailable);
  Serial.printf("UI: IMU %s; fallback=%s\n",
                imuAvailable ? "ready" : "unavailable",
                imuAvailable ? "none" : "preview landscape / status portrait");
  Serial.printf("UI: sound %s\n", uiSound.begin() ? "ready" : "disabled/unavailable");
  ui.showBoot(FIRMWARE_BOOT_MESSAGE);
  waitForSerialConsole();

  beginStickPower();
  serviceDevicePowerIndicator(millis());
  buttons.begin();
  decoder.begin();
  jpegViewportWidth = M5.Display.width();
  jpegViewportHeight = M5.Display.height();

  camera.begin(cameraHost);
  restoreDisplaySettings();

  if (!psramFound()) {
    LOGLINE_W("MEM", "PSRAM not found; JPEG buffer allocation may fail");
    ui.showError("PSRAM not found");
  }

  rvf::PreviewFrameBufferStorage frameBufferStorage = rvf::PreviewFrameBufferStorage::Psram;
  frameBuffer = static_cast<uint8_t*>(heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frameBuffer == nullptr) {
    frameBufferStorage = rvf::PreviewFrameBufferStorage::InternalRam;
    frameBuffer = static_cast<uint8_t*>(heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_8BIT));
  }
  if (frameBuffer == nullptr) {
    LOGLINE_E("MEM", "Failed to allocate JPEG frame buffer");
    ui.showError("Frame buffer alloc failed");
    while (true) {
      delay(1000);
    }
  }

  if (!camera.attachPreviewBuffer(frameBuffer, FRAME_BUFFER_SIZE, frameBufferStorage)) {
    LOGLINE_E("FRAME", "Failed to attach JPEG frame buffer");
    ui.showError("Frame buffer attach failed");
    while (true) {
      delay(1000);
    }
  }

  runtime.startCamera();
}

void loop() {
  runtime.tick();
}
