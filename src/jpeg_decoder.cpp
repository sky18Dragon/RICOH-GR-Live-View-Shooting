#include "jpeg_decoder.h"

#include <algorithm>
#include <climits>

#include <esp_heap_caps.h>

namespace {
constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr size_t RGB565_BYTES_PER_PIXEL = 2;

bool isSofMarker(uint8_t marker) {
    switch (marker) {
        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xCD:
        case 0xCE:
        case 0xCF:
            return true;
        default:
            return false;
    }
}
}  // namespace

bool JpegDecoder::begin() {
    _displayW = M5.Display.width() > 0 ? M5.Display.width() : DISPLAY_WIDTH;
    _displayH = M5.Display.height() > 0 ? M5.Display.height() : DISPLAY_HEIGHT;
    _lastDecodeMs = 0;
    _lastWidth = 0;
    _lastHeight = 0;
    return ensureDecoder();
}

bool JpegDecoder::ensureDecoder() {
    if (_jpeg == nullptr) {
        jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
        config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
        config.scale.width = JPEG_DECODE_WIDTH;
        config.scale.height = JPEG_DECODE_HEIGHT;

        const jpeg_error_t rc = jpeg_dec_open(&config, &_jpeg);
        if (rc != JPEG_ERR_OK || _jpeg == nullptr) {
            Serial.printf("JPEG: esp_new_jpeg open failed rc=%d\n", static_cast<int>(rc));
            _jpeg = nullptr;
            return setError("JPEG decoder open failed");
        }
    }

    if (_outputBuffer == nullptr) {
        _outputCapacity = static_cast<size_t>(JPEG_DECODE_WIDTH) *
                          static_cast<size_t>(JPEG_DECODE_HEIGHT) *
                          RGB565_BYTES_PER_PIXEL;
        const bool hasPsram = psramFound();
        const char* storage = hasPsram ? "psram" : "internal";
        if (hasPsram) {
            _outputBuffer = static_cast<uint8_t*>(heap_caps_aligned_calloc(
                16, 1, _outputCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        }
        if (_outputBuffer == nullptr) {
            _outputBuffer = static_cast<uint8_t*>(heap_caps_aligned_calloc(
                16, 1, _outputCapacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            storage = "internal";
        }
        if (_outputBuffer == nullptr) {
            return setError("JPEG output allocation failed");
        }
        Serial.printf("JPEG: esp_new_jpeg %ux%u buffer=%u storage=%s free_int=%lu free_psram=%lu\n",
                      static_cast<unsigned>(JPEG_DECODE_WIDTH),
                      static_cast<unsigned>(JPEG_DECODE_HEIGHT),
                      static_cast<unsigned>(_outputCapacity),
                      storage,
                      static_cast<unsigned long>(
                          heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                      static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    }

    _lastError = "ok";
    return true;
}

bool JpegDecoder::drawFrame(LovyanGFX* dst,
                            const uint8_t* data,
                            size_t length,
                            bool mirrorHorizontal) {
    const uint32_t started = millis();
    _lastDecodeMs = 0;
    if (data == nullptr || length < 4) {
        return setError("empty jpeg frame");
    }
    if (length > static_cast<size_t>(INT_MAX)) {
        return setError("jpeg frame too large");
    }
    if (!ensureDecoder()) {
        return false;
    }

    _dst = dst != nullptr ? dst : &M5.Display;
    _mirrorHorizontal = mirrorHorizontal;
    readJpegDimensions(data, length, _lastWidth, _lastHeight);

    jpeg_dec_io_t io = {};
    jpeg_dec_header_info_t outputInfo = {};
    io.inbuf = const_cast<uint8_t*>(data);
    io.inbuf_len = static_cast<int>(length);

    jpeg_error_t rc = jpeg_dec_parse_header(_jpeg, &io, &outputInfo);
    if (rc != JPEG_ERR_OK) {
        _lastDecodeMs = millis() - started;
        Serial.printf("JPEG: header parse failed rc=%d len=%u\n",
                      static_cast<int>(rc),
                      static_cast<unsigned>(length));
        return setError("JPEG header parse failed");
    }

    int outputLength = 0;
    rc = jpeg_dec_get_outbuf_len(_jpeg, &outputLength);
    if (rc != JPEG_ERR_OK || outputLength <= 0 ||
        static_cast<size_t>(outputLength) > _outputCapacity) {
        _lastDecodeMs = millis() - started;
        Serial.printf("JPEG: invalid output rc=%d size=%d capacity=%u dimensions=%ux%u\n",
                      static_cast<int>(rc),
                      outputLength,
                      static_cast<unsigned>(_outputCapacity),
                      static_cast<unsigned>(outputInfo.width),
                      static_cast<unsigned>(outputInfo.height));
        return setError("JPEG output dimensions invalid");
    }

    io.outbuf = _outputBuffer;
    rc = jpeg_dec_process(_jpeg, &io);
    if (rc != JPEG_ERR_OK) {
        _lastDecodeMs = millis() - started;
        Serial.printf("JPEG: decode failed rc=%d\n", static_cast<int>(rc));
        return setError("JPEG decode failed");
    }

    const bool rendered = renderCenterCrop(outputInfo.width, outputInfo.height);
    _lastDecodeMs = millis() - started;
    if (!rendered) {
        return false;
    }

    _lastError = "ok";
    return true;
}

bool JpegDecoder::renderCenterCrop(int decodedWidth, int decodedHeight) {
    if (_dst == nullptr || decodedWidth <= 0 || decodedHeight <= 0 ||
        decodedWidth > JPEG_DECODE_WIDTH || decodedHeight > JPEG_DECODE_HEIGHT) {
        return setError("JPEG crop dimensions invalid");
    }

    const int visibleWidth = std::min(decodedWidth, _displayW);
    const int visibleHeight = std::min(decodedHeight, _displayH);
    const int cropX = (decodedWidth - visibleWidth) / 2;
    const int cropY = (decodedHeight - visibleHeight) / 2;
    const int drawX = (_displayW - visibleWidth) / 2;
    const int drawY = (_displayH - visibleHeight) / 2;
    const uint16_t* pixels = reinterpret_cast<const uint16_t*>(_outputBuffer);

    _dst->startWrite();
    _dst->fillRect(0, 0, _displayW, _displayH, COLOR_BLACK);

    if (!_mirrorHorizontal && cropX == 0 && visibleWidth == decodedWidth) {
        const uint16_t* firstVisibleRow = pixels + cropY * decodedWidth;
        _dst->pushImage(drawX, drawY, visibleWidth, visibleHeight, firstVisibleRow);
    } else {
        for (int row = 0; row < visibleHeight; ++row) {
            const uint16_t* source = pixels + (cropY + row) * decodedWidth + cropX;
            if (_mirrorHorizontal) {
                for (int column = 0; column < visibleWidth; ++column) {
                    _mirrorRow[column] = source[visibleWidth - 1 - column];
                }
                _dst->pushImage(drawX, drawY + row, visibleWidth, 1, _mirrorRow);
            } else {
                _dst->pushImage(drawX, drawY + row, visibleWidth, 1, source);
            }
        }
    }

    _dst->endWrite();
    return true;
}

bool JpegDecoder::readJpegDimensions(const uint8_t* data,
                                     size_t length,
                                     int& width,
                                     int& height) {
    width = 0;
    height = 0;
    if (data == nullptr || length < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;
    }

    size_t cursor = 2;
    while (cursor + 3 < length) {
        while (cursor < length && data[cursor] != 0xFF) ++cursor;
        while (cursor < length && data[cursor] == 0xFF) ++cursor;
        if (cursor >= length) break;

        const uint8_t marker = data[cursor++];
        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (cursor + 1 >= length) break;

        const size_t segmentLength =
            (static_cast<size_t>(data[cursor]) << 8) | data[cursor + 1];
        if (segmentLength < 2 || segmentLength > length - cursor) break;
        if (isSofMarker(marker) && segmentLength >= 7) {
            height = (static_cast<int>(data[cursor + 3]) << 8) | data[cursor + 4];
            width = (static_cast<int>(data[cursor + 5]) << 8) | data[cursor + 6];
            return width > 0 && height > 0;
        }
        cursor += segmentLength;
    }
    return false;
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
