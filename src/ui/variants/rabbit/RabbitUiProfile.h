#pragma once

#include "../../core/UiFeatures.h"

namespace rvf::ui {

struct RabbitUiProfile {
    static constexpr bool kShowFps = uiFeatureValue(UI_FEATURE_FPS, true);
    static constexpr bool kShowFrameStats = uiFeatureValue(UI_FEATURE_FRAME_STATS, false);
    static constexpr bool kShowWifiRssi = uiFeatureValue(UI_FEATURE_WIFI_RSSI, true);
    static constexpr bool kShowBattery = uiFeatureValue(UI_FEATURE_BATTERY, true);
    static constexpr bool kShowCameraModel = uiFeatureValue(UI_FEATURE_CAMERA_MODEL, false);
    static constexpr bool kShowFocusBracket = uiFeatureValue(UI_FEATURE_FOCUS_BRACKET, true);
    static constexpr bool kShowRabbitBackground = uiFeatureValue(UI_FEATURE_MASCOTS, true);
};

}  // namespace rvf::ui
