#include "jpeg_decoder.h"

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

bool JpegDecoder::drawFrame(LovyanGFX* dst, const uint8_t* data, size_t length) {
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
    _sourceW = (_lastWidth + divisor - 1) / divisor;
    _sourceH = (_lastHeight + divisor - 1) / divisor;
    const ImageFitRect fit = calculateContainRect(_sourceW, _sourceH, _displayW, _displayH);
    _drawX = fit.x;
    _drawY = fit.y;
    _targetW = fit.width;
    _targetH = fit.height;
    if (_targetW <= 0 || _targetH <= 0 ||
        _targetW > static_cast<int>(sizeof(_fitRow) / sizeof(_fitRow[0]))) {
        _jpeg.close();
        activeDecoder = nullptr;
        return setError("invalid fitted JPEG dimensions");
    }

    // The frame is rendered with contain semantics: preserve its aspect ratio,
    // show the whole image, and leave black letterbox/pillarbox bars as needed.
    _dst->fillRect(0, 0, _displayW, _displayH, COLOR_BLACK);

    _dst->startWrite();
    const int rc = _jpeg.decode(0, 0, scale);
    _dst->endWrite();
    _jpeg.close();
    activeDecoder = nullptr;

    _lastDecodeMs = millis() - started;
    if (rc == 0) {
        return setError("JPEG decode failed");
    }

    _lastError = "ok";
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
    if (_sourceW <= 0 || _sourceH <= 0 || _targetW <= 0 || _targetH <= 0) {
        return 0;
    }

    const int sourceX0 = draw->x;
    const int sourceY0 = draw->y;
    const int sourceX1 = min(_sourceW, sourceX0 + draw->iWidth);
    const int sourceY1 = min(_sourceH, sourceY0 + draw->iHeight);
    if (sourceX0 >= sourceX1 || sourceY0 >= sourceY1) {
        return 1;
    }

    uint16_t* pixels = static_cast<uint16_t*>(draw->pPixels);
    const int stride = draw->iWidth;

    const int targetX0 = (sourceX0 * _targetW + _sourceW - 1) / _sourceW;
    const int targetX1 = (sourceX1 * _targetW + _sourceW - 1) / _sourceW;
    const int targetY0 = (sourceY0 * _targetH + _sourceH - 1) / _sourceH;
    const int targetY1 = (sourceY1 * _targetH + _sourceH - 1) / _sourceH;
    const int drawWidth = targetX1 - targetX0;
    if (drawWidth <= 0) {
        return 1;
    }

    for (int targetY = targetY0; targetY < targetY1; ++targetY) {
        const int sourceY = (targetY * _sourceH) / _targetH;
        const int localY = sourceY - sourceY0;
        for (int targetX = targetX0; targetX < targetX1; ++targetX) {
            const int sourceX = (targetX * _sourceW) / _targetW;
            const int localX = sourceX - sourceX0;
            _fitRow[targetX - targetX0] = pixels[localY * stride + localX];
        }
        _dst->pushImage(_drawX + targetX0,
                        _drawY + targetY,
                        drawWidth,
                        1,
                        _fitRow);
    }
    return 1;
}
