#pragma once

#include <cstdint>

namespace rvf::ui {

struct KawaiiLayout {
    static constexpr int16_t kScreenW = 240;
    static constexpr int16_t kScreenH = 135;

    static constexpr int16_t kOuterMargin = 4;
    static constexpr int16_t kShellX = 4;
    static constexpr int16_t kShellY = 10;
    static constexpr int16_t kShellW = 232;
    static constexpr int16_t kShellH = 102;
    static constexpr int16_t kShellRadius = 12;
    static constexpr int16_t kEarTopY = 3;
    static constexpr int16_t kHeaderH = 20;
    static constexpr int16_t kFooterY = 115;
    static constexpr int16_t kFooterH = 18;
    static constexpr int16_t kCardRadius = 7;
    static constexpr int16_t kInnerGap = 4;

    static constexpr int16_t kStatusCardX = 8;
    static constexpr int16_t kStatusCardY = 29;
    static constexpr int16_t kStatusCardW = 68;
    static constexpr int16_t kStatusCardH = 79;
    static constexpr int16_t kStatusRowsX = 81;
    static constexpr int16_t kStatusRowsY = 29;
    static constexpr int16_t kStatusRowsW = 151;
    static constexpr int16_t kStatusRowH = 13;
    static constexpr int16_t kStatusRowRadius = 6;
    static constexpr int16_t kStatusRowGap = 3;

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
