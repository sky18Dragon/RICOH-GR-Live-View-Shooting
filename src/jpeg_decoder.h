#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <JPEGDEC.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH 240
#endif

#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 135
#endif

#ifndef JPEG_SCALE_POLICY
#define JPEG_SCALE_POLICY JPEG_SCALE_QUARTER
#endif

class JpegDecoder {
public:
    bool begin();
    bool drawFrame(LovyanGFX* dst,
                   const uint8_t* data,
                   size_t length,
                   bool mirrorHorizontal = false);

    uint32_t lastDecodeMs() const;
    int lastWidth() const;
    int lastHeight() const;
    const String& lastError() const;

private:
    JPEGDEC _jpeg;
    LovyanGFX* _dst = nullptr;
    uint32_t _lastDecodeMs = 0;
    int _lastWidth = 0;
    int _lastHeight = 0;
    String _lastError = "not started";

    int _drawX = 0;
    int _drawY = 0;
    int _displayW = DISPLAY_WIDTH;
    int _displayH = DISPLAY_HEIGHT;
    uint16_t _mirrorRow[DISPLAY_WIDTH] = {};

    bool setError(const char* error);
    bool mirrorRegion(int16_t x, int16_t y, int16_t width, int16_t height);

    static int jpegDrawCallback(JPEGDRAW* draw);
    int drawBlock(JPEGDRAW* draw);
};
