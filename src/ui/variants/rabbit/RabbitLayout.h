#pragma once

#include <cstdint>

namespace rvf::ui {

struct RabbitLayout {
    static constexpr int16_t kScreenW = 240;
    static constexpr int16_t kScreenH = 135;
    static constexpr int16_t kSafeX = 5;
    static constexpr int16_t kSafeW = 108;
    static constexpr int16_t kRabbitStartX = 127;
    static constexpr int16_t kHeaderY = 4;
    static constexpr int16_t kHeaderH = 18;
    static constexpr int16_t kRowY = 28;
    static constexpr int16_t kRowH = 14;
    static constexpr int16_t kRowGap = 3;
    static constexpr int16_t kFooterY = 115;
    static constexpr int16_t kFocusW = 30;
    static constexpr int16_t kFocusH = 22;

    static_assert(kSafeX + kSafeW < kRabbitStartX,
                  "Rabbit UI text safe area must not overlap the rabbit artwork");
};

}  // namespace rvf::ui
