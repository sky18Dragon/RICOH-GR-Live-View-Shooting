#pragma once

#include <cstdint>

namespace rvf::ui {

struct RabbitTheme {
    static constexpr uint16_t kBackground = 0xFF9C;
    static constexpr uint16_t kPanel = 0xFFDF;
    static constexpr uint16_t kPanelShadow = 0xE71C;
    static constexpr uint16_t kBorder = 0x9B0C;
    static constexpr uint16_t kText = 0x5142;
    static constexpr uint16_t kMuted = 0xA514;
    static constexpr uint16_t kPink = 0xFBB6;
    static constexpr uint16_t kPinkDark = 0xE30F;
    static constexpr uint16_t kWhite = 0xFFFF;
    static constexpr uint16_t kSuccess = 0x4E6B;
    static constexpr uint16_t kWarning = 0xFD20;
    static constexpr uint16_t kError = 0xE9E7;
};

}  // namespace rvf::ui
