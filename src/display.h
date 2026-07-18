#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH 240
#endif

#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 135
#endif

class DisplayUi {
public:
    bool begin();

    void showBoot(const char* message = nullptr);
    void showStatus(const char* line1, const char* line2, const char* line3, const char* line4);
    void showStatus(const String& line1, const String& line2, const String& line3, const String& line4);
    void showError(const char* message, const char* detail = nullptr);
    void showError(const String& message, const String& detail = String());
    void showPasskeyEntry(const uint8_t digits[6], uint8_t activeIndex);
    void drawOverlay(const String& wifiStatus,
                     const String& liveviewStatus,
                     const String& model,
                     const String& battery,
                     float fps,
                     int32_t rssi,
                     uint32_t frames,
                     uint32_t droppedFrames);

    int16_t width() const;
    int16_t height() const;

    LovyanGFX* getCanvas() { return &_canvas; }
    void pushCanvas() { _canvas.pushSprite(&M5.Display, 0, 0); }

private:
    int16_t _width = DISPLAY_WIDTH;
    int16_t _height = DISPLAY_HEIGHT;
    M5Canvas _canvas;

    void clear(uint16_t color = 0x0000);
    void drawStatusLines(const char* line1, const char* line2, const char* line3, const char* line4 = nullptr);
    void drawWifiIcon(int16_t x, int16_t y, int32_t rssi);
    void drawBatteryIcon(int16_t x, int16_t y, const char* batteryStr);
};
