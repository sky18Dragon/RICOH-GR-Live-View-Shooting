#include "RabbitUiRenderer.h"

#include <cstdio>
#include <cstring>

#include "RabbitBackground.h"
#include "RabbitLayout.h"
#include "RabbitTheme.h"
#include "RabbitUiProfile.h"

namespace rvf::ui {

const char* RabbitUiRenderer::safeText(const char* value, const char* fallback) {
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

void RabbitUiRenderer::drawBackground(LovyanGFX& canvas) {
    if constexpr (RabbitUiProfile::kShowRabbitBackground) {
        canvas.drawPng(kRabbitBackgroundPng,
                       kRabbitBackgroundPng_len,
                       0,
                       0,
                       RabbitLayout::kScreenW,
                       RabbitLayout::kScreenH);
    } else {
        canvas.fillScreen(RabbitTheme::kBackground);
    }
}

void RabbitUiRenderer::drawPanel(LovyanGFX& canvas,
                                 int16_t x,
                                 int16_t y,
                                 int16_t width,
                                 int16_t height,
                                 uint16_t fill) {
    const uint16_t panelColor = fill == 0 ? RabbitTheme::kPanel : fill;
    canvas.fillRoundRect(x + 1, y + 2, width, height, 6, RabbitTheme::kPanelShadow);
    canvas.fillRoundRect(x, y, width, height, 6, panelColor);
    canvas.drawRoundRect(x, y, width, height, 6, RabbitTheme::kBorder);
}

void RabbitUiRenderer::drawHeader(LovyanGFX& canvas, const char* title) {
    drawPanel(canvas,
              RabbitLayout::kSafeX,
              RabbitLayout::kHeaderY,
              RabbitLayout::kSafeW,
              RabbitLayout::kHeaderH,
              RabbitTheme::kPink);
    canvas.setTextSize(1);
    canvas.setTextColor(RabbitTheme::kText, RabbitTheme::kPink);
    canvas.setCursor(RabbitLayout::kSafeX + 8, RabbitLayout::kHeaderY + 5);
    canvas.print(title);
    canvas.fillRect(RabbitLayout::kSafeX + RabbitLayout::kSafeW - 15,
                    RabbitLayout::kHeaderY + 7,
                    2,
                    2,
                    RabbitTheme::kWhite);
    canvas.fillRect(RabbitLayout::kSafeX + RabbitLayout::kSafeW - 11,
                    RabbitLayout::kHeaderY + 7,
                    2,
                    2,
                    RabbitTheme::kWhite);
}

void RabbitUiRenderer::printTruncated(LovyanGFX& canvas,
                                      const char* text,
                                      int16_t x,
                                      int16_t y,
                                      size_t maxChars,
                                      uint16_t color,
                                      uint16_t background) {
    char buffer[24];
    const size_t limit = maxChars < sizeof(buffer) - 1 ? maxChars : sizeof(buffer) - 1;
    std::snprintf(buffer, sizeof(buffer), "%.*s", static_cast<int>(limit), safeText(text));
    canvas.setTextSize(1);
    canvas.setTextColor(color, background);
    canvas.setCursor(x, y);
    canvas.print(buffer);
}

void RabbitUiRenderer::drawStatusRow(LovyanGFX& canvas,
                                     int16_t y,
                                     const char* label,
                                     const char* value,
                                     bool active) {
    drawPanel(canvas, RabbitLayout::kSafeX, y, RabbitLayout::kSafeW, RabbitLayout::kRowH);
    canvas.setTextSize(1);
    canvas.setTextColor(RabbitTheme::kText, RabbitTheme::kPanel);
    canvas.setCursor(RabbitLayout::kSafeX + 6, y + 4);
    canvas.print(label);
    if (value != nullptr && value[0] != '\0') {
        printTruncated(canvas,
                       value,
                       RabbitLayout::kSafeX + 42,
                       y + 4,
                       7,
                       RabbitTheme::kMuted,
                       RabbitTheme::kPanel);
    }
    canvas.fillCircle(RabbitLayout::kSafeX + RabbitLayout::kSafeW - 9,
                      y + RabbitLayout::kRowH / 2,
                      3,
                      active ? RabbitTheme::kSuccess : RabbitTheme::kMuted);
}

void RabbitUiRenderer::drawKeyHint(LovyanGFX& canvas,
                                   int16_t x,
                                   char key,
                                   const char* label) {
    constexpr int16_t width = 52;
    drawPanel(canvas, x, RabbitLayout::kFooterY, width, 15);
    canvas.fillCircle(x + 8, RabbitLayout::kFooterY + 7, 5, RabbitTheme::kPinkDark);
    canvas.setTextSize(1);
    canvas.setTextColor(RabbitTheme::kWhite, RabbitTheme::kPinkDark);
    canvas.setCursor(x + 5, RabbitLayout::kFooterY + 4);
    canvas.print(key);
    canvas.setTextColor(RabbitTheme::kText, RabbitTheme::kPanel);
    canvas.setCursor(x + 16, RabbitLayout::kFooterY + 4);
    canvas.print(label);
}

void RabbitUiRenderer::drawWifiBars(LovyanGFX& canvas,
                                    int16_t x,
                                    int16_t y,
                                    bool connected,
                                    int32_t rssi) {
    uint8_t bars = 0;
    if (connected) {
        if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else bars = 1;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t height = static_cast<int16_t>(2 + i * 2);
        canvas.fillRect(x + static_cast<int16_t>(i * 3),
                        y + 8 - height,
                        2,
                        height,
                        i < bars ? RabbitTheme::kSuccess : RabbitTheme::kMuted);
    }
}

void RabbitUiRenderer::drawFocusBracket(LovyanGFX& canvas) {
    const int16_t left = (canvas.width() - RabbitLayout::kFocusW) / 2;
    const int16_t top = (canvas.height() - RabbitLayout::kFocusH) / 2;
    constexpr int16_t corner = 7;
    const uint16_t color = RabbitTheme::kPinkDark;
    canvas.drawFastHLine(left, top, corner, color);
    canvas.drawFastVLine(left, top, corner, color);
    canvas.drawFastHLine(left + RabbitLayout::kFocusW - corner, top, corner, color);
    canvas.drawFastVLine(left + RabbitLayout::kFocusW, top, corner, color);
    canvas.drawFastHLine(left, top + RabbitLayout::kFocusH, corner, color);
    canvas.drawFastVLine(left, top + RabbitLayout::kFocusH - corner, corner, color);
    canvas.drawFastHLine(left + RabbitLayout::kFocusW - corner,
                         top + RabbitLayout::kFocusH,
                         corner,
                         color);
    canvas.drawFastVLine(left + RabbitLayout::kFocusW,
                         top + RabbitLayout::kFocusH - corner,
                         corner,
                         color);
}

void RabbitUiRenderer::renderBoot(LovyanGFX& canvas, const UiModel& model) {
    drawBackground(canvas);
    drawHeader(canvas, "RABBIT GR");

    canvas.setTextSize(2);
    canvas.setTextColor(RabbitTheme::kText);
    canvas.setCursor(9, 32);
    canvas.print("GR LIVE");
    canvas.setTextSize(1);
    canvas.setTextColor(RabbitTheme::kPinkDark);
    canvas.setCursor(10, 53);
    canvas.print("BUNNY VIEW");

    drawPanel(canvas, 6, 69, 106, 14);
    printTruncated(canvas,
                   safeText(model.detail, "Booting..."),
                   12,
                   73,
                   14,
                   RabbitTheme::kText,
                   RabbitTheme::kPanel);

    drawPanel(canvas, 6, 89, 106, 13);
    canvas.fillRoundRect(10, 93, 65, 5, 2, RabbitTheme::kPinkDark);
    canvas.setTextSize(1);
    canvas.setTextColor(RabbitTheme::kText, RabbitTheme::kPanel);
    canvas.setCursor(82, 92);
    canvas.print("67%");

    canvas.setTextColor(RabbitTheme::kMuted);
    canvas.setCursor(8, 118);
    canvas.print("StickS3");
    canvas.setCursor(58, 118);
    canvas.print("FW 1.0");
}

void RabbitUiRenderer::renderStatus(LovyanGFX& canvas, const UiModel& model) {
    drawBackground(canvas);
    drawHeader(canvas, "RABBIT STATUS");

    const char* battery = safeText(model.battery);
    int16_t y = RabbitLayout::kRowY;
    constexpr int16_t stride = RabbitLayout::kRowH + RabbitLayout::kRowGap;
    drawStatusRow(canvas, y, "CAM", "LINK", model.bleConnected && model.wifiConnected);
    y += stride;
    drawStatusRow(canvas, y, "BLE", nullptr, model.bleConnected);
    y += stride;
    drawStatusRow(canvas, y, "WIFI", nullptr, model.wifiConnected);
    y += stride;
    drawStatusRow(canvas, y, "BAT", safeText(battery, "--"), battery[0] != '\0');
    y += stride;
    drawStatusRow(canvas, y, "READY", nullptr, model.previewRunning || model.shutterReady);

    drawKeyHint(canvas, 5, 'A', model.cameraStandby ? "WAKE" : "GO");
    drawKeyHint(canvas, 61, 'B', "RESET");
}

void RabbitUiRenderer::renderSettings(LovyanGFX& canvas, const UiModel& model) {
    drawBackground(canvas);
    drawHeader(canvas, "BUNNY SETUP");
    drawStatusRow(canvas, 32, "WIFI", model.wifiConnected ? "ON" : "OFF", model.wifiConnected);
    drawStatusRow(canvas, 51, "SHOT", "1/125", true);
    drawStatusRow(canvas, 70, "EV", "+/-0", true);
    drawStatusRow(canvas, 89, "SLEEP", "2 MIN", true);
    drawKeyHint(canvas, 5, 'A', "OK");
    drawKeyHint(canvas, 61, 'B', "BACK");
}

void RabbitUiRenderer::renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model) {
    const char* liveText = "LIVE";
    uint16_t liveColor = RabbitTheme::kPinkDark;
    if (model.shutterStatus == UiShutterStatus::Shooting) {
        liveText = "SHOOTING";
        liveColor = RabbitTheme::kWarning;
    } else if (model.shutterStatus == UiShutterStatus::Succeeded) {
        liveText = "SHOT OK";
        liveColor = RabbitTheme::kSuccess;
    } else if (model.shutterStatus == UiShutterStatus::Failed) {
        liveText = "SHOT FAIL";
        liveColor = RabbitTheme::kError;
    }

    canvas.fillRoundRect(3, 3, 69, 14, 6, RabbitTheme::kPanel);
    canvas.drawRoundRect(3, 3, 69, 14, 6, liveColor);
    canvas.setTextSize(1);
    canvas.setTextColor(liveColor, RabbitTheme::kPanel);
    canvas.setCursor(8, 7);
    canvas.print(liveText);
    if constexpr (RabbitUiProfile::kShowFps) {
        if (model.shutterStatus == UiShutterStatus::Idle) {
            char fps[8];
            std::snprintf(fps, sizeof(fps), "%.0f", static_cast<double>(model.fps));
            canvas.setCursor(53, 7);
            canvas.print(fps);
        }
    }

    canvas.fillRoundRect(82, 3, 76, 14, 6, RabbitTheme::kPink);
    canvas.drawRoundRect(82, 3, 76, 14, 6, RabbitTheme::kBorder);
    canvas.setTextColor(RabbitTheme::kText, RabbitTheme::kPink);
    canvas.setCursor(96, 7);
    canvas.print("RICOH GR");

    canvas.fillRoundRect(167, 3, 70, 14, 6, RabbitTheme::kPanel);
    canvas.drawRoundRect(167, 3, 70, 14, 6, RabbitTheme::kBorder);
    if constexpr (RabbitUiProfile::kShowWifiRssi) {
        drawWifiBars(canvas, 173, 6, model.wifiConnected, model.wifiRssi);
    }
    if constexpr (RabbitUiProfile::kShowBattery) {
        printTruncated(canvas,
                       safeText(model.battery, "--"),
                       193,
                       7,
                       6,
                       RabbitTheme::kText,
                       RabbitTheme::kPanel);
    }

    if constexpr (RabbitUiProfile::kShowFrameStats) {
        char stats[18];
        std::snprintf(stats,
                      sizeof(stats),
                      "%lu/%lu",
                      static_cast<unsigned long>(model.renderedFrames),
                      static_cast<unsigned long>(model.droppedFrames));
        canvas.setTextColor(RabbitTheme::kWhite);
        canvas.setCursor(5, 22);
        canvas.print(stats);
    }

    if constexpr (RabbitUiProfile::kShowFocusBracket) {
        drawFocusBracket(canvas);
    }
}

void RabbitUiRenderer::renderError(LovyanGFX& canvas, const UiModel& model) {
    drawBackground(canvas);
    drawHeader(canvas, "BUNNY ERROR");
    drawPanel(canvas, 5, 31, 108, 67);
    canvas.setTextSize(2);
    canvas.setTextColor(RabbitTheme::kError, RabbitTheme::kPanel);
    canvas.setCursor(12, 37);
    canvas.print("OH NO");
    printTruncated(canvas,
                   safeText(model.errorMessage, "Unknown error"),
                   11,
                   61,
                   15,
                   RabbitTheme::kText,
                   RabbitTheme::kPanel);
    printTruncated(canvas,
                   safeText(model.detail),
                   11,
                   77,
                   15,
                   RabbitTheme::kMuted,
                   RabbitTheme::kPanel);
    drawKeyHint(canvas, 5, 'A', "RETRY");
    drawKeyHint(canvas, 61, 'B', "RESET");
}

void RabbitUiRenderer::renderShutdown(LovyanGFX& canvas, const UiModel& model) {
    drawBackground(canvas);
    drawHeader(canvas, "RABBIT GR");
    drawPanel(canvas, 5, 35, 108, 54);
    canvas.setTextSize(2);
    canvas.setTextColor(RabbitTheme::kPinkDark, RabbitTheme::kPanel);
    canvas.setCursor(11, 43);
    canvas.print("GOOD");
    canvas.setCursor(11, 61);
    canvas.print("NIGHT");
    printTruncated(canvas,
                   safeText(model.detail, "Powering off"),
                   8,
                   101,
                   16,
                   RabbitTheme::kMuted,
                   RabbitTheme::kBackground);
}

}  // namespace rvf::ui
