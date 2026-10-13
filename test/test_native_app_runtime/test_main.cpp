#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "SimCameraDriver.h"
#include "camera/CameraRegistry.h"

namespace {

using rvf::AppRuntime;
using rvf::ButtonEvents;
using rvf::FakeAppPlatform;
using rvf::SimCameraDriver;
using rvf::UiOrientation;

std::string join(const std::vector<std::string>& calls) {
  std::string out;
  for (const std::string& call : calls) {
    if (!out.empty()) out += " > ";
    out += call;
  }
  return out;
}

void expectCalls(const std::vector<std::string>& calls, const char* expected) {
  TEST_ASSERT_EQUAL_STRING(expected, join(calls).c_str());
}

struct Fixture {
  std::vector<std::string> calls;
  SimCameraDriver camera{calls};
  FakeAppPlatform platform{calls};
  AppRuntime runtime{camera, platform};
};

void testTickRunsTheLoopInOrder() {
  Fixture f;
  f.runtime.tick();
  // Supervisor sits between the flow and background ticks, as in the
  // original loop, so it sees the flow's result before Wi-Fi cache work.
  expectCalls(f.calls,
              "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > "
              "platform.refreshUi(input) > cam.serviceFlowTick > platform.superviseSystem > "
              "cam.serviceBackgroundTick > platform.serviceDevicePower > platform.refreshUi > "
              "platform.idle");
}

void testTickSkipsButtonsWhenTheDriverIsBusy() {
  Fixture f;
  f.camera.beginTickResult = false;
  f.runtime.tick();
  expectCalls(f.calls,
              "cam.beginTick > cam.serviceFlowTick > platform.superviseSystem > "
              "cam.serviceBackgroundTick > platform.serviceDevicePower > platform.refreshUi > "
              "platform.idle");
}

void testPowerButtonShutsDownBeforeAnyCameraWork() {
  Fixture f;
  f.platform.nextButtons.powerOff = true;
  f.runtime.tick();
  // powerOff short-circuits the PMIC hold poll, matching the original `||`.
  // On hardware shutdown() never returns; the fake does, so the loop pass
  // continues and is not part of this assertion.
  TEST_ASSERT_EQUAL_STRING("cam.beginTick > platform.pollButtons > platform.shutdown",
                           join({f.calls.begin(), f.calls.begin() + 3}).c_str());

  Fixture g;
  g.platform.powerButtonHoldResult = true;
  g.runtime.tick();
  TEST_ASSERT_EQUAL_STRING(
      "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > platform.shutdown",
      join({g.calls.begin(), g.calls.begin() + 4}).c_str());
}

void testAutoPowerOffEndsThePassWithoutCameraFlow() {
  Fixture f;
  f.camera.autoPowerOffDueResult = true;
  f.runtime.tick();
  expectCalls(f.calls,
              "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > "
              "platform.refreshUi(input) > platform.shutdown");
  TEST_ASSERT_EQUAL_STRING("Power: CameraSleep idle timeout reached\n", f.platform.logText.c_str());
}

void testShootGoesToTheDriver() {
  Fixture f;
  f.platform.nextButtons.buttonADown = true;
  f.runtime.tick();
  TEST_ASSERT_EQUAL_STRING(
      "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > cam.shoot > "
      "platform.refreshUi(input)",
      join({f.calls.begin(), f.calls.begin() + 5}).c_str());
  TEST_ASSERT_TRUE(f.platform.lastRefreshInput.buttonADown);
}

void testResetPairingReleasesTheLockAndSkipsTheRefresh() {
  Fixture f;
  f.camera.previewActiveResult = true;
  f.platform.nextButtons.toggleLiveViewLock = true;
  f.runtime.tick();
  TEST_ASSERT_TRUE(f.runtime.liveViewLocked());

  f.calls.clear();
  f.platform.nextButtons.resetPairing = true;
  f.runtime.tick();
  TEST_ASSERT_FALSE(f.runtime.liveViewLocked());
  TEST_ASSERT_EQUAL_STRING(
      "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > cam.resetPairing > "
      "cam.serviceFlowTick",
      join({f.calls.begin(), f.calls.begin() + 5}).c_str());
}

void testLiveViewLockOnlyEngagesDuringPreview() {
  Fixture f;
  f.platform.nextButtons.toggleLiveViewLock = true;
  f.runtime.tick();
  TEST_ASSERT_FALSE(f.runtime.liveViewLocked());
  TEST_ASSERT_EQUAL_STRING("LiveView lock: ignored outside LiveView\n", f.platform.logText.c_str());

  f.platform.logText.clear();
  f.camera.previewActiveResult = true;
  f.platform.nextButtons.toggleLiveViewLock = true;
  f.runtime.tick();
  TEST_ASSERT_TRUE(f.runtime.liveViewLocked());
  TEST_ASSERT_EQUAL_STRING("LiveView lock: enabled=1\n", f.platform.logText.c_str());

  // Unlocking is allowed even after the preview ended.
  f.platform.logText.clear();
  f.camera.previewActiveResult = false;
  f.platform.nextButtons.toggleLiveViewLock = true;
  f.runtime.tick();
  TEST_ASSERT_FALSE(f.runtime.liveViewLocked());
  TEST_ASSERT_EQUAL_STRING("LiveView lock: enabled=0\n", f.platform.logText.c_str());
}

void testMirrorToggleIsAPlatformAction() {
  Fixture f;
  f.platform.nextButtons.toggleDisplayMirror = true;
  f.runtime.tick();
  TEST_ASSERT_EQUAL_STRING(
      "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > "
      "platform.toggleDisplayMirror > platform.refreshUi(input)",
      join({f.calls.begin(), f.calls.begin() + 5}).c_str());
}

void testPreviewIntentFollowsOrientationLockAndGuide() {
  Fixture f;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(UiOrientation::Portrait),
                        static_cast<int>(f.runtime.applyPreviewOrientation(UiOrientation::Portrait)));
  TEST_ASSERT_FALSE(f.camera.lastPreviewLandscape);

  f.runtime.applyPreviewOrientation(UiOrientation::Landscape);
  TEST_ASSERT_TRUE(f.camera.lastPreviewLandscape);
  TEST_ASSERT_FALSE(f.camera.lastPreviewModelSelection);

  // The LiveView lock keeps the preview landscape while the device is upright.
  f.camera.previewActiveResult = true;
  f.platform.nextButtons.toggleLiveViewLock = true;
  f.runtime.tick();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(UiOrientation::Landscape),
                        static_cast<int>(f.runtime.applyPreviewOrientation(UiOrientation::Portrait)));
  TEST_ASSERT_TRUE(f.camera.lastPreviewLandscape);

  // The driver hears that the model picker is up, so it can hold the preview.
  f.runtime.requestModelSelection(rvf::cameraModelAt(0).id);
  f.runtime.applyPreviewOrientation(UiOrientation::Landscape);
  TEST_ASSERT_TRUE(f.camera.lastPreviewModelSelection);
}

void testModelPickerConsumesInputAndConfirmsTheSelection() {
  Fixture f;
  f.runtime.requestModelSelection(rvf::cameraModelAt(rvf::defaultCameraModelIndex()).id);

  // Guide input never reaches shutter, mirror or reset; it only redraws.
  f.platform.nextButtons.buttonBClicked = true;
  f.runtime.tick();
  expectCalls(f.calls,
              "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > "
              "platform.refreshUi(input) > platform.serviceDevicePower > platform.refreshUi > "
              "platform.idle");
  TEST_ASSERT_FALSE(f.platform.lastRefreshInput.buttonBClicked);
  const size_t expected = (rvf::defaultCameraModelIndex() + 1) % rvf::cameraModelCount();
  TEST_ASSERT_EQUAL_UINT32(expected, f.runtime.pairingGuide().selectedIndex());

  f.calls.clear();
  f.platform.nextButtons.buttonADown = true;
  f.runtime.tick();
  TEST_ASSERT_FALSE(f.runtime.pairingGuide().active());
  TEST_ASSERT_EQUAL_UINT8(rvf::cameraModelAt(expected).id, f.camera.lastConfirmedModel);
  TEST_ASSERT_EQUAL_STRING(
      "cam.beginTick > platform.pollButtons > platform.pollPowerButtonHold > cam.confirmModel > "
      "cam.serviceFlowTick",
      join({f.calls.begin(), f.calls.begin() + 5}).c_str());
}

void testStartupShowsThePickerWhenNoCameraIsConfigured() {
  Fixture f;
  f.runtime.startCamera();
  expectCalls(f.calls, "cam.runStartupFlow");
  TEST_ASSERT_FALSE(f.camera.lastStartupModelSelection);

  Fixture g;
  g.runtime.requestModelSelection(rvf::cameraModelAt(0).id);
  g.runtime.startCamera();
  expectCalls(g.calls, "cam.runStartupFlow > cam.onModelSelectionShown > platform.refreshUi");
  TEST_ASSERT_TRUE(g.camera.lastStartupModelSelection);
}

void testProvisioningPassOwnsTheLoop() {
  Fixture f;
  f.camera.provisioningActiveResult = true;
  f.runtime.tick();
  expectCalls(f.calls,
              "cam.serviceProvisioningPortal > platform.pollButtons > "
              "platform.pollPowerButtonHold > cam.handleProvisioningInput > "
              "platform.serviceDevicePower > platform.refreshUi(input) > platform.idle");

  Fixture g;
  g.camera.provisioningActiveResult = true;
  g.camera.handleProvisioningInputResult = true;
  g.platform.nextButtons.resetPairing = true;
  g.runtime.tick();
  TEST_ASSERT_TRUE(g.camera.lastProvisioningReset);
  expectCalls(g.calls,
              "cam.serviceProvisioningPortal > platform.pollButtons > "
              "platform.pollPowerButtonHold > cam.handleProvisioningInput > "
              "platform.refreshUi(input)");
  TEST_ASSERT_FALSE(g.platform.lastRefreshInput.resetPairing);

  Fixture h;
  h.camera.provisioningActiveResult = true;
  h.platform.powerButtonHoldResult = true;
  h.runtime.tick();
  expectCalls(h.calls,
              "cam.serviceProvisioningPortal > platform.pollButtons > "
              "platform.pollPowerButtonHold > cam.stopProvisioning > platform.shutdown");
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testTickRunsTheLoopInOrder);
  RUN_TEST(testTickSkipsButtonsWhenTheDriverIsBusy);
  RUN_TEST(testPowerButtonShutsDownBeforeAnyCameraWork);
  RUN_TEST(testAutoPowerOffEndsThePassWithoutCameraFlow);
  RUN_TEST(testShootGoesToTheDriver);
  RUN_TEST(testResetPairingReleasesTheLockAndSkipsTheRefresh);
  RUN_TEST(testLiveViewLockOnlyEngagesDuringPreview);
  RUN_TEST(testMirrorToggleIsAPlatformAction);
  RUN_TEST(testPreviewIntentFollowsOrientationLockAndGuide);
  RUN_TEST(testModelPickerConsumesInputAndConfirmsTheSelection);
  RUN_TEST(testStartupShowsThePickerWhenNoCameraIsConfigured);
  RUN_TEST(testProvisioningPassOwnsTheLoop);
  return UNITY_END();
}
