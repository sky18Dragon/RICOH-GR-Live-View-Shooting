#pragma once

#include "../../core/UiFeatures.h"

namespace rvf::ui {

struct RicohUiProfile {
    static constexpr bool kShowFps = uiFeatureValue(UI_FEATURE_FPS, true);
    static constexpr bool kShowFrameStats = uiFeatureValue(UI_FEATURE_FRAME_STATS, true);
    static constexpr bool kShowWifiRssi = uiFeatureValue(UI_FEATURE_WIFI_RSSI, true);
    static constexpr bool kShowBattery = uiFeatureValue(UI_FEATURE_BATTERY, true);
    static constexpr bool kShowCameraModel = uiFeatureValue(UI_FEATURE_CAMERA_MODEL, true);
    static constexpr bool kShowFocusBracket = uiFeatureValue(UI_FEATURE_FOCUS_BRACKET, true);
};

}  // namespace rvf::ui
