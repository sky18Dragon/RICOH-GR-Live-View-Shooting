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
    uint8_t toggleRotation();
    bool toggleMirror();
    uint8_t rotation() const { return _rotation; }
    bool mirrored() const { return _mirrored; }

    LovyanGFX* getCanvas() { return &_canvas; }
    void pushCanvas();

private:
    int16_t _width = DISPLAY_WIDTH;
    int16_t _height = DISPLAY_HEIGHT;
    uint8_t _rotation = 1;
    bool _mirrored = false;
    M5Canvas _canvas;

    void clear(uint16_t color = 0x0000);
    void drawStatusLines(const char* line1, const char* line2, const char* line3, const char* line4 = nullptr);
    void drawDeviceBatteryStatus(int16_t rightX, int16_t y);
    void drawWifiIcon(int16_t x, int16_t y, int32_t rssi);
    void drawBatteryIcon(int16_t x, int16_t y, const char* batteryStr);
};
