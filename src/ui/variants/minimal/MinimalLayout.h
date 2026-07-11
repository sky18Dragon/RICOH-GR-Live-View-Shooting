#pragma once

#include <cstdint>

namespace rvf::ui {

struct MinimalLayout {
    static constexpr int16_t kMargin = 8;
    static constexpr int16_t kHeaderY = 8;
    static constexpr int16_t kBodyY = 32;
    static constexpr int16_t kLineHeight = 18;
    static constexpr int16_t kFocusHalfWidth = 12;
    static constexpr int16_t kFocusHalfHeight = 8;
};

}  // namespace rvf::ui
