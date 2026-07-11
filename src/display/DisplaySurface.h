#pragma once

#include <M5Unified.h>

namespace rvf::display {

class M5DisplaySurface {
public:
    bool begin();

    LovyanGFX& canvas();
    const LovyanGFX& canvas() const;

    void clear(uint16_t color);
    void present();

    int16_t width() const;
    int16_t height() const;

private:
    int16_t _width = 0;
    int16_t _height = 0;
    M5Canvas _canvas;
};

}  // namespace rvf::display
