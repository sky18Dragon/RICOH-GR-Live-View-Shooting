#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "ble_pairing_policy.h"
#include "ble_discovery_policy.h"
#include "ble_reconnect_policy.h"
#include "ble_scan_lifecycle_policy.h"

#include "app/AppController.h"
#include "camera_identity.h"
#include "camera_profile_schema.h"
#include "camera_protocol_profile.h"
#include "ricoh/RicohBleProtocolRouter.h"
#include "camera_sleep_policy.h"
#include "image_fit.h"
#include "mjpeg_stream.h"
#include "supervisor/SystemSupervisor.h"
#include "ui/ButtonInput.h"
#include "ui/OrientationTracker.h"
#include "ui/UiAnimator.h"
#include "ui/UiCoordinator.h"

namespace {

struct CapturedFrames {
  uint32_t callbacks = 0;
  std::vector<uint8_t> lastFrame;
};

void captureFrame(const uint8_t* data, size_t len, void* user) {
  auto* captured = static_cast<CapturedFrames*>(user);
  ++captured->callbacks;
  captured->lastFrame.assign(data, data + len);
}

void assertDerivedBleName(const char* ssid, const char* expected) {
  const std::string actual = deriveBleNameFromWifiSsid(ssid);
  TEST_ASSERT_EQUAL_STRING(expected, actual.c_str());
}

void testContainFitPreservesFourByThreeFrame() {
  const ImageFitRect fit = calculateContainRect(320, 240, 240, 135);
  TEST_ASSERT_EQUAL_INT(180, fit.width);
  TEST_ASSERT_EQUAL_INT(135, fit.height);
  TEST_ASSERT_EQUAL_INT(30, fit.x);
  TEST_ASSERT_EQUAL_INT(0, fit.y);
}

void testContainFitPreservesWideFrame() {
  const ImageFitRect fit = calculateContainRect(640, 360, 240, 135);
  TEST_ASSERT_EQUAL_INT(240, fit.width);
  TEST_ASSERT_EQUAL_INT(135, fit.height);
  TEST_ASSERT_EQUAL_INT(0, fit.x);
  TEST_ASSERT_EQUAL_INT(0, fit.y);
}

void testContainFitLetterboxesTallFrame() {
  const ImageFitRect fit = calculateContainRect(135, 240, 240, 135);
  TEST_ASSERT_EQUAL_INT(76, fit.width);
  TEST_ASSERT_EQUAL_INT(135, fit.height);
  TEST_ASSERT_EQUAL_INT(82, fit.x);
  TEST_ASSERT_EQUAL_INT(0, fit.y);
}

void testContainFitRejectsInvalidDimensions() {
  const ImageFitRect fit = calculateContainRect(0, 240, 240, 135);
  TEST_ASSERT_EQUAL_INT(0, fit.width);
  TEST_ASSERT_EQUAL_INT(0, fit.height);
}

void testCameraSleepAutoPowerOffWaitsForTimeout() {
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(true, 1000, 30999, 30000));
  TEST_ASSERT_TRUE(cameraSleepAutoPowerOffDue(true, 1000, 31000, 30000));
}

void testCameraSleepAutoPowerOffRequiresActiveSleep() {
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(false, 1000, 50000, 30000));
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(true, 1000, 50000, 0));
}

void testCameraSleepAutoPowerOffHandlesMillisWrap() {
  constexpr uint32_t enteredAt = 0xFFFFFF00U;
  TEST_ASSERT_FALSE(cameraSleepAutoPowerOffDue(true, enteredAt, 0x000000F0U, 1000));
  TEST_ASSERT_TRUE(cameraSleepAutoPowerOffDue(true, enteredAt, 0x00000300U, 1000));
}

void testCameraSleepDisconnectReasonRejectsParkingFailure() {
  constexpr int remoteUser = 0x213;
  constexpr int remotePowerOff = 0x215;
  TEST_ASSERT_TRUE(isExplicitCameraSleepDisconnectReason(0x213, remoteUser, remotePowerOff));
  TEST_ASSERT_TRUE(isExplicitCameraSleepDisconnectReason(0x215, remoteUser, remotePowerOff));
  TEST_ASSERT_FALSE(isExplicitCameraSleepDisconnectReason(0x216, remoteUser, remotePowerOff));
  TEST_ASSERT_FALSE(isExplicitCameraSleepDisconnectReason(0, remoteUser, remotePowerOff));
}

struct FlowHarness {
  static bool bleConnected;
  static bool wifiConnected;
  static bool previewRunning;
  static bool cachedCredentials;
  static bool guardActive;
  static bool resetStackSucceeds;
  static bool activateSucceeds;
  static uint32_t lastRecoveryAt;
  static uint32_t activateCalls;
  static uint32_t readCredentialsCalls;
  static uint32_t applyCredentialsCalls;
  static uint32_t connectCalls;
  static uint32_t disconnectCalls;
  static uint32_t openPreviewCalls;
  static uint32_t runBleDiscoveryCalls;
  static uint32_t resetStackCalls;

  static void reset() {
    bleConnected = true;
    wifiConnected = false;
    previewRunning = false;
    cachedCredentials = false;
    guardActive = false;
    resetStackSucceeds = true;
    activateSucceeds = true;
    lastRecoveryAt = 0;
    activateCalls = 0;
    readCredentialsCalls = 0;
    applyCredentialsCalls = 0;
    connectCalls = 0;
    disconnectCalls = 0;
    openPreviewCalls = 0;
    runBleDiscoveryCalls = 0;
    resetStackCalls = 0;
  }

  static bool guardBlocks(const char*) { return guardActive; }
  static bool isGuardActive() { return guardActive; }
  static bool isBleConnected() { return bleConnected; }
  static bool isWifiConnected() { return wifiConnected; }
  static bool runBleDiscovery() {
    ++runBleDiscoveryCalls;
    return bleConnected;
  }
  static bool resetBleStack(const char*) {
    ++resetStackCalls;
    return resetStackSucceeds;
  }
  static bool activateWifi() {
    ++activateCalls;
    return activateSucceeds;
  }
  static bool hasCredentials() { return cachedCredentials; }
  static bool readCredentials() {
    ++readCredentialsCalls;
    return true;
  }
  static void applyCredentials() {
    ++applyCredentialsCalls;
    cachedCredentials = true;
  }
  static bool connectWifi() {
    ++connectCalls;
    wifiConnected = true;
    return true;
  }
  static void disconnectWifi() {
    ++disconnectCalls;
    wifiConnected = false;
    previewRunning = false;
  }
  static bool openPreview() {
    ++openPreviewCalls;
    previewRunning = wifiConnected;
    return previewRunning;
  }
  static bool isPreviewRunning() { return previewRunning; }
  static bool recoveryInactive() { return false; }
  static uint32_t getLastRecoveryAt() { return lastRecoveryAt; }
  static void setLastRecoveryAt(uint32_t value) { lastRecoveryAt = value; }

  static rvf::AppFlowActions actions() {
    rvf::AppFlowActions result;
    result.cameraSleepGuardBlocksFlow = guardBlocks;
    result.cameraSleepGuardActive = isGuardActive;
    result.isBleConnected = isBleConnected;
    result.isWifiConnected = isWifiConnected;
    result.disconnectWifi = disconnectWifi;
    result.runBleDiscovery = runBleDiscovery;
    result.activateCameraWifiOverBle = activateWifi;
    result.hasUsableCachedWifiCredentials = hasCredentials;
    result.connectCachedWifiFromProfile = connectWifi;
    result.readFreshWifiCredentials = readCredentials;
    result.applyFreshWifiCredentials = applyCredentials;
    result.connectFreshWifiFromProfile = connectWifi;
    result.openLiveView = openPreview;
    result.previewStreamRunning = isPreviewRunning;
    result.cameraRecoveryInProgress = recoveryInactive;
    result.resetBleStackBeforeScanAfterLinkLoss = resetBleStack;
    result.lastCameraRecoveryAt = getLastRecoveryAt;
    result.setLastCameraRecoveryAt = setLastRecoveryAt;
    result.liveviewEnabled = true;
    result.wifiOpenAttempts = 1;
    result.bleScanRetryIntervalMs = 1000;
    result.liveViewStallTimeoutMs = 5000;
    return result;
  }
};

bool FlowHarness::bleConnected = true;
bool FlowHarness::wifiConnected = false;
bool FlowHarness::previewRunning = false;
bool FlowHarness::cachedCredentials = false;
bool FlowHarness::guardActive = false;
bool FlowHarness::resetStackSucceeds = true;
bool FlowHarness::activateSucceeds = true;
uint32_t FlowHarness::lastRecoveryAt = 0;
uint32_t FlowHarness::activateCalls = 0;
uint32_t FlowHarness::readCredentialsCalls = 0;
uint32_t FlowHarness::applyCredentialsCalls = 0;
uint32_t FlowHarness::connectCalls = 0;
uint32_t FlowHarness::disconnectCalls = 0;
uint32_t FlowHarness::openPreviewCalls = 0;
uint32_t FlowHarness::runBleDiscoveryCalls = 0;
uint32_t FlowHarness::resetStackCalls = 0;

void testRecoveryStopsWhenBleStackResetFails() {
  FlowHarness::reset();
  FlowHarness::bleConnected = false;
  FlowHarness::resetStackSucceeds = false;
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  controller.recoverCameraConnection(actions, "BLE disconnected");

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::resetStackCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::runBleDiscoveryCalls);
}

void testPortraitStartupReturnsAsSoonAsBleShutterIsReady() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::BleReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::readCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::applyCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::openPreviewCalls);
}

void testPortraitBleReconnectReactivatesCameraWlan() {
  FlowHarness::reset();
  FlowHarness::cachedCredentials = true;
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::runBleDiscoveryCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::readCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::applyCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
}

void testPortraitReconnectKeepsWlanReadyForLandscapeConnect() {
  FlowHarness::reset();
  FlowHarness::cachedCredentials = true;
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));

  // Landscape only associates with the AP that was already reactivated.
  controller.setPreviewRequested(true);
  controller.serviceCameraFlowIfNeeded(actions, 200);

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::connectCalls);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::PreviewRunning),
                        static_cast<int>(controller.state()));
}

void testLandscapeStartupOpensLiveViewWithoutPropsProbe() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  controller.setPreviewRequested(true);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::PreviewRunning),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::connectCalls);
  TEST_ASSERT_TRUE(FlowHarness::wifiConnected);
  TEST_ASSERT_TRUE(FlowHarness::previewRunning);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::openPreviewCalls);
}

void testPortraitToLandscapeResumesAfterCredentialCache() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();
  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);

  controller.setPreviewRequested(true);
  controller.serviceCameraFlowIfNeeded(actions, 200);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::PreviewRunning),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::connectCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::openPreviewCalls);
}

void testLandscapeToPortraitDisconnectsWifiAndKeepsBleReady() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  controller.setPreviewRequested(true);
  const rvf::AppFlowActions actions = FlowHarness::actions();
  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));

  controller.setPreviewRequested(false);
  controller.serviceCameraFlowIfNeeded(actions, 200);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::disconnectCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_FALSE(FlowHarness::previewRunning);
  TEST_ASSERT_TRUE(FlowHarness::bleConnected);
}

void testBleReadyFailureKeepsScheduledRetryPaced() {
  FlowHarness::reset();
  FlowHarness::activateSucceeds = false;
  rvf::AppController controller(rvf::AppState::BleReady);
  controller.begin(rvf::AppState::BleReady);
  controller.setPreviewRequested(true);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  controller.serviceCameraFlowIfNeeded(actions, 2000);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(2000, FlowHarness::lastRecoveryAt);

  controller.serviceCameraFlowIfNeeded(actions, 2001);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
}

void testPortraitBleReadyDoesNotRetryOptionalWifiSetup() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleReady);
  controller.begin(rvf::AppState::BleReady);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  controller.serviceCameraFlowIfNeeded(actions, 2000);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::BleReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::activateCalls);
}

void testCameraSleepGuardKeepsControllerOutOfScan() {
  FlowHarness::reset();
  FlowHarness::bleConnected = false;
  FlowHarness::guardActive = true;
  rvf::AppController controller(rvf::AppState::CameraPowerOff);
  controller.begin(rvf::AppState::CameraPowerOff);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  controller.serviceCameraFlowIfNeeded(actions, 2000);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::CameraPowerOff),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::activateCalls);
}

void testBeginRejectsInvalidInputs() {
  MjpegStream stream;
  uint8_t buffer[32] = {};
  CapturedFrames captured;

  TEST_ASSERT_FALSE(stream.begin(nullptr, sizeof(buffer), captureFrame, &captured));
  TEST_ASSERT_FALSE(stream.begin(buffer, 3, captureFrame, &captured));
  TEST_ASSERT_FALSE(stream.begin(buffer, sizeof(buffer), nullptr, &captured));
  TEST_ASSERT_EQUAL_size_t(0, stream.process(buffer, sizeof(buffer)));
}

void testDeliversFrameSplitAcrossChunks() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t prefix[] = {0x00, 0x11, 0xFF};
  const uint8_t middle[] = {
      0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF,
  };
  const uint8_t suffix[] = {0xD9, 0x22};
  const uint8_t expected[] = {
      0xFF, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0xD9,
  };

  TEST_ASSERT_EQUAL_size_t(0, stream.process(prefix, sizeof(prefix)));
  TEST_ASSERT_EQUAL_size_t(0, stream.process(middle, sizeof(middle)));
  TEST_ASSERT_EQUAL_size_t(1, stream.process(suffix, sizeof(suffix)));

  TEST_ASSERT_EQUAL_UINT32(1, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(1, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(0, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(sizeof(expected), captured.lastFrame.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, captured.lastFrame.data(), sizeof(expected));
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDropsShortFrame() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t shortFrame[] = {0xFF, 0xD8, 0x01, 0xFF, 0xD9};

  TEST_ASSERT_EQUAL_size_t(0, stream.process(shortFrame, sizeof(shortFrame)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDropsOverflowFrameWhenEoiArrives() {
  MjpegStream stream;
  uint8_t buffer[8] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t frame[] = {
      0xFF, 0xD8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0xFF, 0xD9,
  };

  TEST_ASSERT_EQUAL_size_t(0, stream.process(frame, sizeof(frame)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testResetClearsPartialFrameOnly() {
  MjpegStream stream;
  uint8_t buffer[64] = {};
  CapturedFrames captured;
  TEST_ASSERT_TRUE(stream.begin(buffer, sizeof(buffer), captureFrame, &captured));

  const uint8_t shortFrame[] = {0xFF, 0xD8, 0x01, 0xFF, 0xD9};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(shortFrame, sizeof(shortFrame)));

  const uint8_t partial[] = {0xFF, 0xD8, 0x01, 0x02};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(partial, sizeof(partial)));
  TEST_ASSERT_GREATER_THAN_size_t(0, stream.currentLength());

  stream.reset();

  const uint8_t suffix[] = {0x03, 0xFF, 0xD9};
  TEST_ASSERT_EQUAL_size_t(0, stream.process(suffix, sizeof(suffix)));
  TEST_ASSERT_EQUAL_UINT32(0, captured.callbacks);
  TEST_ASSERT_EQUAL_UINT32(0, stream.frames());
  TEST_ASSERT_EQUAL_UINT32(1, stream.droppedFrames());
  TEST_ASSERT_EQUAL_size_t(0, stream.currentLength());
}

void testDerivesBleNameFromRicohWifiSsid() {
  assertDerivedBleName("GR_H264456", "GR_H264457");
  assertDerivedBleName("GR_000099", "GR_000100");
  assertDerivedBleName("GR_H009999", "GR_H010000");
}

void testLeavesNonNumericRicohWifiSsidUnchanged() {
  assertDerivedBleName("GR_", "GR_");
  assertDerivedBleName("GR_H", "GR_H");
  assertDerivedBleName("GR_HABC123", "GR_HABC123");
}

void testRejectsNonRicohWifiSsidForBleName() {
  assertDerivedBleName("", "");
  assertDerivedBleName("RICOH_GR", "");
  assertDerivedBleName("XGR_H264456", "");
}

void testRequiresBleAddressAndAddressTypeForDirectReconnect() {
  TEST_ASSERT_TRUE(hasDirectBleReconnectIdentity("aa:bb:cc:dd:ee:ff", true));
  TEST_ASSERT_FALSE(hasDirectBleReconnectIdentity("aa:bb:cc:dd:ee:ff", false));
  TEST_ASSERT_FALSE(hasDirectBleReconnectIdentity("", true));
  TEST_ASSERT_FALSE(hasDirectBleReconnectIdentity(nullptr, true));
}

void testDirectReconnectIsOnlyUsedForKnownBondedBootProfile() {
  constexpr const char* address = "aa:bb:cc:dd:ee:ff";
  TEST_ASSERT_TRUE(shouldAttemptDirectBleReconnect(true, true, true, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(false, true, true, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(true, false, true, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(true, true, false, address, true));
  TEST_ASSERT_FALSE(shouldAttemptDirectBleReconnect(true, true, true, address, false));
}

void testProtocolDiscoveryRefreshesOnlyRequiredCharacteristicServices() {
  TEST_ASSERT_TRUE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::SharedWlan));
  TEST_ASSERT_TRUE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::Camera));
  TEST_ASSERT_TRUE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::Shooting));
  TEST_ASSERT_FALSE(shouldRefreshProtocolDiscoveryCharacteristics(
      BleProtocolDiscoveryService::Control));
}

void testNewPeerMustPersistBondBeforeConnectionSucceeds() {
  constexpr unsigned long timeoutMs = 1000;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Wait),
      static_cast<int>(decideBleBondPersistence(false, true, false, 100, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Wait),
      static_cast<int>(decideBleBondPersistence(false, true, false, 300, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Wait),
      static_cast<int>(decideBleBondPersistence(false, true, false, 500, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Ready),
      static_cast<int>(decideBleBondPersistence(false, true, true, 500, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Ready),
      static_cast<int>(decideBleBondPersistence(true, true, false, timeoutMs, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Disconnected),
      static_cast<int>(decideBleBondPersistence(true, false, false, 0, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::Disconnected),
      static_cast<int>(decideBleBondPersistence(false, false, false, 500, timeoutMs)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(BleBondPersistenceDecision::TimedOut),
      static_cast<int>(decideBleBondPersistence(false, true, false, timeoutMs, timeoutMs)));
}

void testBleScanCleanupRequiresHostAndCallbackQuiescence() {
  TEST_ASSERT_TRUE(canCleanupBleScanSession(false, false, false, false, false, true, false));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(false, false, false, false, false, false, true));

  // Natural completion requires the host's onScanEnd signal.
  TEST_ASSERT_TRUE(canCleanupBleScanSession(true, false, true, true, true, true, false));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, false, true, false, true, true, true));

  // NimBLE stop() does not emit onScanEnd; successful cancellation, an
  // inactive controller, callback quiescence and a host-queue fence are the
  // safe terminal state.
  TEST_ASSERT_TRUE(canCleanupBleScanSession(true, true, true, false, true, true, true));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, true, false, true, true, false));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, false, false, true, true, true));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, true, false, false, true, true));
  TEST_ASSERT_FALSE(canCleanupBleScanSession(true, true, true, false, true, false, true));
}

void testBleStackObjectsClearOnlyAfterHostStops() {
  TEST_ASSERT_TRUE(canUseBleStack(false));
  TEST_ASSERT_FALSE(canUseBleStack(true));

  TEST_ASSERT_FALSE(canClearBleStackObjects(false));
  TEST_ASSERT_TRUE(canClearBleStackObjects(true));

  TEST_ASSERT_FALSE(canRestartBleStack(false, false, true));
  TEST_ASSERT_FALSE(canRestartBleStack(false, true, true));
  TEST_ASSERT_TRUE(canRestartBleStack(true, false, false));
  TEST_ASSERT_TRUE(canRestartBleStack(true, true, true));
  TEST_ASSERT_FALSE(canRestartBleStack(true, true, false));
}

void testBleCandidateDiscoveryIsOpenWithoutStoredIdentity() {
  TEST_ASSERT_TRUE(bleCandidateMatchesStoredIdentity("", "34:90:ea:cc:87:35"));
  TEST_ASSERT_TRUE(bleCandidateMatchesStoredIdentity(nullptr, "34:90:ea:cc:87:35"));
}

void testBleCandidateMustMatchStoredIdentity() {
  TEST_ASSERT_TRUE(bleCandidateMatchesStoredIdentity("34:90:EA:CC:87:35",
                                                    "34:90:ea:cc:87:35"));
  TEST_ASSERT_FALSE(bleCandidateMatchesStoredIdentity("34:90:ea:cc:87:35",
                                                     "f0:3e:05:26:44:57"));
  TEST_ASSERT_FALSE(bleCandidateMatchesStoredIdentity("34:90:ea:cc:87:35", ""));
  TEST_ASSERT_FALSE(bleCandidateMatchesStoredIdentity("34:90:ea:cc:87:35", nullptr));
}

rvf::SystemHealthSnapshot healthyPreviewSnapshot() {
  rvf::SystemHealthSnapshot snapshot;
  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.bleConnected = true;
  snapshot.wifiConnected = true;
  snapshot.previewRunning = true;
  snapshot.liveviewEnabled = true;
  snapshot.lastFrameAt = 1000;
  snapshot.lastLiveViewActivityAt = 1000;
  snapshot.liveViewStallTimeoutMs = 5000;
  return snapshot;
}

void testSupervisorWaitsForIntervalAndIgnoresHealthyPreview() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  supervisor.begin(1000);

  TEST_ASSERT_FALSE(supervisor.check(1999, healthyPreviewSnapshot(), message));
  TEST_ASSERT_FALSE(supervisor.check(2000, healthyPreviewSnapshot(), message));
}

void testSupervisorReportsPreviewClosed() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.previewRunning = false;
  supervisor.begin(0);

  TEST_ASSERT_TRUE(supervisor.check(1000, snapshot, message));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppEventType::PreviewStopped), static_cast<int>(message.type));
  TEST_ASSERT_EQUAL_STRING("supervisor preview closed", message.detail);
}

void testSupervisorIgnoresCameraSleepGuard() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.previewRunning = false;
  snapshot.cameraSleepGuardActive = true;
  supervisor.begin(0);

  TEST_ASSERT_FALSE(supervisor.check(1000, snapshot, message));
}

void testSupervisorReportsPreviewIdleTimeout() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.lastFrameAt = 1000;
  snapshot.lastLiveViewActivityAt = 1000;
  snapshot.liveViewStallTimeoutMs = 5000;
  supervisor.begin(0);

  TEST_ASSERT_TRUE(supervisor.check(7001, snapshot, message));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppEventType::PreviewTimeout), static_cast<int>(message.type));
  TEST_ASSERT_EQUAL_INT(6001, message.code);
  TEST_ASSERT_EQUAL_STRING("supervisor preview frame idle", message.detail);
}

void testSupervisorReportsFrameStallDespiteIncomingBytes() {
  rvf::SystemSupervisor supervisor;
  rvf::AppMessage message;
  rvf::SystemHealthSnapshot snapshot = healthyPreviewSnapshot();
  snapshot.lastFrameAt = 1000;
  snapshot.lastLiveViewActivityAt = 6500;
  snapshot.liveViewStallTimeoutMs = 5000;
  supervisor.begin(0);

  TEST_ASSERT_TRUE(supervisor.check(7001, snapshot, message));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppEventType::PreviewTimeout), static_cast<int>(message.type));
  TEST_ASSERT_EQUAL_INT(6001, message.code);
  TEST_ASSERT_EQUAL_STRING("supervisor preview frame idle", message.detail);
}

void testUiMapsAppStatesToScenes() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::BleScan;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::Pairing),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Portrait)));

  snapshot.appState = rvf::AppState::ConnectingBle;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::Connecting),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Portrait)));

  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.previewRunning = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::RemoteReady),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Portrait)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::LivePreview),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape)));
}

void testPortraitBleReadyShowsRemoteInsteadOfMergedConnectionDot() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::BleReady;
  snapshot.bleConnected = true;
  snapshot.shutterReady = true;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::RemoteReady),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Portrait)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::Connecting),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape)));

  snapshot.appState = rvf::AppState::CheckingCameraPower;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::RemoteReady),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Portrait)));
}

void testUiPropagatesDeviceChargingIndicator() {
  rvf::UiCoordinator coordinator;
  rvf::UiSnapshot snapshot;
  rvf::ButtonEvents input;
  snapshot.appState = rvf::AppState::BleReady;
  snapshot.deviceBatteryPercent = 68;
  snapshot.deviceCharging = true;

  coordinator.begin(0);
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 1);

  TEST_ASSERT_EQUAL_INT8(68, coordinator.viewModel().deviceBatteryPercent);
  TEST_ASSERT_TRUE(coordinator.viewModel().deviceCharging);

  snapshot.deviceCharging = false;
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 2);
  TEST_ASSERT_FALSE(coordinator.viewModel().deviceCharging);
}

void testConnectingDotsOnlyMergeAfterBleConnects() {
  rvf::UiCoordinator coordinator;
  rvf::UiSnapshot snapshot;
  rvf::ButtonEvents input;
  snapshot.appState = rvf::AppState::ConnectingBle;
  snapshot.bleConnected = false;
  coordinator.begin(0);
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 5000);
  TEST_ASSERT_FALSE(coordinator.viewModel().bleConnected);
  TEST_ASSERT_TRUE(coordinator.viewModel().sceneProgress < 0.94f);

  snapshot.bleConnected = true;
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 5001);
  TEST_ASSERT_TRUE(coordinator.viewModel().bleConnected);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, coordinator.viewModel().sceneProgress);
}

void testUiScenePriority() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.previewRunning = true;
  snapshot.cameraSleepLike = true;
  snapshot.resettingPairing = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::ResetPairing),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape)));
}

void testInitialCameraSelectionSuppressesResetVisual() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::InitialCameraSelection;
  snapshot.initialCameraSelectionActive = true;
  snapshot.resettingPairing = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::InitialCameraSelection),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Portrait, true)));
}

void testOrientationRequiresStableCandidate() {
  rvf::OrientationTracker tracker(rvf::UiOrientation::Portrait);
  tracker.reset(rvf::UiOrientation::Portrait, 0);
  TEST_ASSERT_FALSE(tracker.update(0.0f, 1.0f, 0.0f, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Portrait),
                        static_cast<int>(tracker.orientation()));
  TEST_ASSERT_FALSE(tracker.update(0.0f, 1.0f, 0.0f, 599));
  TEST_ASSERT_TRUE(tracker.update(0.0f, 1.0f, 0.0f, 600));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Landscape),
                        static_cast<int>(tracker.orientation()));
}

void testOrientationMapsStickS3PhysicalAxes() {
  rvf::OrientationTracker portraitTracker(rvf::UiOrientation::Landscape);
  portraitTracker.reset(rvf::UiOrientation::Landscape, 0);
  portraitTracker.update(1.0f, 0.0f, 0.0f, 100);
  TEST_ASSERT_TRUE(portraitTracker.update(1.0f, 0.0f, 0.0f, 600));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Portrait),
                        static_cast<int>(portraitTracker.orientation()));

  rvf::OrientationTracker landscapeTracker(rvf::UiOrientation::Portrait);
  landscapeTracker.reset(rvf::UiOrientation::Portrait, 0);
  landscapeTracker.update(0.0f, 1.0f, 0.0f, 100);
  TEST_ASSERT_TRUE(landscapeTracker.update(0.0f, 1.0f, 0.0f, 600));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Landscape),
                        static_cast<int>(landscapeTracker.orientation()));
}

void testOrientationHysteresisPreventsBoundaryChatter() {
  rvf::OrientationTracker tracker(rvf::UiOrientation::Portrait);
  tracker.reset(rvf::UiOrientation::Portrait, 0);
  tracker.update(0.0f, 1.0f, 0.0f, 100);
  tracker.update(0.0f, 1.0f, 0.0f, 600);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Landscape),
                        static_cast<int>(tracker.orientation()));
  TEST_ASSERT_FALSE(tracker.update(0.55f, 0.50f, 0.0f, 1100));
  TEST_ASSERT_FALSE(tracker.update(0.50f, 0.55f, 0.0f, 1700));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Landscape),
                        static_cast<int>(tracker.orientation()));
}

void testLiveViewLockForcesLandscapeAndUnlockRestoresPosture() {
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(rvf::UiOrientation::Landscape),
      static_cast<int>(rvf::UiCoordinator::resolvePreviewOrientation(
          rvf::UiOrientation::Portrait, true)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(rvf::UiOrientation::Portrait),
      static_cast<int>(rvf::UiCoordinator::resolvePreviewOrientation(
          rvf::UiOrientation::Portrait, false)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(rvf::UiOrientation::Landscape),
      static_cast<int>(rvf::UiCoordinator::resolvePreviewOrientation(
          rvf::UiOrientation::Landscape, false)));
}

void testActivePreviewOutlivesPortraitUiForLockHandoff() {
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.previewRunning = true;

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(rvf::UiScene::RemoteReady),
      static_cast<int>(rvf::UiCoordinator::selectScene(
          snapshot, rvf::UiOrientation::Portrait)));
  TEST_ASSERT_TRUE(controller.isPreviewActive());
}

void testAnimationProgressAndCompletion() {
  rvf::AnimationState animation;
  animation.start(1000, 1000);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, animation.progress(1000));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5f, animation.progress(1500));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, animation.progress(2000));
  TEST_ASSERT_TRUE(animation.update(2000));
  TEST_ASSERT_FALSE(animation.active);
}

void testAnimationElapsedIsMillisWrapSafe() {
  constexpr uint32_t start = UINT32_MAX - 100U;
  TEST_ASSERT_EQUAL_UINT32(151U, rvf::uiElapsedMs(50U, start));
  rvf::AnimationState animation;
  animation.start(start, 200U);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.755f, animation.progress(50U));
}

void testButtonBReportsContinuousProgress() {
  rvf::ButtonInput input(3000);
  input.reset();
  input.update(false, true, false, 100);
  const rvf::ButtonEvents halfway = input.update(false, true, false, 1600);
  TEST_ASSERT_TRUE(halfway.resetHoldActive);
  TEST_ASSERT_EQUAL_UINT32(1500, halfway.resetHoldMs);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, halfway.resetHoldProgress);
  TEST_ASSERT_FALSE(halfway.resetPairing);
}

void testButtonBSingleClickTogglesMirrorAfterDoubleClickWindow() {
  rvf::ButtonInput input(3000, 350);
  input.update(false, true, false, 100);
  const rvf::ButtonEvents released = input.update(false, false, false, 200);
  TEST_ASSERT_FALSE(released.resetPairing);
  TEST_ASSERT_FALSE(released.resetHoldActive);
  TEST_ASSERT_FALSE(released.toggleDisplayMirror);
  const rvf::ButtonEvents confirmed = input.update(false, false, false, 551);
  TEST_ASSERT_TRUE(confirmed.toggleDisplayMirror);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::ToggleDisplayMirror),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(confirmed)));
}

void testButtonBDoubleClickTogglesLiveViewLockWithoutMirror() {
  rvf::ButtonInput input(3000, 350);
  input.update(false, true, false, 100);
  const rvf::ButtonEvents firstRelease = input.update(false, false, false, 160);
  TEST_ASSERT_FALSE(firstRelease.toggleDisplayMirror);
  TEST_ASSERT_FALSE(firstRelease.toggleLiveViewLock);

  input.update(false, true, false, 260);
  const rvf::ButtonEvents secondRelease = input.update(false, false, false, 320);
  TEST_ASSERT_TRUE(secondRelease.toggleLiveViewLock);
  TEST_ASSERT_FALSE(secondRelease.toggleDisplayMirror);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::ToggleLiveViewLock),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(secondRelease)));

  const rvf::ButtonEvents afterWindow = input.update(false, false, false, 1000);
  TEST_ASSERT_FALSE(afterWindow.toggleDisplayMirror);
  TEST_ASSERT_FALSE(afterWindow.toggleLiveViewLock);
}

void testButtonBThresholdTriggersOnlyOnce() {
  rvf::ButtonInput input(3000);
  input.update(false, true, false, 100);
  TEST_ASSERT_TRUE(input.update(false, true, false, 3100).resetPairing);
  TEST_ASSERT_FALSE(input.update(false, true, false, 4100).resetPairing);
  TEST_ASSERT_FALSE(input.update(false, true, false, 5100).resetPairing);
  const rvf::ButtonEvents released = input.update(false, false, false, 5200);
  TEST_ASSERT_FALSE(released.toggleDisplayMirror);
  TEST_ASSERT_FALSE(released.toggleLiveViewLock);
}

void testButtonAOperationTriggersAtMostOneShoot() {
  rvf::ButtonInput input;
  const rvf::ButtonEvents pressed = input.update(true, false, false, 100);
  TEST_ASSERT_TRUE(pressed.buttonADown);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::Shoot),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(pressed)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::None),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(
                            input.update(true, false, false, 500))));
  const rvf::ButtonEvents released = input.update(false, false, false, 600);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::None),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(released)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::None),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(
                            input.update(false, false, false, 700))));
}

void testShutterOverlaySuccessAndFailureLifecycles() {
  rvf::UiCoordinator coordinator;
  coordinator.begin(0);
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.previewRunning = true;
  rvf::ButtonEvents input;
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 10);
  coordinator.notifyShutterStarted(50);
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 50);
  TEST_ASSERT_TRUE(coordinator.viewModel().focusActive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, coordinator.viewModel().focusProgress);
  coordinator.notifyShutterResult(true, 100);
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 100);
  TEST_ASSERT_TRUE(coordinator.viewModel().shutterOverlayActive);
  TEST_ASSERT_FALSE(coordinator.viewModel().shutterFailed);
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 400);
  TEST_ASSERT_FALSE(coordinator.viewModel().shutterOverlayActive);

  coordinator.notifyShutterResult(false, 500);
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 500);
  TEST_ASSERT_TRUE(coordinator.viewModel().shutterOverlayActive);
  TEST_ASSERT_TRUE(coordinator.viewModel().shutterFailed);
}

void testSleepSceneOverridesOrientationScene() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::PreviewRunning;
  snapshot.previewRunning = true;
  snapshot.cameraSleepLike = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::CameraSleep),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape)));
}

void testErrorSceneOverridesEveryOrdinaryScene() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::Error;
  snapshot.previewRunning = true;
  snapshot.cameraSleepLike = true;
  snapshot.resettingPairing = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::Error),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape, true)));
}

void testDetectsProtocolOnlyFromSafeEvidence() {
  ProtocolDetectionEvidence gr3;
  gr3.gattDiscoveryComplete = true;
  gr3.hasGr3WlanService = true;
  gr3.hasGr3NetworkTypeCharacteristic = true;
  gr3.hasGr3SsidCharacteristic = true;
  gr3.hasGr3PassphraseCharacteristic = true;
  gr3.hasCameraService = true;
  gr3.hasOperationModeCharacteristic = true;
  gr3.hasShootingService = true;
  gr3.hasShootingFlavorCharacteristic = true;
  gr3.hasOperationRequestCharacteristic = true;
  gr3.hasControlService = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(detectRicohProtocol(gr3)));

  ProtocolDetectionEvidence incompleteGr3;
  incompleteGr3.gattDiscoveryComplete = true;
  incompleteGr3.hasGr3WlanService = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Unknown),
                        static_cast<int>(detectRicohProtocol(incompleteGr3)));

  ProtocolDetectionEvidence interruptedGr4MisleadingAsGr3 = gr3;
  interruptedGr4MisleadingAsGr3.gattDiscoveryComplete = false;
  interruptedGr4MisleadingAsGr3.hasGr4PowerCharacteristicAtExpectedHandle = false;
  interruptedGr4MisleadingAsGr3.gr4ExpectedWlanCharacteristicCount = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(interruptedGr4MisleadingAsGr3)));

  ProtocolDetectionEvidence gr4;
  gr4.hasCameraService = true;
  gr4.hasOperationModeCharacteristic = true;
  gr4.hasShootingService = true;
  gr4.hasShootingFlavorCharacteristic = true;
  gr4.hasOperationRequestCharacteristic = true;
  gr4.hasControlService = true;
  gr4.hasGr4PowerCharacteristicAtExpectedHandle = true;
  gr4.gr4WlanHandlesInExpectedService = true;
  gr4.gr4KnownWlanUuidHandleMapping = true;
  gr4.gr4ExpectedWlanCharacteristicCount = 6;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(detectRicohProtocol(gr4)));

  ProtocolDetectionEvidence gr4WithSharedWlanUuids = gr4;
  gr4WithSharedWlanUuids.hasGr3WlanService = true;
  gr4WithSharedWlanUuids.hasGr3NetworkTypeCharacteristic = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(detectRicohProtocol(gr4WithSharedWlanUuids)));

  ProtocolDetectionEvidence crossServiceNumericCollision = gr4;
  crossServiceNumericCollision.gr4WlanHandlesInExpectedService = false;
  crossServiceNumericCollision.gr4KnownWlanUuidHandleMapping = false;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(crossServiceNumericCollision)));

  ProtocolDetectionEvidence fiveOfSixGr4Handles = gr4;
  fiveOfSixGr4Handles.gr4ExpectedWlanCharacteristicCount = 5;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(fiveOfSixGr4Handles)));

  ProtocolDetectionEvidence wrongKnownUuidMapping = gr4;
  wrongKnownUuidMapping.gr4KnownWlanUuidHandleMapping = false;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(RicohProtocolGeneration::Unknown),
      static_cast<int>(detectRicohProtocol(wrongKnownUuidMapping)));

  ProtocolDetectionEvidence incompleteFixedHandleConflict = gr3;
  incompleteFixedHandleConflict.hasGr4PowerCharacteristicAtExpectedHandle = true;
  incompleteFixedHandleConflict.gr4WlanHandlesInExpectedService = false;
  incompleteFixedHandleConflict.gr4KnownWlanUuidHandleMapping = false;
  incompleteFixedHandleConflict.gr4ExpectedWlanCharacteristicCount = 3;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Unknown),
                        static_cast<int>(detectRicohProtocol(incompleteFixedHandleConflict)));
}

void testSecurityProfilesKeepGr4LegacyFrozen() {
  const RicohSecurityProfile& gr3 =
      ricohSecurityProfile(RicohSecurityProfileId::Gr3Passkey);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleIoCapability::KeyboardDisplay),
                        static_cast<int>(gr3.ioCapability));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleOwnAddressMode::Public),
                        static_cast<int>(gr3.ownAddressMode));
  TEST_ASSERT_TRUE(gr3.distributeEncryptionKey);
  TEST_ASSERT_TRUE(gr3.distributeIdentityKey);
  TEST_ASSERT_TRUE(gr3.distributeSigningKey);
  TEST_ASSERT_FALSE(gr3.usesFixedPasskey);

  const RicohSecurityProfile& gr4 =
      ricohSecurityProfile(RicohSecurityProfileId::Gr4Legacy);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleIoCapability::DisplayYesNo),
                        static_cast<int>(gr4.ioCapability));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohBleOwnAddressMode::RpaPublicDefault),
                        static_cast<int>(gr4.ownAddressMode));
  TEST_ASSERT_TRUE(gr4.distributeEncryptionKey);
  TEST_ASSERT_TRUE(gr4.distributeIdentityKey);
  TEST_ASSERT_FALSE(gr4.distributeSigningKey);
  TEST_ASSERT_TRUE(gr4.usesFixedPasskey);
  TEST_ASSERT_EQUAL_UINT32(123456, gr4.fixedPasskey);
}

void testOnlyGr4DiscoveryCanReuseTheLegacyConnection() {
  TEST_ASSERT_TRUE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Unknown,
      RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_TRUE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Gr4Legacy,
      RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_FALSE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Unknown,
      RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_FALSE(canPromoteDiscoveryConnectionInPlace(
      RicohSecurityProfileId::Gr3Passkey,
      RicohProtocolGeneration::Gr4Family));
}

void testProtocolRouterSelectsExactlyOneImplementation() {
  RicohBleProtocolRouter router;
  TEST_ASSERT_FALSE(router.hasProtocol());
  TEST_ASSERT_FALSE(router.select(RicohProtocolGeneration::Unknown));
  TEST_ASSERT_FALSE(router.hasProtocol());

  TEST_ASSERT_TRUE(router.select(RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_TRUE(router.hasProtocol());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(router.generation()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohSecurityProfileId::Gr4Legacy),
                        static_cast<int>(router.securityProfile()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohWifiActivationTransport::FixedHandle),
                        static_cast<int>(router.protocol()->wifiActivationTransport()));
  TEST_ASSERT_FALSE(router.protocol()->requireAuthenticatedBond());

  TEST_ASSERT_TRUE(router.select(RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(router.profile().generation));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohSecurityProfileId::Gr3Passkey),
                        static_cast<int>(router.securityProfile()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohWifiActivationTransport::NetworkTypeUuid),
                        static_cast<int>(router.protocol()->wifiActivationTransport()));
  TEST_ASSERT_TRUE(router.protocol()->requireAuthenticatedBond());

  TEST_ASSERT_FALSE(router.select(RicohProtocolGeneration::Gr2Family));
  TEST_ASSERT_FALSE(router.hasProtocol());
}

void testUnknownAndGr2ProfilesBlockBleSideEffects() {
  const CameraProtocolProfile& unknown = cameraProtocolProfile(RicohProtocolGeneration::Unknown);
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::WifiActivation));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::CameraPowerWrite));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(unknown, BleSideEffect::Shutter));

  const CameraProtocolProfile& gr2 = cameraProtocolProfile(RicohProtocolGeneration::Gr2Family);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiActivationMethod::ManualOnly),
                        static_cast<int>(gr2.wifiActivationMethod));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiCredentialMethod::ManualConfiguration),
                        static_cast<int>(gr2.wifiCredentialMethod));
  TEST_ASSERT_FALSE(protocolAllowsBleSideEffect(gr2, BleSideEffect::WifiActivation));
}

void testOperationModeSafetyIsGenerationSpecific() {
  const CameraProtocolProfile& gr3 = cameraProtocolProfile(RicohProtocolGeneration::Gr3Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, true));
  // A GR IIIx ready to shoot reports Other, not Capture.
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Other, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::BleStartup, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::PowerOffTransfer, true));
  // Unlike GR IV, GR III still refuses to act on a mode it could not read.
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, false));

  const CameraProtocolProfile& gr4 = cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Unknown, false));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::PowerOffTransfer, true));
}

void testHttpShutterIsOnlyClaimedWhereItWasVerified() {
  // Parking the BLE link moves the shutter onto POST /v1/camera/shoot, so a
  // generation may only be parked once that endpoint is confirmed on real
  // hardware. GR III is; GR IV has not been tested and must stay untouched.
  TEST_ASSERT_TRUE(cameraProtocolProfile(RicohProtocolGeneration::Gr3Family)
                       .capabilities.supportsHttpShutter);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr4Family)
                        .capabilities.supportsHttpShutter);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Gr2Family)
                        .capabilities.supportsHttpShutter);
  TEST_ASSERT_FALSE(cameraProtocolProfile(RicohProtocolGeneration::Unknown)
                        .capabilities.supportsHttpShutter);
}

void testGr3CredentialShapeAllowsOptionalChannel() {
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 0));
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 11));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "secret", 12));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("", "secret", 1));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "", 1));
}

void testOldGr4ProfileMetadataMigratesWithoutRepairing() {
  StoredCameraProfileMetadata old;
  old.schemaVersion = 3;
  old.legacyBleIdentityPresent = true;
  old.legacyWifiValid = true;
  const CameraProfileMetadata decoded = decodeCameraProfileMetadata(old);
  TEST_ASSERT_EQUAL_UINT32(CAMERA_PROFILE_SCHEMA_VERSION, decoded.schemaVersion);
  TEST_ASSERT_TRUE(decoded.protocolGenerationKnown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RicohProtocolGeneration::Gr4Family),
                          decoded.protocolGeneration);
  TEST_ASSERT_TRUE(decoded.securityProfileKnown);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RicohSecurityProfileId::Gr4Legacy),
                          decoded.securityProfile);
  TEST_ASSERT_TRUE(decoded.migratedLegacyGr4);
  TEST_ASSERT_TRUE(decoded.wifiCredentialsValid);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiCredentialSource::BleFixedHandles),
                        static_cast<int>(decoded.wifiSource));
}

void testNewProfileMetadataRoundTrips() {
  CameraProfileMetadata metadata;
  metadata.protocolGeneration = static_cast<uint8_t>(RicohProtocolGeneration::Gr3Family);
  metadata.protocolGenerationKnown = true;
  metadata.securityProfile = static_cast<uint8_t>(RicohSecurityProfileId::Gr3Passkey);
  metadata.securityProfileKnown = true;
  metadata.bleAuthenticated = true;
  metadata.capabilityVersion = CAMERA_CAPABILITY_SCHEMA_VERSION;
  metadata.wifiSource = WifiCredentialSource::BleUuidCharacteristics;
  metadata.wifiCredentialsValid = true;

  const StoredCameraProfileMetadata stored = encodeCameraProfileMetadata(metadata);
  const CameraProfileMetadata decoded = decodeCameraProfileMetadata(stored);
  TEST_ASSERT_TRUE(decoded.protocolGenerationKnown);
  TEST_ASSERT_EQUAL_UINT8(metadata.protocolGeneration, decoded.protocolGeneration);
  TEST_ASSERT_EQUAL_UINT8(metadata.securityProfile, decoded.securityProfile);
  TEST_ASSERT_TRUE(decoded.securityProfileKnown);
  TEST_ASSERT_TRUE(decoded.bleAuthenticated);
  TEST_ASSERT_EQUAL_UINT16(metadata.capabilityVersion, decoded.capabilityVersion);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(metadata.wifiSource), static_cast<int>(decoded.wifiSource));
  TEST_ASSERT_TRUE(decoded.wifiCredentialsValid);
}

void testLockedBindingRejectsNewPairingAndOtherCameras() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PairingRequestDecision::Allow),
                        static_cast<int>(pairingRequestDecision(CameraBindingState::Pairing)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PairingRequestDecision::RejectAndInvalidate),
                        static_cast<int>(pairingRequestDecision(CameraBindingState::Locked)));
  TEST_ASSERT_TRUE(bindingStateAllowsCandidate(CameraBindingState::Locked,
                                               "AA:BB:CC:DD:EE:FF",
                                               "aa:bb:cc:dd:ee:ff"));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::Locked,
                                                "AA:BB:CC:DD:EE:FF",
                                                "11:22:33:44:55:66"));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::BondInvalid,
                                                "AA:BB:CC:DD:EE:FF",
                                                "AA:BB:CC:DD:EE:FF"));
}

void testPairingBindingAcceptsFirstValidCandidateWithoutStoredIdentity() {
  TEST_ASSERT_TRUE(bindingStateAllowsCandidate(CameraBindingState::Unpaired,
                                               "",
                                               "F0:3E:05:26:44:57"));
  TEST_ASSERT_TRUE(bindingStateAllowsCandidate(CameraBindingState::Pairing,
                                               "",
                                               "F0:3E:05:26:44:57"));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::Pairing,
                                                "",
                                                ""));
  TEST_ASSERT_FALSE(bindingStateAllowsCandidate(CameraBindingState::Pairing,
                                                "",
                                                nullptr));
}

void testNormalizesResolvedPeerAddressTypes() {
  TEST_ASSERT_EQUAL_UINT8(0x00, normalizedPeerAddressType(0x00));
  TEST_ASSERT_EQUAL_UINT8(0x01, normalizedPeerAddressType(0x01));
  TEST_ASSERT_EQUAL_UINT8(0x00, normalizedPeerAddressType(0x02));
  TEST_ASSERT_EQUAL_UINT8(0x01, normalizedPeerAddressType(0x03));
}

void testPairingRecoveryCountsOnlyExplicitSecurityFailures() {
  PairingRecoveryPolicy policy;
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x208));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_FALSE(policy.onBondedSecurityFailure(0x213));
  TEST_ASSERT_TRUE(policy.onBondedSecurityFailure(0x213));
}

void testPairingLatencyPolicyAvoidsNestedUnbondedRetries() {
  TEST_ASSERT_EQUAL_UINT8(0, bleClientConnectRetries(false));
  TEST_ASSERT_EQUAL_UINT8(1, bleClientConnectRetries(true));
  TEST_ASSERT_EQUAL_UINT32(150, bleRetryDelayMs(true, 1000, 150));
  TEST_ASSERT_EQUAL_UINT32(1000, bleRetryDelayMs(false, 1000, 150));
}

void testPairingRecoveryDropsUnauthenticatedBondAfterTwoReads() {
  PairingRecoveryPolicy policy;
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x105));
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x101));
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x10F));
  TEST_ASSERT_TRUE(policy.onInsufficientAuthRead(0x108));
  policy.onAuthenticatedRead();
  TEST_ASSERT_FALSE(policy.onInsufficientAuthRead(0x105));
}

void testPasskeySerialCollectorCompletesWithoutLoggingValue() {
  PasskeyDigitCollector collector;
  int32_t code = -1;
  const char* input = "2x5 6\r4:45";
  for (const char* c = input; *c != '\0'; ++c) {
    const int32_t result = collector.feed(*c);
    if (result >= 0) {
      code = result;
    }
  }
  TEST_ASSERT_EQUAL_INT32(256445, code);
  TEST_ASSERT_EQUAL_INT32(-1, collector.feed('1'));
}

void testPasskeyButtonEntryCompletesResetsAndTimesOut() {
  PasskeyButtonEntry entry;
  entry.start(1000, 45000);
  for (int i = 0; i < 5; ++i) {
    entry.shortPress();
  }
  TEST_ASSERT_EQUAL_UINT8(5, entry.digits()[0]);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Editing),
                        static_cast<int>(entry.confirmDigit()));
  for (int i = 0; i < 11; ++i) {
    entry.shortPress();
  }
  TEST_ASSERT_EQUAL_UINT8(1, entry.digits()[1]);
  for (int i = 0; i < 5; ++i) {
    entry.confirmDigit();
  }
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Complete),
                        static_cast<int>(entry.status(2000)));
  TEST_ASSERT_EQUAL_INT32(510000, entry.code());

  entry.start(5000, 100);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Editing),
                        static_cast<int>(entry.status(5099)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::TimedOut),
                        static_cast<int>(entry.status(5100)));
  entry.reset();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Idle),
                        static_cast<int>(entry.status(6000)));
  TEST_ASSERT_EQUAL_INT32(0, entry.code());
}

void testPasskeyCanMoveWithBAndSubmitWithLongA() {
  PasskeyButtonEntry entry;
  entry.start(100, 1000);
  entry.shortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Editing),
                        static_cast<int>(entry.confirmDigit()));
  entry.shortPress();
  entry.shortPress();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(PasskeyEntryStatus::Complete),
                        static_cast<int>(entry.submit()));
  TEST_ASSERT_EQUAL_INT32(120000, entry.code());
  entry.reset();
  TEST_ASSERT_EQUAL_INT32(0, entry.code());
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testContainFitPreservesFourByThreeFrame);
  RUN_TEST(testContainFitPreservesWideFrame);
  RUN_TEST(testContainFitLetterboxesTallFrame);
  RUN_TEST(testContainFitRejectsInvalidDimensions);
  RUN_TEST(testCameraSleepAutoPowerOffWaitsForTimeout);
  RUN_TEST(testCameraSleepAutoPowerOffRequiresActiveSleep);
  RUN_TEST(testCameraSleepAutoPowerOffHandlesMillisWrap);
  RUN_TEST(testCameraSleepDisconnectReasonRejectsParkingFailure);
  RUN_TEST(testPortraitStartupReturnsAsSoonAsBleShutterIsReady);
  RUN_TEST(testPortraitBleReconnectReactivatesCameraWlan);
  RUN_TEST(testPortraitReconnectKeepsWlanReadyForLandscapeConnect);
  RUN_TEST(testLandscapeStartupOpensLiveViewWithoutPropsProbe);
  RUN_TEST(testPortraitToLandscapeResumesAfterCredentialCache);
  RUN_TEST(testLandscapeToPortraitDisconnectsWifiAndKeepsBleReady);
  RUN_TEST(testBleReadyFailureKeepsScheduledRetryPaced);
  RUN_TEST(testPortraitBleReadyDoesNotRetryOptionalWifiSetup);
  RUN_TEST(testDetectsProtocolOnlyFromSafeEvidence);
  RUN_TEST(testSecurityProfilesKeepGr4LegacyFrozen);
  RUN_TEST(testOnlyGr4DiscoveryCanReuseTheLegacyConnection);
  RUN_TEST(testProtocolRouterSelectsExactlyOneImplementation);
  RUN_TEST(testUnknownAndGr2ProfilesBlockBleSideEffects);
  RUN_TEST(testOperationModeSafetyIsGenerationSpecific);
  RUN_TEST(testHttpShutterIsOnlyClaimedWhereItWasVerified);
  RUN_TEST(testGr3CredentialShapeAllowsOptionalChannel);
  RUN_TEST(testOldGr4ProfileMetadataMigratesWithoutRepairing);
  RUN_TEST(testNewProfileMetadataRoundTrips);
  RUN_TEST(testNormalizesResolvedPeerAddressTypes);
  RUN_TEST(testPairingRecoveryCountsOnlyExplicitSecurityFailures);
  RUN_TEST(testPairingLatencyPolicyAvoidsNestedUnbondedRetries);
  RUN_TEST(testPairingRecoveryDropsUnauthenticatedBondAfterTwoReads);
  RUN_TEST(testPasskeySerialCollectorCompletesWithoutLoggingValue);
  RUN_TEST(testPasskeyButtonEntryCompletesResetsAndTimesOut);
  RUN_TEST(testPasskeyCanMoveWithBAndSubmitWithLongA);
  RUN_TEST(testLockedBindingRejectsNewPairingAndOtherCameras);
  RUN_TEST(testPairingBindingAcceptsFirstValidCandidateWithoutStoredIdentity);
  RUN_TEST(testCameraSleepGuardKeepsControllerOutOfScan);
  RUN_TEST(testRecoveryStopsWhenBleStackResetFails);
  RUN_TEST(testBeginRejectsInvalidInputs);
  RUN_TEST(testDeliversFrameSplitAcrossChunks);
  RUN_TEST(testDropsShortFrame);
  RUN_TEST(testDropsOverflowFrameWhenEoiArrives);
  RUN_TEST(testResetClearsPartialFrameOnly);
  RUN_TEST(testDerivesBleNameFromRicohWifiSsid);
  RUN_TEST(testLeavesNonNumericRicohWifiSsidUnchanged);
  RUN_TEST(testRejectsNonRicohWifiSsidForBleName);
  RUN_TEST(testRequiresBleAddressAndAddressTypeForDirectReconnect);
  RUN_TEST(testDirectReconnectIsOnlyUsedForKnownBondedBootProfile);
  RUN_TEST(testProtocolDiscoveryRefreshesOnlyRequiredCharacteristicServices);
  RUN_TEST(testNewPeerMustPersistBondBeforeConnectionSucceeds);
  RUN_TEST(testBleScanCleanupRequiresHostAndCallbackQuiescence);
  RUN_TEST(testBleStackObjectsClearOnlyAfterHostStops);
  RUN_TEST(testBleCandidateDiscoveryIsOpenWithoutStoredIdentity);
  RUN_TEST(testBleCandidateMustMatchStoredIdentity);
  RUN_TEST(testSupervisorWaitsForIntervalAndIgnoresHealthyPreview);
  RUN_TEST(testSupervisorReportsPreviewClosed);
  RUN_TEST(testSupervisorIgnoresCameraSleepGuard);
  RUN_TEST(testSupervisorReportsPreviewIdleTimeout);
  RUN_TEST(testSupervisorReportsFrameStallDespiteIncomingBytes);
  RUN_TEST(testUiMapsAppStatesToScenes);
  RUN_TEST(testPortraitBleReadyShowsRemoteInsteadOfMergedConnectionDot);
  RUN_TEST(testUiPropagatesDeviceChargingIndicator);
  RUN_TEST(testConnectingDotsOnlyMergeAfterBleConnects);
  RUN_TEST(testUiScenePriority);
  RUN_TEST(testInitialCameraSelectionSuppressesResetVisual);
  RUN_TEST(testOrientationRequiresStableCandidate);
  RUN_TEST(testOrientationMapsStickS3PhysicalAxes);
  RUN_TEST(testOrientationHysteresisPreventsBoundaryChatter);
  RUN_TEST(testLiveViewLockForcesLandscapeAndUnlockRestoresPosture);
  RUN_TEST(testActivePreviewOutlivesPortraitUiForLockHandoff);
  RUN_TEST(testAnimationProgressAndCompletion);
  RUN_TEST(testAnimationElapsedIsMillisWrapSafe);
  RUN_TEST(testButtonBReportsContinuousProgress);
  RUN_TEST(testButtonBSingleClickTogglesMirrorAfterDoubleClickWindow);
  RUN_TEST(testButtonBDoubleClickTogglesLiveViewLockWithoutMirror);
  RUN_TEST(testButtonBThresholdTriggersOnlyOnce);
  RUN_TEST(testButtonAOperationTriggersAtMostOneShoot);
  RUN_TEST(testShutterOverlaySuccessAndFailureLifecycles);
  RUN_TEST(testSleepSceneOverridesOrientationScene);
  RUN_TEST(testErrorSceneOverridesEveryOrdinaryScene);
  return UNITY_END();
}
