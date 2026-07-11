#include <unity.h>

#include "ui/variants/debug/DebugUiProfile.h"
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

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(testRicohProfileDefaults);
    RUN_TEST(testMinimalProfileDefaults);
    RUN_TEST(testDebugProfileDefaults);
    return UNITY_END();
}
