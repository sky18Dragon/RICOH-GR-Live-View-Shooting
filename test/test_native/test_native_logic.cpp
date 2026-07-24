#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "ble_pairing_policy.h"
#include "ble_reconnect_policy.h"

#include "app/AppController.h"
#include "camera_identity.h"
#include "camera_profile_schema.h"
#include "camera_protocol_profile.h"
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

struct FlowHarness {
  static bool bleConnected;
  static bool wifiConnected;
  static bool previewRunning;
  static bool cachedCredentials;
  static uint32_t lastRecoveryAt;
  static uint32_t activateCalls;
  static uint32_t readCredentialsCalls;
  static uint32_t applyCredentialsCalls;
  static uint32_t connectCalls;
  static uint32_t disconnectCalls;
  static uint32_t fetchPropsCalls;
  static uint32_t openPreviewCalls;

  static void reset() {
    bleConnected = true;
    wifiConnected = false;
    previewRunning = false;
    cachedCredentials = false;
    lastRecoveryAt = 0;
    activateCalls = 0;
    readCredentialsCalls = 0;
    applyCredentialsCalls = 0;
    connectCalls = 0;
    disconnectCalls = 0;
    fetchPropsCalls = 0;
    openPreviewCalls = 0;
  }

  static bool noGuard(const char*) { return false; }
  static bool guardInactive() { return false; }
  static bool isBleConnected() { return bleConnected; }
  static bool isWifiConnected() { return wifiConnected; }
  static bool runBleDiscovery() { return bleConnected; }
  static bool activateWifi() {
    ++activateCalls;
    return true;
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
  static bool fetchProps() {
    ++fetchPropsCalls;
    return wifiConnected;
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
    result.cameraSleepGuardBlocksFlow = noGuard;
    result.cameraSleepGuardActive = guardInactive;
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
    result.fetchCameraProps = fetchProps;
    result.openLiveView = openPreview;
    result.previewStreamRunning = isPreviewRunning;
    result.cameraRecoveryInProgress = recoveryInactive;
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
uint32_t FlowHarness::lastRecoveryAt = 0;
uint32_t FlowHarness::activateCalls = 0;
uint32_t FlowHarness::readCredentialsCalls = 0;
uint32_t FlowHarness::applyCredentialsCalls = 0;
uint32_t FlowHarness::connectCalls = 0;
uint32_t FlowHarness::disconnectCalls = 0;
uint32_t FlowHarness::fetchPropsCalls = 0;
uint32_t FlowHarness::openPreviewCalls = 0;

void testPortraitStartupCachesWifiWithoutConnecting() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::readCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::applyCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::fetchPropsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::openPreviewCalls);
}

void testLandscapeStartupRunsOriginalFullFlow() {
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
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::fetchPropsCalls);
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

void testButtonBReleaseBeforeThresholdDoesNotReset() {
  rvf::ButtonInput input(3000);
  input.update(false, true, false, 100);
  const rvf::ButtonEvents released = input.update(false, false, false, 2500);
  TEST_ASSERT_FALSE(released.resetPairing);
  TEST_ASSERT_FALSE(released.resetHoldActive);
}

void testButtonBThresholdTriggersOnlyOnce() {
  rvf::ButtonInput input(3000);
  input.update(false, true, false, 100);
  TEST_ASSERT_TRUE(input.update(false, true, false, 3100).resetPairing);
  TEST_ASSERT_FALSE(input.update(false, true, false, 4100).resetPairing);
  TEST_ASSERT_FALSE(input.update(false, true, false, 5100).resetPairing);
}

void testButtonAOperationTriggersAtMostOneShoot() {
  rvf::ButtonInput input;
  const rvf::ButtonEvents pressed = input.update(true, false, false, 100);
  TEST_ASSERT_TRUE(pressed.buttonADown);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::None),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(pressed)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::None),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(
                            input.update(true, false, false, 500))));
  const rvf::ButtonEvents released = input.update(false, false, false, 600);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::Shoot),
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
  gr3.hasGr3WlanService = true;
  gr3.hasGr3NetworkTypeCharacteristic = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(detectRicohProtocol(gr3)));

  ProtocolDetectionEvidence incompleteGr3;
  incompleteGr3.hasGr3WlanService = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Unknown),
                        static_cast<int>(detectRicohProtocol(incompleteGr3)));

  ProtocolDetectionEvidence gr4;
  gr4.gr4PowerHandleReadSucceeded = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(detectRicohProtocol(gr4)));
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
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::BleStartup, true));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr3, RicohCameraOperationMode::Capture, false));

  const CameraProtocolProfile& gr4 = cameraProtocolProfile(RicohProtocolGeneration::Gr4Family);
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Playback, true));
  TEST_ASSERT_TRUE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::Unknown, false));
  TEST_ASSERT_FALSE(operationModeAllowsWifi(gr4, RicohCameraOperationMode::PowerOffTransfer, true));
}

void testGr3CredentialShapeAllowsOptionalChannel() {
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 0));
  TEST_ASSERT_TRUE(validGr3WifiCredentials("GR_TEST", "secret", 11));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "secret", 12));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("", "secret", 1));
  TEST_ASSERT_FALSE(validGr3WifiCredentials("GR_TEST", "", 1));
}

void testOldProfileMetadataUpgradesWithoutAssumingProtocol() {
  StoredCameraProfileMetadata old;
  old.schemaVersion = 3;
  old.legacyWifiValid = true;
  const CameraProfileMetadata decoded = decodeCameraProfileMetadata(old);
  TEST_ASSERT_EQUAL_UINT32(CAMERA_PROFILE_SCHEMA_VERSION, decoded.schemaVersion);
  TEST_ASSERT_FALSE(decoded.protocolGenerationKnown);
  TEST_ASSERT_TRUE(decoded.wifiCredentialsValid);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WifiCredentialSource::Unknown),
                        static_cast<int>(decoded.wifiSource));
}

void testNewProfileMetadataRoundTrips() {
  CameraProfileMetadata metadata;
  metadata.protocolGeneration = static_cast<uint8_t>(RicohProtocolGeneration::Gr3Family);
  metadata.protocolGenerationKnown = true;
  metadata.capabilityVersion = CAMERA_CAPABILITY_SCHEMA_VERSION;
  metadata.wifiSource = WifiCredentialSource::BleUuidCharacteristics;
  metadata.wifiCredentialsValid = true;

  const StoredCameraProfileMetadata stored = encodeCameraProfileMetadata(metadata);
  const CameraProfileMetadata decoded = decodeCameraProfileMetadata(stored);
  TEST_ASSERT_TRUE(decoded.protocolGenerationKnown);
  TEST_ASSERT_EQUAL_UINT8(metadata.protocolGeneration, decoded.protocolGeneration);
  TEST_ASSERT_EQUAL_UINT16(metadata.capabilityVersion, decoded.capabilityVersion);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(metadata.wifiSource), static_cast<int>(decoded.wifiSource));
  TEST_ASSERT_TRUE(decoded.wifiCredentialsValid);
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

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testPortraitStartupCachesWifiWithoutConnecting);
  RUN_TEST(testLandscapeStartupRunsOriginalFullFlow);
  RUN_TEST(testPortraitToLandscapeResumesAfterCredentialCache);
  RUN_TEST(testLandscapeToPortraitDisconnectsWifiAndKeepsBleReady);
  RUN_TEST(testDetectsProtocolOnlyFromSafeEvidence);
  RUN_TEST(testUnknownAndGr2ProfilesBlockBleSideEffects);
  RUN_TEST(testOperationModeSafetyIsGenerationSpecific);
  RUN_TEST(testGr3CredentialShapeAllowsOptionalChannel);
  RUN_TEST(testOldProfileMetadataUpgradesWithoutAssumingProtocol);
  RUN_TEST(testNewProfileMetadataRoundTrips);
  RUN_TEST(testNormalizesResolvedPeerAddressTypes);
  RUN_TEST(testPairingRecoveryCountsOnlyExplicitSecurityFailures);
  RUN_TEST(testPairingRecoveryDropsUnauthenticatedBondAfterTwoReads);
  RUN_TEST(testPasskeySerialCollectorCompletesWithoutLoggingValue);
  RUN_TEST(testPasskeyButtonEntryCompletesResetsAndTimesOut);
  RUN_TEST(testBeginRejectsInvalidInputs);
  RUN_TEST(testDeliversFrameSplitAcrossChunks);
  RUN_TEST(testDropsShortFrame);
  RUN_TEST(testDropsOverflowFrameWhenEoiArrives);
  RUN_TEST(testResetClearsPartialFrameOnly);
  RUN_TEST(testDerivesBleNameFromRicohWifiSsid);
  RUN_TEST(testLeavesNonNumericRicohWifiSsidUnchanged);
  RUN_TEST(testRejectsNonRicohWifiSsidForBleName);
  RUN_TEST(testRequiresBleAddressAndAddressTypeForDirectReconnect);
  RUN_TEST(testBleCandidateDiscoveryIsOpenWithoutStoredIdentity);
  RUN_TEST(testBleCandidateMustMatchStoredIdentity);
  RUN_TEST(testSupervisorWaitsForIntervalAndIgnoresHealthyPreview);
  RUN_TEST(testSupervisorReportsPreviewClosed);
  RUN_TEST(testSupervisorIgnoresCameraSleepGuard);
  RUN_TEST(testSupervisorReportsPreviewIdleTimeout);
  RUN_TEST(testSupervisorReportsFrameStallDespiteIncomingBytes);
  RUN_TEST(testUiMapsAppStatesToScenes);
  RUN_TEST(testUiScenePriority);
  RUN_TEST(testOrientationRequiresStableCandidate);
  RUN_TEST(testOrientationMapsStickS3PhysicalAxes);
  RUN_TEST(testOrientationHysteresisPreventsBoundaryChatter);
  RUN_TEST(testAnimationProgressAndCompletion);
  RUN_TEST(testAnimationElapsedIsMillisWrapSafe);
  RUN_TEST(testButtonBReportsContinuousProgress);
  RUN_TEST(testButtonBReleaseBeforeThresholdDoesNotReset);
  RUN_TEST(testButtonBThresholdTriggersOnlyOnce);
  RUN_TEST(testButtonAOperationTriggersAtMostOneShoot);
  RUN_TEST(testShutterOverlaySuccessAndFailureLifecycles);
  RUN_TEST(testSleepSceneOverridesOrientationScene);
  RUN_TEST(testErrorSceneOverridesEveryOrdinaryScene);
  return UNITY_END();
}
