#pragma once

#include <cstddef>
#include <cstdint>

namespace rvf {

inline void mirrorRgb565Row(uint16_t* pixels, size_t width) {
    if (pixels == nullptr || width < 2) {
        return;
    }

    for (size_t left = 0, right = width - 1; left < right; ++left, --right) {
        const uint16_t pixel = pixels[left];
        pixels[left] = pixels[right];
        pixels[right] = pixel;
    }
}

}  // namespace rvf
