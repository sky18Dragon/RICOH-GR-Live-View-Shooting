#include "jpeg_decoder.h"

#include "image_transform.h"

namespace {
constexpr uint16_t COLOR_BLACK = 0x0000;

JpegDecoder* activeDecoder = nullptr;

int scaleDivisorFromOption(int option) {
#ifdef JPEG_SCALE_EIGHTH
    if (option == JPEG_SCALE_EIGHTH) {
        return 8;
    }
#endif
#ifdef JPEG_SCALE_QUARTER
    if (option == JPEG_SCALE_QUARTER) {
        return 4;
    }
#endif
#ifdef JPEG_SCALE_HALF
    if (option == JPEG_SCALE_HALF) {
        return 2;
    }
#endif
    return 1;
}
}  // namespace

bool JpegDecoder::begin() {
    _displayW = M5.Display.width() > 0 ? M5.Display.width() : DISPLAY_WIDTH;
    _displayH = M5.Display.height() > 0 ? M5.Display.height() : DISPLAY_HEIGHT;
    _lastDecodeMs = 0;
    _lastWidth = 0;
    _lastHeight = 0;
    _lastError = "ok";
    return true;
}

bool JpegDecoder::drawFrame(LovyanGFX* dst,
                            const uint8_t* data,
                            size_t length,
                            bool mirrorHorizontal) {
    if (data == nullptr || length < 4) {
        return setError("empty jpeg frame");
    }
    if (length > static_cast<size_t>(INT32_MAX)) {
        return setError("jpeg frame too large");
    }

    _dst = dst != nullptr ? dst : &M5.Display;
    const uint32_t started = millis();
    activeDecoder = this;

    if (!_jpeg.openRAM(const_cast<uint8_t*>(data), static_cast<int>(length), JpegDecoder::jpegDrawCallback)) {
        activeDecoder = nullptr;
        return setError("JPEG openRAM failed");
    }
    // M5GFX pushImage() expects byte-swapped RGB565 when Display.getSwapBytes() is false.
    // JPEGDEC defaults to little-endian RGB565, which produces psychedelic/garbled colors
    // on the StickS3 LCD. Output big-endian pixels directly for the SPI LCD path.
    _jpeg.setPixelType(RGB565_BIG_ENDIAN);

    _lastWidth = _jpeg.getWidth();
    _lastHeight = _jpeg.getHeight();

    const int scale = JPEG_SCALE_POLICY;
    const int divisor = scaleDivisorFromOption(scale);
    const int scaledW = (_lastWidth + divisor - 1) / divisor;
    const int scaledH = (_lastHeight + divisor - 1) / divisor;
    _drawX = (_displayW - scaledW) / 2;
    _drawY = (_displayH - scaledH) / 2;

    // Clear only the letterbox/pillarbox area.  The decoded image may be smaller
    // than the screen (quarter scale) or larger and clipped (half scale).  Without
    // this, status text from the previous screen remains visible around the image.
    const int16_t visibleX = _drawX < 0 ? 0 : _drawX;
    const int16_t visibleY = _drawY < 0 ? 0 : _drawY;
    const int16_t visibleW = min<int16_t>(_displayW, max<int16_t>(0, scaledW + min<int16_t>(_drawX, 0)));
    const int16_t visibleH = min<int16_t>(_displayH, max<int16_t>(0, scaledH + min<int16_t>(_drawY, 0)));
    if (visibleY > 0) {
        _dst->fillRect(0, 0, _displayW, visibleY, COLOR_BLACK);
    }
    if (visibleY + visibleH < _displayH) {
        _dst->fillRect(0, visibleY + visibleH, _displayW, _displayH - (visibleY + visibleH), COLOR_BLACK);
    }
    if (visibleX > 0) {
        _dst->fillRect(0, visibleY, visibleX, visibleH, COLOR_BLACK);
    }
    if (visibleX + visibleW < _displayW) {
        _dst->fillRect(visibleX + visibleW, visibleY, _displayW - (visibleX + visibleW), visibleH, COLOR_BLACK);
    }

    _dst->startWrite();
    const int rc = _jpeg.decode(_drawX, _drawY, scale);
    _dst->endWrite();
    _jpeg.close();
    activeDecoder = nullptr;

    if (rc == 0) {
        _lastDecodeMs = millis() - started;
        return setError("JPEG decode failed");
    }
    if (mirrorHorizontal && !mirrorRegion(visibleX, visibleY, visibleW, visibleH)) {
        _lastDecodeMs = millis() - started;
        return false;
    }

    _lastDecodeMs = millis() - started;
    _lastError = "ok";
    return true;
}

bool JpegDecoder::mirrorRegion(int16_t x,
                               int16_t y,
                               int16_t width,
                               int16_t height) {
    if (width <= 1 || height <= 0) {
        return true;
    }
    if (width > static_cast<int16_t>(DISPLAY_WIDTH)) {
        return setError("mirror region too wide");
    }

    for (int16_t row = 0; row < height; ++row) {
        _dst->readRect(x, y + row, width, 1, _mirrorRow);
        rvf::mirrorRgb565Row(_mirrorRow, static_cast<size_t>(width));
        _dst->pushImage(x, y + row, width, 1, _mirrorRow);
    }
    return true;
}

uint32_t JpegDecoder::lastDecodeMs() const {
    return _lastDecodeMs;
}

int JpegDecoder::lastWidth() const {
    return _lastWidth;
}

int JpegDecoder::lastHeight() const {
    return _lastHeight;
}

const String& JpegDecoder::lastError() const {
    return _lastError;
}

bool JpegDecoder::setError(const char* error) {
    _lastError = error != nullptr ? error : "unknown error";
    Serial.print(F("JPEG decoder error: "));
    Serial.println(_lastError);
    return false;
}

int JpegDecoder::jpegDrawCallback(JPEGDRAW* draw) {
    if (activeDecoder == nullptr || draw == nullptr) {
        return 0;
    }
    return activeDecoder->drawBlock(draw);
}

int JpegDecoder::drawBlock(JPEGDRAW* draw) {
    int16_t dstX = static_cast<int16_t>(draw->x);
    int16_t dstY = static_cast<int16_t>(draw->y);
    int16_t srcX = 0;
    int16_t srcY = 0;
    int16_t drawW = static_cast<int16_t>(draw->iWidth);
    int16_t drawH = static_cast<int16_t>(draw->iHeight);

    if (dstX < 0) {
        srcX = -dstX;
        drawW -= srcX;
        dstX = 0;
    }
    if (dstY < 0) {
        srcY = -dstY;
        drawH -= srcY;
        dstY = 0;
    }
    if (dstX + drawW > _displayW) {
        drawW = _displayW - dstX;
    }
    if (dstY + drawH > _displayH) {
        drawH = _displayH - dstY;
    }
    if (drawW <= 0 || drawH <= 0) {
        return 1;
    }

    uint16_t* pixels = static_cast<uint16_t*>(draw->pPixels);
    const int stride = draw->iWidth;

    if (srcX == 0 && drawW == draw->iWidth) {
        _dst->pushImage(dstX, dstY, drawW, drawH, pixels + (srcY * stride));
    } else {
        for (int16_t row = 0; row < drawH; ++row) {
            _dst->pushImage(dstX, dstY + row, drawW, 1, pixels + ((srcY + row) * stride) + srcX);
        }
    }
    return 1;
}
