#pragma once

#include <cstddef>

#include <M5Unified.h>

#include "../../model/UiModel.h"

namespace rvf::ui {

class KawaiiUiRenderer {
public:
    void renderBoot(LovyanGFX& canvas, const UiModel& model);
    void renderStatus(LovyanGFX& canvas, const UiModel& model);
    void renderSettings(LovyanGFX& canvas, const UiModel& model);
    void renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model);
    void renderError(LovyanGFX& canvas, const UiModel& model);
    void renderShutdown(LovyanGFX& canvas, const UiModel& model);

private:
    static const char* safeText(const char* value, const char* fallback = "");
    static const char* bleStatusText(const UiModel& model);
    static const char* wifiStatusText(const UiModel& model);
    static const char* signalText(const UiModel& model);

    static void printCentered(LovyanGFX& canvas,
                              const char* text,
                              int16_t y,
                              uint8_t textSize,
                              uint16_t color,
                              uint16_t background);
    static void printTruncated(LovyanGFX& canvas,
                               const char* text,
                               int16_t x,
                               int16_t y,
                               size_t maxChars,
                               uint16_t color,
                               uint16_t background);

    static void drawKawaiiBlob(LovyanGFX& canvas,
                               int16_t x,
                               int16_t y,
                               int16_t radius,
                               uint16_t color);
    static void drawPatternBackground(LovyanGFX& canvas);
    static void drawOuterShell(LovyanGFX& canvas);
    static void drawHeader(LovyanGFX& canvas, const char* title);
    static void drawFooterPanel(LovyanGFX& canvas);
    static void drawPaw(LovyanGFX& canvas, int16_t x, int16_t y, uint16_t color);
    static void drawHeart(LovyanGFX& canvas, int16_t x, int16_t y, uint16_t color);
    static void drawMascot(LovyanGFX& canvas,
                           int16_t centerX,
                           int16_t centerY,
                           bool mirrored,
                           bool crying);
    static void drawMiniMascot(LovyanGFX& canvas, int16_t centerX, int16_t centerY);
    static void drawWifiBadge(LovyanGFX& canvas,
                              int16_t x,
                              int16_t y,
                              bool connected,
                              int32_t rssi);
    static void drawBatteryBadge(LovyanGFX& canvas,
                                 int16_t x,
                                 int16_t y,
                                 const char* battery);
    static void drawStatusRow(LovyanGFX& canvas,
                              int16_t y,
                              const char* label,
                              const char* value,
                              bool ok);
    static void drawSettingTile(LovyanGFX& canvas,
                                int16_t x,
                                int16_t y,
                                const char* label,
                                const char* value);
    static void drawLiveBadge(LovyanGFX& canvas, const UiModel& model);
    static void drawFocusBracket(LovyanGFX& canvas);
};

}  // namespace rvf::ui
