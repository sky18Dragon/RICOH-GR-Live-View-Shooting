#pragma once

#include <cstdint>

namespace rvf::ui {

struct RicohLayout {
    static constexpr int16_t kCardX = 8;
    static constexpr int16_t kCardY = 6;
    static constexpr int16_t kCardInset = 8;
    static constexpr int16_t kCardVerticalInset = 6;
    static constexpr int16_t kCardRadius = 10;

    static constexpr int16_t kViewportX = 30;
    static constexpr int16_t kViewportY = 0;
    static constexpr int16_t kViewportWidth = 180;
    static constexpr int16_t kViewportHeight = 135;
    static constexpr int16_t kCropMarkLength = 8;

    static constexpr int16_t kFocusHalfWidth = 12;
    static constexpr int16_t kFocusHalfHeight = 8;
    static constexpr int16_t kHudInset = 14;
};

}  // namespace rvf::ui
