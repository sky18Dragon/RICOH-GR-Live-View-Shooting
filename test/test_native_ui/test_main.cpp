#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "app/AppController.h"
#include "charging_indicator_policy.h"
#include "ui/OrientationTracker.h"
#include "ui/PairingGuide.h"
#include "ui/UiCoordinator.h"

namespace {

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

void testPairingGuideOverridesConnectionScene() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::BleScan;
  snapshot.pairingGuideActive = true;
  snapshot.pairingGuideGr2Selected = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::PairingGuide),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape)));

  rvf::UiCoordinator coordinator;
  coordinator.begin(0);
  rvf::ButtonEvents input;
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 1);
  TEST_ASSERT_TRUE(coordinator.viewModel().pairingGuideGr2Selected);
  TEST_ASSERT_FALSE(coordinator.viewModel().pairingGuideGr4Selected);
}

void testWifiProvisioningOverridesPairingAndPropagatesPortalDetails() {
  rvf::UiSnapshot snapshot;
  snapshot.appState = rvf::AppState::WifiCredentialsReady;
  snapshot.pairingGuideActive = true;
  snapshot.wifiProvisioningActive = true;
  snapshot.provisioningSsid = "GR-II-Setup-1234";
  snapshot.provisioningPassword = "GR288888";
  snapshot.provisioningUrl = "http://192.168.4.1";
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiScene::WifiProvisioning),
                        static_cast<int>(rvf::UiCoordinator::selectScene(
                            snapshot, rvf::UiOrientation::Landscape)));

  rvf::UiCoordinator coordinator;
  rvf::ButtonEvents input;
  coordinator.begin(0);
  coordinator.update(snapshot, input, rvf::UiOrientation::Landscape, 1);
  TEST_ASSERT_EQUAL_STRING("GR-II-Setup-1234", coordinator.viewModel().provisioningSsid);
  TEST_ASSERT_EQUAL_STRING("GR288888", coordinator.viewModel().provisioningPassword);
  TEST_ASSERT_EQUAL_STRING("http://192.168.4.1", coordinator.viewModel().provisioningUrl);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UiOrientation::Portrait),
                        static_cast<int>(coordinator.viewModel().orientation));

  snapshot.wifiProvisioningPreparing = true;
  coordinator.update(snapshot, input, rvf::UiOrientation::Portrait, 2);
  TEST_ASSERT_TRUE(coordinator.viewModel().wifiProvisioningPreparing);
}

void testPairingGuideUsesBToSelectAndAToConfirm() {
  rvf::PairingGuide guide;
  guide.activate();
  TEST_ASSERT_TRUE(guide.active());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(guide.selection()));

  rvf::ButtonEvents select;
  select.buttonBClicked = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::PairingGuideAction::SelectionChanged),
                        static_cast<int>(guide.handle(select)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr4Family),
                        static_cast<int>(guide.selection()));

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::PairingGuideAction::SelectionChanged),
                        static_cast<int>(guide.handle(select)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr2Family),
                        static_cast<int>(guide.selection()));

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::PairingGuideAction::SelectionChanged),
                        static_cast<int>(guide.handle(select)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(guide.selection()));

  rvf::ButtonEvents confirm;
  confirm.buttonADown = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::PairingGuideAction::Confirmed),
                        static_cast<int>(guide.handle(confirm)));
  TEST_ASSERT_FALSE(guide.active());
}

void testPairingGuideConsumesLongBWithoutResettingOrSelecting() {
  rvf::PairingGuide guide;
  guide.activate();
  rvf::ButtonEvents longB;
  longB.resetPairing = true;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::PairingGuideAction::None),
                        static_cast<int>(guide.handle(longB)));
  TEST_ASSERT_TRUE(guide.active());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RicohProtocolGeneration::Gr3Family),
                        static_cast<int>(guide.selection()));
}

void testPairingSelectionRejectsTheOtherProtocolGeneration() {
  TEST_ASSERT_TRUE(ricohProtocolMatchesExpectedGeneration(
      RicohProtocolGeneration::Unknown, RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_TRUE(ricohProtocolMatchesExpectedGeneration(
      RicohProtocolGeneration::Gr3Family, RicohProtocolGeneration::Gr3Family));
  TEST_ASSERT_FALSE(ricohProtocolMatchesExpectedGeneration(
      RicohProtocolGeneration::Gr3Family, RicohProtocolGeneration::Gr4Family));
  TEST_ASSERT_FALSE(ricohProtocolMatchesExpectedGeneration(
      RicohProtocolGeneration::Gr4Family, RicohProtocolGeneration::Gr3Family));
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
  rvf::ButtonInput input(3000, 350, 450);
  input.reset();
  const rvf::ButtonEvents pressed = input.update(false, true, false, 100);
  TEST_ASSERT_FALSE(pressed.resetHoldActive);
  const rvf::ButtonEvents beforeDelay = input.update(false, true, false, 549);
  TEST_ASSERT_FALSE(beforeDelay.resetHoldActive);
  const rvf::ButtonEvents atDelay = input.update(false, true, false, 550);
  TEST_ASSERT_TRUE(atDelay.resetHoldActive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, atDelay.resetHoldProgress);
  const rvf::ButtonEvents halfway = input.update(false, true, false, 1600);
  TEST_ASSERT_TRUE(halfway.resetHoldActive);
  TEST_ASSERT_EQUAL_UINT32(1500, halfway.resetHoldMs);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1050.0f / 2550.0f, halfway.resetHoldProgress);
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
  TEST_ASSERT_TRUE(confirmed.buttonBClicked);
  TEST_ASSERT_FALSE(confirmed.buttonBDoubleClicked);
  TEST_ASSERT_TRUE(confirmed.toggleDisplayMirror);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::UserCommand::ToggleDisplayMirror),
                        static_cast<int>(rvf::ButtonInput::commandFromEvents(confirmed)));
}

void testButtonBDoubleClickTogglesLiveViewLockWithoutMirror() {
  rvf::ButtonInput input(3000, 350);
  const rvf::ButtonEvents firstPress = input.update(false, true, false, 100);
  TEST_ASSERT_FALSE(firstPress.resetHoldActive);
  const rvf::ButtonEvents firstRelease = input.update(false, false, false, 160);
  TEST_ASSERT_FALSE(firstRelease.toggleDisplayMirror);
  TEST_ASSERT_FALSE(firstRelease.toggleLiveViewLock);

  const rvf::ButtonEvents secondPress = input.update(false, true, false, 260);
  TEST_ASSERT_FALSE(secondPress.resetHoldActive);
  const rvf::ButtonEvents secondRelease = input.update(false, false, false, 320);
  TEST_ASSERT_TRUE(secondRelease.buttonBDoubleClicked);
  TEST_ASSERT_FALSE(secondRelease.buttonBClicked);
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

void testChargingIndicatorTurnsOnImmediately() {
  ChargingIndicatorFilter filter(12000);
  TEST_ASSERT_FALSE(filter.charging());
  TEST_ASSERT_FALSE(filter.update(false, 1000));  // unplugged stays off, no change
  TEST_ASSERT_TRUE(filter.update(true, 2000));
  TEST_ASSERT_TRUE(filter.charging());
  TEST_ASSERT_FALSE(filter.update(true, 4000));   // no repeated change report
}

void testChargingIndicatorIgnoresTopOffFlapping() {
  // Raw flag alternating every ~3 s near full charge, as seen on a StickS3.
  ChargingIndicatorFilter filter(12000);
  TEST_ASSERT_TRUE(filter.update(true, 0));
  uint32_t now = 0;
  for (int i = 0; i < 20; ++i) {
    now += 3000;
    TEST_ASSERT_FALSE(filter.update((i % 2) == 1, now));
    TEST_ASSERT_TRUE(filter.charging());
  }
}

void testChargingIndicatorClearsAfterSustainedOff() {
  ChargingIndicatorFilter filter(12000);
  TEST_ASSERT_TRUE(filter.update(true, 0x00000000U));
  TEST_ASSERT_FALSE(filter.update(false, 1000));  // off starts
  TEST_ASSERT_FALSE(filter.update(false, 12999));
  TEST_ASSERT_TRUE(filter.charging());
  TEST_ASSERT_TRUE(filter.update(false, 13000));
  TEST_ASSERT_FALSE(filter.charging());
  // millis() wrap inside the hold.
  ChargingIndicatorFilter wrap(12000);
  TEST_ASSERT_TRUE(wrap.update(true, 0xFFFFE000U));
  TEST_ASSERT_FALSE(wrap.update(false, 0xFFFFF000U));
  TEST_ASSERT_FALSE(wrap.update(false, 0x00001000U));   // 8 s into the hold
  TEST_ASSERT_TRUE(wrap.update(false, 0x00002000U));    // 12 s
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testChargingIndicatorTurnsOnImmediately);
  RUN_TEST(testChargingIndicatorIgnoresTopOffFlapping);
  RUN_TEST(testChargingIndicatorClearsAfterSustainedOff);
  RUN_TEST(testUiMapsAppStatesToScenes);
  RUN_TEST(testUiPropagatesDeviceChargingIndicator);
  RUN_TEST(testConnectingDotsOnlyMergeAfterBleConnects);
  RUN_TEST(testUiScenePriority);
  RUN_TEST(testPairingGuideOverridesConnectionScene);
  RUN_TEST(testWifiProvisioningOverridesPairingAndPropagatesPortalDetails);
  RUN_TEST(testPairingGuideUsesBToSelectAndAToConfirm);
  RUN_TEST(testPairingGuideConsumesLongBWithoutResettingOrSelecting);
  RUN_TEST(testPairingSelectionRejectsTheOtherProtocolGeneration);
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
