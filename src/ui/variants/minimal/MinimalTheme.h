#pragma once

#include <cstdint>

namespace rvf::ui {

struct MinimalTheme {
    static constexpr uint16_t kBackground = 0x0000;
    static constexpr uint16_t kPrimary = 0xFFFF;
    static constexpr uint16_t kMuted = 0x7BEF;
    static constexpr uint16_t kSuccess = 0x07E0;
    static constexpr uint16_t kWarning = 0xFFE0;
    static constexpr uint16_t kError = 0xF800;
};

}  // namespace rvf::ui
