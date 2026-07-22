#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "ble_reconnect_policy.h"

#include "camera_identity.h"
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
  TEST_ASSERT_FALSE(tracker.update(1.0f, 0.0f, 0.0f, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Portrait),
                        static_cast<int>(tracker.orientation()));
  TEST_ASSERT_FALSE(tracker.update(1.0f, 0.0f, 0.0f, 599));
  TEST_ASSERT_TRUE(tracker.update(1.0f, 0.0f, 0.0f, 600));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Landscape),
                        static_cast<int>(tracker.orientation()));
}

void testOrientationHysteresisPreventsBoundaryChatter() {
  rvf::OrientationTracker tracker(rvf::UiOrientation::Portrait);
  tracker.reset(rvf::UiOrientation::Portrait, 0);
  tracker.update(1.0f, 0.0f, 0.0f, 100);
  tracker.update(1.0f, 0.0f, 0.0f, 600);
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

}  // namespace

int main() {
  UNITY_BEGIN();
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
