#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_jpeg_dec.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH 240
#endif

#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 135
#endif

#ifndef JPEG_DECODE_WIDTH
#define JPEG_DECODE_WIDTH 216
#endif

#ifndef JPEG_DECODE_HEIGHT
#define JPEG_DECODE_HEIGHT 144
#endif

class JpegDecoder {
public:
    bool begin();
    bool drawFrame(LovyanGFX* dst, const uint8_t* data, size_t length, bool mirrorHorizontal = false);

    uint32_t lastDecodeMs() const;
    int lastWidth() const;
    int lastHeight() const;
    const String& lastError() const;

private:
    jpeg_dec_handle_t _jpeg = nullptr;
    uint8_t* _outputBuffer = nullptr;
    size_t _outputCapacity = 0;
    LovyanGFX* _dst = nullptr;
    uint32_t _lastDecodeMs = 0;
    int _lastWidth = 0;
    int _lastHeight = 0;
    String _lastError = "not started";

    int _displayW = DISPLAY_WIDTH;
    int _displayH = DISPLAY_HEIGHT;
    bool _mirrorHorizontal = false;
    uint16_t _mirrorRow[JPEG_DECODE_WIDTH] = {};

    bool setError(const char* error);
    bool ensureDecoder();
    bool renderCenterCrop(int decodedWidth, int decodedHeight);
    static bool readJpegDimensions(const uint8_t* data, size_t length, int& width, int& height);
};
