#pragma once

#include <cstdint>

namespace rvf::ui {

struct RicohTheme {
    static constexpr uint16_t kBackground = 0x1082;
    static constexpr uint16_t kPrimary = 0xFD20;
    static constexpr uint16_t kText = 0xFFFF;
    static constexpr uint16_t kSuccess = 0x07E0;
    static constexpr uint16_t kWarning = 0xFFE0;
    static constexpr uint16_t kError = 0xF800;
    static constexpr uint16_t kSlate = 0x2104;
    static constexpr uint16_t kGray = 0x7BEF;
    static constexpr uint16_t kCard = 0x0841;
    static constexpr uint16_t kPanel = 0x18E3;
    static constexpr uint16_t kGraphite = 0x31A6;
    static constexpr uint16_t kDark = 0x0000;
    static constexpr uint16_t kHighlight = 0xE71C;
};

}  // namespace rvf::ui
