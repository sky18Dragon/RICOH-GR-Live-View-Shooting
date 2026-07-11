#include "DisplaySurface.h"

namespace rvf::display {

bool M5DisplaySurface::begin() {
    auto config = M5.config();
    config.serial_baudrate = 115200;
    M5.begin(config);

    M5.Display.setRotation(1);
    _width = M5.Display.width();
    _height = M5.Display.height();

    // StickS3 is physically 135x240. Keep the logical surface landscape even
    // when a board package reports the rotations in the opposite order.
    if (_width < _height) {
        M5.Display.setRotation(3);
        _width = M5.Display.width();
        _height = M5.Display.height();
    }

    _canvas.setColorDepth(16);
    if (_canvas.createSprite(_width, _height) == nullptr) {
        return false;
    }
    _canvas.setTextSize(1);
    _canvas.setTextWrap(false);
    clear(0x0000);
    present();
    return true;
}

LovyanGFX& M5DisplaySurface::canvas() {
    return _canvas;
}

const LovyanGFX& M5DisplaySurface::canvas() const {
    return _canvas;
}

void M5DisplaySurface::clear(uint16_t color) {
    _canvas.fillScreen(color);
}

void M5DisplaySurface::present() {
    _canvas.pushSprite(&M5.Display, 0, 0);
}

int16_t M5DisplaySurface::width() const {
    return _width;
}

int16_t M5DisplaySurface::height() const {
    return _height;
}

}  // namespace rvf::display
