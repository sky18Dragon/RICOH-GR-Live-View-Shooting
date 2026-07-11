#include <unity.h>

#include "ui/presenter/UiPresenter.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

using rvf::AppState;
using rvf::ui::UiModel;
using rvf::ui::UiPhase;
using rvf::ui::UiPresenter;
using rvf::ui::UiRuntimeSnapshot;
using rvf::ui::UiScreen;

UiModel presentState(AppState state) {
    UiRuntimeSnapshot snapshot;
    snapshot.appState = state;
    return UiPresenter().present(snapshot);
}

void assertPhase(UiPhase expected, const UiModel& model) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(model.phase));
}

void assertScreen(UiScreen expected, const UiModel& model) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(model.screen));
}

void testMapsBleScanToBleScanning() {
    const UiModel model = presentState(AppState::BleScan);
    assertScreen(UiScreen::Status, model);
    assertPhase(UiPhase::BleScanning, model);
}

void testMapsConnectingBleToBleConnecting() {
    assertPhase(UiPhase::BleConnecting, presentState(AppState::ConnectingBle));
}

void testMapsConnectingWifiToWifiConnecting() {
    assertPhase(UiPhase::WifiConnecting, presentState(AppState::ConnectingWifi));
}

void testMapsHttpProbingToHttpProbing() {
    assertPhase(UiPhase::HttpProbing, presentState(AppState::HttpProbing));
}

void testMapsPreviewStartingToStartingPreview() {
    assertPhase(UiPhase::StartingPreview, presentState(AppState::PreviewStarting));
}

void testMapsPreviewRunningToLiveView() {
    const UiModel model = presentState(AppState::PreviewRunning);
    assertScreen(UiScreen::LiveView, model);
    assertPhase(UiPhase::PreviewRunning, model);
}

void testCameraStandbyTakesPriorityOverAppStateMapping() {
    UiRuntimeSnapshot snapshot;
    snapshot.appState = AppState::ConnectingWifi;
    snapshot.wifiConnected = true;
    snapshot.cameraStandby = true;

    const UiModel model = UiPresenter().present(snapshot);

    assertPhase(UiPhase::CameraStandby, model);
    TEST_ASSERT_TRUE(model.cameraStandby);
}

void testErrorStateSelectsErrorScreenAndPhase() {
    UiRuntimeSnapshot snapshot;
    snapshot.appState = AppState::Error;
    snapshot.previewRunning = true;
    snapshot.cameraStandby = true;

    const UiModel model = UiPresenter().present(snapshot);

    assertScreen(UiScreen::Error, model);
    assertPhase(UiPhase::Error, model);
}

void testStructuredErrorFlagSelectsErrorScreen() {
    UiRuntimeSnapshot snapshot;
    snapshot.appState = AppState::BleScan;
    snapshot.errorActive = true;

    const UiModel model = UiPresenter().present(snapshot);

    assertScreen(UiScreen::Error, model);
    assertPhase(UiPhase::Error, model);
}

void testShutterFeedbackUsesTypedPhase() {
    UiRuntimeSnapshot snapshot;
    snapshot.appState = AppState::PreviewRunning;
    snapshot.previewRunning = true;
    snapshot.shutterStatus = rvf::ui::UiShutterStatus::Succeeded;

    const UiModel model = UiPresenter().present(snapshot);

    assertScreen(UiScreen::LiveView, model);
    assertPhase(UiPhase::Shooting, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(rvf::ui::UiShutterStatus::Succeeded),
                          static_cast<int>(model.shutterStatus));
}

void testDetailTextDoesNotInfluenceMapping() {
    UiRuntimeSnapshot plain;
    plain.appState = AppState::BleScan;
    plain.detail = "ordinary status text";

    UiRuntimeSnapshot misleading = plain;
    misleading.detail = "ERROR WIFI CONNECTING CAMERA STANDBY LIVE VIEW";

    const UiModel plainModel = UiPresenter().present(plain);
    const UiModel misleadingModel = UiPresenter().present(misleading);

    assertScreen(plainModel.screen, misleadingModel);
    assertPhase(plainModel.phase, misleadingModel);
    assertScreen(UiScreen::Status, misleadingModel);
    assertPhase(UiPhase::BleScanning, misleadingModel);
}

void testNullTextValuesBecomeSafeEmptyStrings() {
    UiRuntimeSnapshot snapshot;
    snapshot.cameraName = nullptr;
    snapshot.cameraModel = nullptr;
    snapshot.battery = nullptr;
    snapshot.localIp = nullptr;
    snapshot.detail = nullptr;
    snapshot.errorMessage = nullptr;

    const UiModel model = UiPresenter().present(snapshot);

    TEST_ASSERT_EQUAL_STRING("", model.cameraName);
    TEST_ASSERT_EQUAL_STRING("", model.cameraModel);
    TEST_ASSERT_EQUAL_STRING("", model.battery);
    TEST_ASSERT_EQUAL_STRING("", model.localIp);
    TEST_ASSERT_EQUAL_STRING("", model.detail);
    TEST_ASSERT_EQUAL_STRING("", model.errorMessage);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(testMapsBleScanToBleScanning);
    RUN_TEST(testMapsConnectingBleToBleConnecting);
    RUN_TEST(testMapsConnectingWifiToWifiConnecting);
    RUN_TEST(testMapsHttpProbingToHttpProbing);
    RUN_TEST(testMapsPreviewStartingToStartingPreview);
    RUN_TEST(testMapsPreviewRunningToLiveView);
    RUN_TEST(testCameraStandbyTakesPriorityOverAppStateMapping);
    RUN_TEST(testErrorStateSelectsErrorScreenAndPhase);
    RUN_TEST(testStructuredErrorFlagSelectsErrorScreen);
    RUN_TEST(testShutterFeedbackUsesTypedPhase);
    RUN_TEST(testDetailTextDoesNotInfluenceMapping);
    RUN_TEST(testNullTextValuesBecomeSafeEmptyStrings);
    return UNITY_END();
}
