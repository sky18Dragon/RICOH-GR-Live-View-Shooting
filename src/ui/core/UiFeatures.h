#pragma once

// -1 keeps the active UI profile default. Define a flag as 0 or 1 in
// PlatformIO to remove or include the element at compile time.
#ifndef UI_FEATURE_FPS
#define UI_FEATURE_FPS -1
#endif

#ifndef UI_FEATURE_FRAME_STATS
#define UI_FEATURE_FRAME_STATS -1
#endif

#ifndef UI_FEATURE_WIFI_RSSI
#define UI_FEATURE_WIFI_RSSI -1
#endif

#ifndef UI_FEATURE_BATTERY
#define UI_FEATURE_BATTERY -1
#endif

#ifndef UI_FEATURE_CAMERA_MODEL
#define UI_FEATURE_CAMERA_MODEL -1
#endif

#ifndef UI_FEATURE_FOCUS_BRACKET
#define UI_FEATURE_FOCUS_BRACKET -1
#endif

#ifndef UI_FEATURE_MASCOTS
#define UI_FEATURE_MASCOTS -1
#endif

#ifndef UI_FEATURE_PATTERN_BACKGROUND
#define UI_FEATURE_PATTERN_BACKGROUND -1
#endif

namespace rvf::ui {

constexpr bool uiFeatureValue(int overrideValue, bool profileDefault) {
    return overrideValue < 0 ? profileDefault : overrideValue != 0;
}

}  // namespace rvf::ui
