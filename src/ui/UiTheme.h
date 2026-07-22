#pragma once

#include <cstdint>

#ifndef UI_ANIMATION_ENABLED
#define UI_ANIMATION_ENABLED 1
#endif
#ifndef UI_SOUND_ENABLED
#define UI_SOUND_ENABLED 1
#endif
#ifndef UI_DEBUG_HUD
#define UI_DEBUG_HUD 0
#endif
#ifndef UI_ORIENTATION_ENABLED
#define UI_ORIENTATION_ENABLED 1
#endif

namespace rvf::UiTheme {

constexpr bool kAnimationEnabled = UI_ANIMATION_ENABLED != 0;
constexpr bool kSoundEnabled = UI_SOUND_ENABLED != 0;
constexpr bool kDebugHud = UI_DEBUG_HUD != 0;
constexpr bool kOrientationEnabled = UI_ORIENTATION_ENABLED != 0;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGreen = 0x4D6A;  // #4CAF50
constexpr uint16_t kRed = 0xF206;    // #F44336
constexpr uint16_t kGray = 0x7BEF;
constexpr uint16_t kDarkGray = 0x3186;
constexpr uint16_t kSleepGray = 0x528A;

constexpr uint16_t kPortraitWidth = 135;
constexpr uint16_t kPortraitHeight = 240;
constexpr uint16_t kLandscapeWidth = 240;
constexpr uint16_t kLandscapeHeight = 135;

constexpr uint16_t kConnectDotDiameter = 20;
constexpr uint16_t kConnectMergedDiameter = 40;
constexpr uint32_t kConnectMergeMs = 2000;
constexpr int16_t kConnectInitialInset = 32;

constexpr uint16_t kRemoteApertureDiameter = 60;
constexpr uint16_t kRemoteCoreDiameter = 20;
constexpr uint8_t kRemoteFocusScalePct = 80;
constexpr uint8_t kRemoteApertureLineWidth = 2;
constexpr uint32_t kFocusHoldThresholdMs = 300;
constexpr uint32_t kFocusTransitionMs = 300;
constexpr uint32_t kFocusTickIntervalMs = 350;

constexpr uint32_t kPortraitShutterFlashMs = 300;
constexpr uint32_t kLandscapeShutterCurtainMs = 100;
constexpr uint16_t kShutterFrameWidth = 15;

constexpr uint16_t kResetCircleDiameter = 40;
constexpr uint8_t kResetProgressWidthPct = 80;
constexpr uint16_t kResetProgressHeight = 4;
constexpr uint16_t kResetProgressBottom = 20;
constexpr uint32_t kResetHoldMs = 3000;
constexpr uint32_t kResetSplitMs = 300;
constexpr int16_t kResetSplitDistance = 30;

constexpr uint32_t kOrientationSampleMs = 40;
constexpr uint32_t kOrientationStableMs = 500;
constexpr uint32_t kOrientationMinHoldMs = 500;
constexpr float kOrientationLowPassAlpha = 0.20f;
constexpr float kOrientationHysteresisG = 0.18f;
constexpr float kOrientationMinAxisG = 0.35f;

constexpr uint8_t kActiveBrightness = 180;
constexpr uint8_t kSleepBrightness = 24;
constexpr uint32_t kSleepDimDurationMs = 900;
constexpr uint32_t kWakeBrightenDurationMs = 180;

constexpr uint16_t kRemoteAnimationFps = 25;
constexpr uint16_t kSleepAnimationFps = 8;
constexpr uint8_t kSoundVolume = 40;

}  // namespace rvf::UiTheme
