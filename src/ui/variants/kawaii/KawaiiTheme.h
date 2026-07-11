#pragma once

#include <cstdint>

namespace rvf::ui {

// RGB565 colors tuned for a soft lavender palette on the StickS3 LCD.
struct KawaiiTheme {
    static constexpr uint16_t kBackground = 0xD61C;
    static constexpr uint16_t kBackgroundShade = 0xC57A;
    static constexpr uint16_t kPattern = 0xB478;
    static constexpr uint16_t kPatternSoft = 0xC55A;
    static constexpr uint16_t kPanel = 0xEF3E;
    static constexpr uint16_t kPanelSoft = 0xE6DD;
    static constexpr uint16_t kPanelBorder = 0x8B14;
    static constexpr uint16_t kPanelShadow = 0x622F;
    static constexpr uint16_t kPrimary = 0x7233;
    static constexpr uint16_t kPrimaryDark = 0x516E;
    static constexpr uint16_t kAccentPink = 0xD359;
    static constexpr uint16_t kAccentGlow = 0xEE5E;
    static constexpr uint16_t kText = 0x620F;
    static constexpr uint16_t kTextSoft = 0x72B1;
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kSuccess = 0x3349;
    static constexpr uint16_t kWarning = 0x8A64;
    static constexpr uint16_t kDanger = 0xA98C;
    static constexpr uint16_t kSkin = 0xFDD5;
    static constexpr uint16_t kHair = 0xC2C8;
    static constexpr uint16_t kCheek = 0xFBCF;
    static constexpr uint16_t kSuit = 0x9B56;
    static constexpr uint16_t kSuitSpot = 0x7251;
};

}  // namespace rvf::ui
