#pragma once

#include <cstdint>

namespace rvf::ui {

struct KawaiiLayout {
    static constexpr int16_t kScreenW = 240;
    static constexpr int16_t kScreenH = 135;

    static constexpr int16_t kOuterMargin = 4;
    static constexpr int16_t kHeaderX = 40;
    static constexpr int16_t kHeaderY = 3;
    static constexpr int16_t kHeaderW = 196;
    static constexpr int16_t kHeaderH = 19;
    static constexpr int16_t kHeaderRadius = 9;

    static constexpr int16_t kShellX = 4;
    static constexpr int16_t kShellY = 10;
    static constexpr int16_t kShellW = 232;
    static constexpr int16_t kShellH = 102;
    static constexpr int16_t kShellRadius = 12;
    static constexpr int16_t kEarTopY = 3;
    static constexpr int16_t kFooterY = 115;
    static constexpr int16_t kFooterH = 18;
    static constexpr int16_t kCardRadius = 7;
    static constexpr int16_t kInnerGap = 4;

    static constexpr int16_t kStatusX = 6;
    static constexpr int16_t kStatusY = 27;
    static constexpr int16_t kStatusW = 60;
    static constexpr int16_t kStatusH = 74;
    static constexpr int16_t kStatusRowY = 30;
    static constexpr int16_t kStatusRowH = 11;
    static constexpr int16_t kStatusRowRadius = 5;
    static constexpr int16_t kStatusRowGap = 3;

    static constexpr int16_t kButtonX = 6;
    static constexpr int16_t kButtonY = 105;
    static constexpr int16_t kButtonW = 88;
    static constexpr int16_t kButtonH = 12;
    static constexpr int16_t kButtonGap = 3;

    static constexpr int16_t kBootTitleX = 18;
    static constexpr int16_t kBootTitleY = 42;
    static constexpr int16_t kBootThemeX = 54;
    static constexpr int16_t kBootThemeY = 66;
    static constexpr int16_t kBootProgressX = 42;
    static constexpr int16_t kBootProgressY = 87;
    static constexpr int16_t kBootProgressW = 132;
    static constexpr int16_t kBootProgressH = 14;

    static constexpr int16_t kBootMascotX = 211;
    static constexpr int16_t kBootMascotY = 72;
    static constexpr uint8_t kBootMascotScale = 1;
    static constexpr int16_t kLiveMascotX = 213;
    static constexpr int16_t kLiveMascotY = 92;
    static constexpr uint8_t kLiveMascotScale = 1;

    static constexpr int16_t kSettingsLeftX = 8;
    static constexpr int16_t kSettingsRightX = 124;
    static constexpr int16_t kSettingsTopY = 36;
    static constexpr int16_t kSettingsTileW = 108;
    static constexpr int16_t kSettingsTileH = 16;
    static constexpr int16_t kSettingsTileGap = 2;

    static constexpr int16_t kFocusBoxW = 26;
    static constexpr int16_t kFocusBoxH = 18;
    static constexpr int16_t kLiveSafeX = 58;
    static constexpr int16_t kLiveSafeY = 36;
    static constexpr int16_t kLiveSafeW = 124;
    static constexpr int16_t kLiveSafeH = 71;
};

}  // namespace rvf::ui
