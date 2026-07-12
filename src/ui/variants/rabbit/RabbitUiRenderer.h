#pragma once

#include <cstddef>

#include <M5Unified.h>

#include "../../model/UiModel.h"

namespace rvf::ui {

class RabbitUiRenderer {
public:
    void renderBoot(LovyanGFX& canvas, const UiModel& model);
    void renderStatus(LovyanGFX& canvas, const UiModel& model);
    void renderSettings(LovyanGFX& canvas, const UiModel& model);
    void renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model);
    void renderError(LovyanGFX& canvas, const UiModel& model);
    void renderShutdown(LovyanGFX& canvas, const UiModel& model);

private:
    static const char* safeText(const char* value, const char* fallback = "");
    static void drawBackground(LovyanGFX& canvas);
    static void drawPanel(LovyanGFX& canvas,
                          int16_t x,
                          int16_t y,
                          int16_t width,
                          int16_t height,
                          uint16_t fill = 0);
    static void drawHeader(LovyanGFX& canvas, const char* title);
    static void printTruncated(LovyanGFX& canvas,
                               const char* text,
                               int16_t x,
                               int16_t y,
                               size_t maxChars,
                               uint16_t color,
                               uint16_t background);
    static void drawStatusRow(LovyanGFX& canvas,
                              int16_t y,
                              const char* label,
                              const char* value,
                              bool active);
    static void drawKeyHint(LovyanGFX& canvas,
                            int16_t x,
                            char key,
                            const char* label);
    static void drawWifiBars(LovyanGFX& canvas,
                             int16_t x,
                             int16_t y,
                             bool connected,
                             int32_t rssi);
    static void drawFocusBracket(LovyanGFX& canvas);
};

}  // namespace rvf::ui
