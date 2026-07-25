#pragma once

#include <cstdint>

struct ImageFitRect {
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
};

inline ImageFitRect calculateContainRect(int sourceWidth,
                                        int sourceHeight,
                                        int viewportWidth,
                                        int viewportHeight) {
    ImageFitRect result;
    if (sourceWidth <= 0 || sourceHeight <= 0 ||
        viewportWidth <= 0 || viewportHeight <= 0) {
        return result;
    }

    if (static_cast<int64_t>(sourceWidth) * viewportHeight >
        static_cast<int64_t>(viewportWidth) * sourceHeight) {
        result.width = viewportWidth;
        result.height = static_cast<int>(
            (static_cast<int64_t>(sourceHeight) * viewportWidth + sourceWidth / 2) /
            sourceWidth);
    } else {
        result.height = viewportHeight;
        result.width = static_cast<int>(
            (static_cast<int64_t>(sourceWidth) * viewportHeight + sourceHeight / 2) /
            sourceHeight);
    }

    if (result.width < 1) result.width = 1;
    if (result.height < 1) result.height = 1;
    if (result.width > viewportWidth) result.width = viewportWidth;
    if (result.height > viewportHeight) result.height = viewportHeight;
    result.x = (viewportWidth - result.width) / 2;
    result.y = (viewportHeight - result.height) / 2;
    return result;
}
