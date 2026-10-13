#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

#include "app/AppController.h"
#include "supervisor/SystemSupervisor.h"

namespace {

struct FlowHarness {
  static bool bleConnected;
  static bool wifiConnected;
  static bool previewRunning;
  static bool cachedCredentials;
  static bool guardActive;
  static bool resetStackSucceeds;
  static bool deactivateSucceeds;
  static bool reactivateSucceeds;
  static bool powerReadySucceeds;
  static uint32_t lastRecoveryAt;
  static uint32_t activateCalls;
  static uint32_t ensurePowerReadyCalls;
  static uint32_t finishWlanCalls;
  static bool finishWlanSawConnectedSta;
  static uint32_t deactivateCalls;
  static uint32_t reactivateCalls;
  static uint32_t readCredentialsCalls;
  static uint32_t applyCredentialsCalls;
  static uint32_t connectCalls;
  static uint32_t disconnectCalls;
  static uint32_t openPreviewCalls;
  static uint32_t runBleDiscoveryCalls;
  static uint32_t resetStackCalls;
  static uint32_t restoreReleasedBleCalls;
  static bool rescanQueriedBeforeRestore;
  static std::string lastRescanReason;
  static bool shutterReadyValue;
  static bool shutterOverWifiValue;
  static bool shootSucceeds;
  static bool previewKeptAfterFailure;
  static uint32_t reopenPreviewCalls;
  static bool reopenPreviewSucceeds;
  static uint32_t recycleStallCalls;
  static bool recycleStallSucceeds;

  static void reset() {
    bleConnected = true;
    wifiConnected = false;
    previewRunning = false;
    cachedCredentials = false;
    guardActive = false;
    resetStackSucceeds = true;
    deactivateSucceeds = true;
    reactivateSucceeds = true;
    powerReadySucceeds = true;
    lastRecoveryAt = 0;
    activateCalls = 0;
    ensurePowerReadyCalls = 0;
    finishWlanCalls = 0;
    finishWlanSawConnectedSta = false;
    deactivateCalls = 0;
    reactivateCalls = 0;
    readCredentialsCalls = 0;
    applyCredentialsCalls = 0;
    connectCalls = 0;
    disconnectCalls = 0;
    openPreviewCalls = 0;
    runBleDiscoveryCalls = 0;
    resetStackCalls = 0;
    restoreReleasedBleCalls = 0;
    rescanQueriedBeforeRestore = false;
    lastRescanReason.clear();
    shutterReadyValue = true;
    shutterOverWifiValue = false;
    shootSucceeds = true;
    previewKeptAfterFailure = false;
    reopenPreviewCalls = 0;
    reopenPreviewSucceeds = true;
    recycleStallCalls = 0;
    recycleStallSucceeds = true;
  }

  static void restoreReleasedBle(const char*) { ++restoreReleasedBleCalls; }
  static bool recordRescanReason(const char* reason) {
    if (restoreReleasedBleCalls == 0) {
      rescanQueriedBeforeRestore = true;
    }
    lastRescanReason = reason != nullptr ? reason : "";
    return false;
  }
  static bool shutterReady() { return shutterReadyValue; }
  static bool shutterOverWifi() { return shutterOverWifiValue; }
  static bool shoot() { return shootSucceeds; }
  static bool keptPreview() { return previewKeptAfterFailure; }
  static bool reopenPreview() {
    ++reopenPreviewCalls;
    if (reopenPreviewSucceeds) {
      previewRunning = true;
    }
    return reopenPreviewSucceeds;
  }
  static uint32_t lastFrame() { return 0; }
  static bool recycleStall() {
    ++recycleStallCalls;
    return recycleStallSucceeds;
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
  static bool ensurePowerReady(const char*) {
    ++ensurePowerReadyCalls;
    if (!powerReadySucceeds) {
      guardActive = true;
      return false;
    }
    return true;
  }
  static bool activateWifi() {
    ++activateCalls;
    return true;
  }
  static bool finishWlan() {
    ++finishWlanCalls;
    finishWlanSawConnectedSta = wifiConnected;
    return true;
  }
  static bool deactivateWifi() {
    ++deactivateCalls;
    return deactivateSucceeds;
  }
  static bool reactivateWifi() {
    ++reactivateCalls;
    return reactivateSucceeds;
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
    result.ensureCameraPowerReady = ensurePowerReady;
    result.activateCameraWifiOverBle = activateWifi;
    result.finishCameraWifiOverHttp = finishWlan;
    result.deactivateCameraWifiOverBle = deactivateWifi;
    result.reactivateCameraWifiForCachedResume = reactivateWifi;
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
bool FlowHarness::deactivateSucceeds = true;
bool FlowHarness::reactivateSucceeds = true;
bool FlowHarness::powerReadySucceeds = true;
uint32_t FlowHarness::lastRecoveryAt = 0;
uint32_t FlowHarness::activateCalls = 0;
uint32_t FlowHarness::ensurePowerReadyCalls = 0;
uint32_t FlowHarness::finishWlanCalls = 0;
bool FlowHarness::finishWlanSawConnectedSta = false;
uint32_t FlowHarness::deactivateCalls = 0;
uint32_t FlowHarness::reactivateCalls = 0;
uint32_t FlowHarness::readCredentialsCalls = 0;
uint32_t FlowHarness::applyCredentialsCalls = 0;
uint32_t FlowHarness::connectCalls = 0;
uint32_t FlowHarness::disconnectCalls = 0;
uint32_t FlowHarness::openPreviewCalls = 0;
uint32_t FlowHarness::runBleDiscoveryCalls = 0;
uint32_t FlowHarness::resetStackCalls = 0;
uint32_t FlowHarness::restoreReleasedBleCalls = 0;
bool FlowHarness::rescanQueriedBeforeRestore = false;
std::string FlowHarness::lastRescanReason;
bool FlowHarness::shutterReadyValue = true;
bool FlowHarness::shutterOverWifiValue = false;
bool FlowHarness::shootSucceeds = true;
bool FlowHarness::previewKeptAfterFailure = false;
uint32_t FlowHarness::reopenPreviewCalls = 0;
bool FlowHarness::reopenPreviewSucceeds = true;
uint32_t FlowHarness::recycleStallCalls = 0;
bool FlowHarness::recycleStallSucceeds = true;

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

void testPortraitStartupCachesWifiWithoutConnecting() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::deactivateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::readCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::applyCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::openPreviewCalls);
}

void testPortraitReconnectReusesCacheWithoutTogglingCameraWlan() {
  FlowHarness::reset();
  FlowHarness::cachedCredentials = true;
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::ensurePowerReadyCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::finishWlanCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::deactivateCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::readCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::applyCredentialsCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::openPreviewCalls);
  TEST_ASSERT_TRUE(FlowHarness::bleConnected);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
}

void testPortraitCachedResumeStaysOffWhenCameraPowerIsOff() {
  FlowHarness::reset();
  FlowHarness::cachedCredentials = true;
  FlowHarness::powerReadySucceeds = false;
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();

  TEST_ASSERT_FALSE(controller.runCameraFlowOnce(actions, 100));
  TEST_ASSERT_NOT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                            static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::ensurePowerReadyCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::activateCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::deactivateCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_TRUE(FlowHarness::guardActive);
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
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::deactivateCalls);
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
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::reactivateCalls);
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
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::finishWlanCalls);
  TEST_ASSERT_TRUE(FlowHarness::finishWlanSawConnectedSta);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::deactivateCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_FALSE(FlowHarness::previewRunning);
  TEST_ASSERT_TRUE(FlowHarness::bleConnected);
}

void testCachedResumeStopsBeforeWifiConnectWhenReactivationFails() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  const rvf::AppFlowActions actions = FlowHarness::actions();
  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));

  FlowHarness::reactivateSucceeds = false;
  controller.setPreviewRequested(true);
  controller.serviceCameraFlowIfNeeded(actions, 200);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::BleReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::reactivateCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::connectCalls);
  TEST_ASSERT_TRUE(FlowHarness::bleConnected);
}

void testWifiDeactivationFailureDoesNotBreakBleShutterState() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::BleScan);
  controller.begin(rvf::AppState::BleScan);
  controller.setPreviewRequested(true);
  const rvf::AppFlowActions actions = FlowHarness::actions();
  TEST_ASSERT_TRUE(controller.runCameraFlowOnce(actions, 100));

  FlowHarness::deactivateSucceeds = false;
  controller.setPreviewRequested(false);
  controller.serviceCameraFlowIfNeeded(actions, 200);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::AppState::WifiCredentialsReady),
                        static_cast<int>(controller.state()));
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::deactivateCalls);
  TEST_ASSERT_FALSE(FlowHarness::wifiConnected);
  TEST_ASSERT_FALSE(FlowHarness::previewRunning);
  TEST_ASSERT_TRUE(FlowHarness::bleConnected);
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

void testRecoveryRestoresReleasedBleBeforeChoosingPath() {
  FlowHarness::reset();
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  controller.begin(rvf::AppState::PreviewRunning);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.restoreReleasedBle = FlowHarness::restoreReleasedBle;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;

  controller.recoverCameraConnection(actions, "WiFi disconnected");

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::restoreReleasedBleCalls);
  TEST_ASSERT_FALSE(FlowHarness::rescanQueriedBeforeRestore);
  TEST_ASSERT_EQUAL_STRING("WiFi disconnected", FlowHarness::lastRescanReason.c_str());
}

void testRecoveryKeepsReleasedBleDownUnderSleepGuard() {
  FlowHarness::reset();
  FlowHarness::guardActive = true;
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  controller.begin(rvf::AppState::PreviewRunning);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.restoreReleasedBle = FlowHarness::restoreReleasedBle;

  controller.recoverCameraConnection(actions, "WiFi disconnected");

  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::restoreReleasedBleCalls);
}

void testLandscapeShutterNotReadyRecoversWithoutBleRescanReason() {
  FlowHarness::reset();
  FlowHarness::shutterReadyValue = false;
  FlowHarness::shutterOverWifiValue = true;
  rvf::AppController controller(rvf::AppState::BleReady);
  controller.begin(rvf::AppState::BleReady);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.shutterReady = FlowHarness::shutterReady;
  actions.shutterOverWifi = FlowHarness::shutterOverWifi;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;

  controller.handleUserCommand(actions, rvf::UserCommand::Shoot);

  TEST_ASSERT_EQUAL_STRING("Button A shutter WiFi not ready",
                           FlowHarness::lastRescanReason.c_str());
}

void testPortraitShutterNotReadyKeepsBleRescanReason() {
  FlowHarness::reset();
  FlowHarness::shutterReadyValue = false;
  rvf::AppController controller(rvf::AppState::WifiCredentialsReady);
  controller.begin(rvf::AppState::WifiCredentialsReady);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.shutterReady = FlowHarness::shutterReady;
  actions.shutterOverWifi = FlowHarness::shutterOverWifi;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;

  controller.handleUserCommand(actions, rvf::UserCommand::Shoot);

  TEST_ASSERT_EQUAL_STRING("Button A shutter BLE not ready",
                           FlowHarness::lastRescanReason.c_str());
}

void testLandscapeHttpShutterFailureNamesHttpPath() {
  FlowHarness::reset();
  FlowHarness::shutterOverWifiValue = true;
  FlowHarness::shootSucceeds = false;
  rvf::AppController controller(rvf::AppState::BleReady);
  controller.begin(rvf::AppState::BleReady);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.shutterReady = FlowHarness::shutterReady;
  actions.shutterOverWifi = FlowHarness::shutterOverWifi;
  actions.shootAutofocus = FlowHarness::shoot;
  actions.previewKeptAfterShutterFailure = FlowHarness::keptPreview;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;

  controller.handleUserCommand(actions, rvf::UserCommand::Shoot);

  TEST_ASSERT_EQUAL_STRING("Button A HTTP shutter failed",
                           FlowHarness::lastRescanReason.c_str());
}

void testLandscapeHttpShutterSuccessReopensPreview() {
  FlowHarness::reset();
  FlowHarness::wifiConnected = true;
  FlowHarness::shutterOverWifiValue = true;
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  controller.begin(rvf::AppState::PreviewRunning);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.shutterReady = FlowHarness::shutterReady;
  actions.shutterOverWifi = FlowHarness::shutterOverWifi;
  actions.shootAutofocus = FlowHarness::shoot;
  actions.reopenPreviewAfterHttpShutter = FlowHarness::reopenPreview;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;
  actions.restoreReleasedBle = FlowHarness::restoreReleasedBle;

  controller.handleUserCommand(actions, rvf::UserCommand::Shoot);

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::reopenPreviewCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::restoreReleasedBleCalls);
  TEST_ASSERT_TRUE(FlowHarness::lastRescanReason.empty());
  TEST_ASSERT_TRUE(FlowHarness::previewRunning);
}

void testLandscapeStallRecyclesPreviewWithoutBleRecovery() {
  FlowHarness::reset();
  FlowHarness::wifiConnected = true;
  FlowHarness::previewRunning = true;
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  controller.begin(rvf::AppState::PreviewRunning);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.liveViewStallTimeoutMs = 0;
  actions.lastFrameAt = FlowHarness::lastFrame;
  actions.recyclePreviewAfterStall = FlowHarness::recycleStall;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;
  actions.restoreReleasedBle = FlowHarness::restoreReleasedBle;

  controller.monitorLiveView(actions, 1);

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::recycleStallCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FlowHarness::restoreReleasedBleCalls);
  TEST_ASSERT_TRUE(FlowHarness::lastRescanReason.empty());
}

void testLandscapeStallRecycleFailureRecovers() {
  FlowHarness::reset();
  FlowHarness::wifiConnected = true;
  FlowHarness::previewRunning = true;
  FlowHarness::recycleStallSucceeds = false;
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  controller.begin(rvf::AppState::PreviewRunning);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.liveViewStallTimeoutMs = 0;
  actions.lastFrameAt = FlowHarness::lastFrame;
  actions.recyclePreviewAfterStall = FlowHarness::recycleStall;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;
  actions.restoreReleasedBle = FlowHarness::restoreReleasedBle;

  controller.monitorLiveView(actions, 1);

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::recycleStallCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::restoreReleasedBleCalls);
  TEST_ASSERT_EQUAL_STRING("LiveView frame stall watchdog",
                           FlowHarness::lastRescanReason.c_str());
}

void testLandscapeHttpShutterReopenFailureRecovers() {
  FlowHarness::reset();
  FlowHarness::wifiConnected = true;
  FlowHarness::shutterOverWifiValue = true;
  FlowHarness::reopenPreviewSucceeds = false;
  rvf::AppController controller(rvf::AppState::PreviewRunning);
  controller.begin(rvf::AppState::PreviewRunning);
  controller.setPreviewRequested(true);
  rvf::AppFlowActions actions = FlowHarness::actions();
  actions.shutterReady = FlowHarness::shutterReady;
  actions.shutterOverWifi = FlowHarness::shutterOverWifi;
  actions.shootAutofocus = FlowHarness::shoot;
  actions.reopenPreviewAfterHttpShutter = FlowHarness::reopenPreview;
  actions.reasonRequiresBleRescan = FlowHarness::recordRescanReason;
  actions.restoreReleasedBle = FlowHarness::restoreReleasedBle;

  controller.handleUserCommand(actions, rvf::UserCommand::Shoot);

  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::reopenPreviewCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FlowHarness::restoreReleasedBleCalls);
  TEST_ASSERT_EQUAL_STRING("LiveView reopen after HTTP shutter failed",
                           FlowHarness::lastRescanReason.c_str());
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

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(testPortraitStartupCachesWifiWithoutConnecting);
  RUN_TEST(testPortraitReconnectReusesCacheWithoutTogglingCameraWlan);
  RUN_TEST(testPortraitCachedResumeStaysOffWhenCameraPowerIsOff);
  RUN_TEST(testLandscapeStartupOpensLiveViewWithoutPropsProbe);
  RUN_TEST(testPortraitToLandscapeResumesAfterCredentialCache);
  RUN_TEST(testLandscapeToPortraitDisconnectsWifiAndKeepsBleReady);
  RUN_TEST(testCachedResumeStopsBeforeWifiConnectWhenReactivationFails);
  RUN_TEST(testWifiDeactivationFailureDoesNotBreakBleShutterState);
  RUN_TEST(testCameraSleepGuardKeepsControllerOutOfScan);
  RUN_TEST(testRecoveryRestoresReleasedBleBeforeChoosingPath);
  RUN_TEST(testRecoveryKeepsReleasedBleDownUnderSleepGuard);
  RUN_TEST(testLandscapeShutterNotReadyRecoversWithoutBleRescanReason);
  RUN_TEST(testPortraitShutterNotReadyKeepsBleRescanReason);
  RUN_TEST(testLandscapeHttpShutterFailureNamesHttpPath);
  RUN_TEST(testLandscapeHttpShutterSuccessReopensPreview);
  RUN_TEST(testLandscapeStallRecyclesPreviewWithoutBleRecovery);
  RUN_TEST(testLandscapeStallRecycleFailureRecovers);
  RUN_TEST(testLandscapeHttpShutterReopenFailureRecovers);
  RUN_TEST(testRecoveryStopsWhenBleStackResetFails);
  RUN_TEST(testSupervisorWaitsForIntervalAndIgnoresHealthyPreview);
  RUN_TEST(testSupervisorReportsPreviewClosed);
  RUN_TEST(testSupervisorIgnoresCameraSleepGuard);
  RUN_TEST(testSupervisorReportsPreviewIdleTimeout);
  RUN_TEST(testSupervisorReportsFrameStallDespiteIncomingBytes);
  return UNITY_END();
}
