#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "ui/UiAnimator.h"
#include "ui/UiCoordinator.h"
#include "ui/UiTypes.h"

class DisplayUi {
public:
    bool begin();
    bool setOrientation(rvf::UiOrientation orientation);
    rvf::UiOrientation orientation() const { return _orientation; }

    void render(const rvf::UiViewModel& view);
    LovyanGFX* beginLiveFrame();
    void renderLiveFrameOverlay(const rvf::UiViewModel& view);
    void finishLiveFrame(bool push);

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

    int16_t width() const { return _width; }
    int16_t height() const { return _height; }
    LovyanGFX* getCanvas() { return _canvasReady ? &_canvas : nullptr; }
    void pushCanvas();

private:
    bool createCanvasFor(rvf::UiOrientation orientation);
    void clear(uint16_t color = 0x0000);
    void drawBoot(const rvf::UiViewModel& view);
    void drawConnecting(const rvf::UiViewModel& view);
    void drawRemote(const rvf::UiViewModel& view);
    void drawReset(const rvf::UiViewModel& view);
    void drawSleep(const rvf::UiViewModel& view);
    void drawDisconnected(const rvf::UiViewModel& view);
    void drawError(const rvf::UiViewModel& view);
    void drawBatteryIndicator(const rvf::UiViewModel& view);
    void drawShutterOverlay(const rvf::UiViewModel& view);
    void drawCenteredText(const char* text, int16_t y, uint16_t color);
    uint32_t renderIntervalMs(rvf::UiScene scene) const;
    void updateBacklight(const rvf::UiViewModel& view);

    int16_t _width = 0;
    int16_t _height = 0;
    M5Canvas _canvas;
    rvf::UiOrientation _orientation = rvf::UiOrientation::Portrait;
    rvf::UiScene _lastScene = rvf::UiScene::Boot;
    rvf::BacklightAnimator _backlight;
    uint32_t _lastRenderAtMs = 0;
    uint32_t _orientationRetryAfterMs = 0;
    rvf::UiOrientation _failedOrientation = rvf::UiOrientation::Portrait;
    bool _canvasReady = false;
    bool _canvasUsePsram = false;
    bool _frameWriteActive = false;
    bool _sceneDrawn = false;
    bool _orientationFailurePending = false;
};
