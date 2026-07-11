#include <unity.h>

#include "ui/model/UiScreen.h"
#include "ui/variants/debug/DebugUiProfile.h"
#include "ui/variants/kawaii/KawaiiUiProfile.h"
#include "ui/variants/minimal/MinimalUiProfile.h"
#include "ui/variants/ricoh/RicohUiProfile.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

void testRicohProfileDefaults() {
    TEST_ASSERT_TRUE(rvf::ui::RicohUiProfile::kShowFps);
    TEST_ASSERT_TRUE(rvf::ui::RicohUiProfile::kShowFrameStats);
    TEST_ASSERT_TRUE(rvf::ui::RicohUiProfile::kShowWifiRssi);
    TEST_ASSERT_TRUE(rvf::ui::RicohUiProfile::kShowBattery);
    TEST_ASSERT_TRUE(rvf::ui::RicohUiProfile::kShowCameraModel);
    TEST_ASSERT_TRUE(rvf::ui::RicohUiProfile::kShowFocusBracket);
}

void testMinimalProfileDefaults() {
    TEST_ASSERT_FALSE(rvf::ui::MinimalUiProfile::kShowFps);
    TEST_ASSERT_FALSE(rvf::ui::MinimalUiProfile::kShowFrameStats);
    TEST_ASSERT_TRUE(rvf::ui::MinimalUiProfile::kShowWifiRssi);
    TEST_ASSERT_TRUE(rvf::ui::MinimalUiProfile::kShowBattery);
    TEST_ASSERT_FALSE(rvf::ui::MinimalUiProfile::kShowCameraModel);
    TEST_ASSERT_TRUE(rvf::ui::MinimalUiProfile::kShowFocusBracket);
}

void testDebugProfileDefaults() {
    TEST_ASSERT_TRUE(rvf::ui::DebugUiProfile::kShowFps);
    TEST_ASSERT_TRUE(rvf::ui::DebugUiProfile::kShowFrameStats);
    TEST_ASSERT_TRUE(rvf::ui::DebugUiProfile::kShowWifiRssi);
    TEST_ASSERT_TRUE(rvf::ui::DebugUiProfile::kShowBattery);
    TEST_ASSERT_TRUE(rvf::ui::DebugUiProfile::kShowCameraModel);
    TEST_ASSERT_TRUE(rvf::ui::DebugUiProfile::kShowFocusBracket);
}

void testKawaiiProfileDefaults() {
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowFps);
    TEST_ASSERT_FALSE(rvf::ui::KawaiiUiProfile::kShowFrameStats);
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowWifiRssi);
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowBattery);
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowCameraModel);
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowFocusBracket);
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowMascots);
    TEST_ASSERT_TRUE(rvf::ui::KawaiiUiProfile::kShowPatternBackground);
}

void testSettingsScreenContract() {
    TEST_ASSERT_EQUAL_STRING("SETTINGS", rvf::ui::uiScreenName(rvf::ui::UiScreen::Settings));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(testRicohProfileDefaults);
    RUN_TEST(testMinimalProfileDefaults);
    RUN_TEST(testDebugProfileDefaults);
    RUN_TEST(testKawaiiProfileDefaults);
    RUN_TEST(testSettingsScreenContract);
    return UNITY_END();
}
