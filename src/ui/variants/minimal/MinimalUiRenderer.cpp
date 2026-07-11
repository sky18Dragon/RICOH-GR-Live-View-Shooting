#include "MinimalUiRenderer.h"

#include <cstdio>
#include <cstring>

#include "MinimalLayout.h"
#include "MinimalTheme.h"
#include "MinimalUiProfile.h"

namespace rvf::ui {

namespace {

const char* safeText(const char* text, const char* fallback = "") {
    return text != nullptr && text[0] != '\0' ? text : fallback;
}

void drawHeader(LovyanGFX& canvas, const char* title, uint16_t color) {
    canvas.setTextSize(1);
    canvas.setTextColor(color, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, MinimalLayout::kHeaderY);
    canvas.print(title);
    canvas.drawFastHLine(MinimalLayout::kMargin,
                         24,
                         canvas.width() - (MinimalLayout::kMargin * 2),
                         color);
}

}  // namespace

void MinimalUiRenderer::renderBoot(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(MinimalTheme::kBackground);
    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setTextSize(2);
    canvas.setCursor(22, 34);
    canvas.print("RICOH VIEWFINDER");
    canvas.setTextSize(1);
    canvas.setTextColor(MinimalTheme::kMuted, MinimalTheme::kBackground);
    canvas.setCursor(72, 70);
    canvas.print(safeText(model.detail, "BOOTING"));
}

void MinimalUiRenderer::renderStatus(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(MinimalTheme::kBackground);
    drawHeader(canvas, "RICOH VIEWFINDER", MinimalTheme::kPrimary);

    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, MinimalLayout::kBodyY);
    canvas.print(uiPhaseName(model.phase));

    canvas.setCursor(MinimalLayout::kMargin, MinimalLayout::kBodyY + MinimalLayout::kLineHeight);
    canvas.print("BLE  ");
    canvas.setTextColor(model.bleConnected ? MinimalTheme::kSuccess : MinimalTheme::kMuted,
                        MinimalTheme::kBackground);
    canvas.print(model.bleConnected ? "CONNECTED" : "OFFLINE");

    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, MinimalLayout::kBodyY + (MinimalLayout::kLineHeight * 2));
    canvas.print("WIFI ");
    canvas.setTextColor(model.wifiConnected ? MinimalTheme::kSuccess : MinimalTheme::kMuted,
                        MinimalTheme::kBackground);
    canvas.print(model.wifiConnected ? "CONNECTED" : "OFFLINE");

    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, MinimalLayout::kBodyY + (MinimalLayout::kLineHeight * 3));
    if (model.cameraName != nullptr && model.cameraName[0] != '\0') {
        canvas.print(model.cameraName);
    } else if constexpr (MinimalUiProfile::kShowCameraModel) {
        canvas.print(safeText(model.cameraModel, "RICOH CAMERA"));
    } else {
        canvas.print("RICOH CAMERA");
    }
}

void MinimalUiRenderer::renderSettings(LovyanGFX& canvas, const UiModel& model) {
    (void)model;
    canvas.fillScreen(MinimalTheme::kBackground);
    drawHeader(canvas, "QUICK CONTROLS", MinimalTheme::kPrimary);
    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, 38);
    canvas.print("SHUTTER    1/125s");
    canvas.setCursor(MinimalLayout::kMargin, 54);
    canvas.print("FILTER     STANDARD");
    canvas.setCursor(MinimalLayout::kMargin, 70);
    canvas.print("EXPOSURE   +/-0.0 EV");
    canvas.setCursor(MinimalLayout::kMargin, 86);
    canvas.print("WIFI       ON");
    canvas.setTextColor(MinimalTheme::kMuted, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, 110);
    canvas.print("STATIC PREVIEW");
}

void MinimalUiRenderer::renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model) {
    if constexpr (MinimalUiProfile::kShowFocusBracket) {
        drawFocusBracket(canvas);
    }

    canvas.setTextSize(1);
    canvas.setTextColor(MinimalTheme::kPrimary);
    if (model.shutterStatus != UiShutterStatus::Idle) {
        const char* shutterText = "SHOOTING";
        uint16_t shutterColor = MinimalTheme::kWarning;
        if (model.shutterStatus == UiShutterStatus::Succeeded) {
            shutterText = "SHOT OK";
            shutterColor = MinimalTheme::kSuccess;
        } else if (model.shutterStatus == UiShutterStatus::Failed) {
            shutterText = "SHOT FAILED";
            shutterColor = MinimalTheme::kError;
        }
        canvas.setTextColor(shutterColor);
        canvas.setCursor(88, 8);
        canvas.print(shutterText);
        canvas.setTextColor(MinimalTheme::kPrimary);
    }
    if constexpr (MinimalUiProfile::kShowBattery) {
        canvas.setCursor(8, 8);
        canvas.print(safeText(model.battery, "BAT --"));
    }

    if constexpr (MinimalUiProfile::kShowWifiRssi) {
        drawWifiBars(canvas, canvas.width() - 22, 7, model.wifiRssi);
    }

    if constexpr (MinimalUiProfile::kShowCameraModel) {
        canvas.setCursor(8, canvas.height() - 14);
        canvas.print(safeText(model.cameraModel, "RICOH GR"));
    }

    if constexpr (MinimalUiProfile::kShowFps) {
        char fps[14];
        std::snprintf(fps, sizeof(fps), "%.1f FPS", static_cast<double>(model.fps));
        canvas.setCursor(canvas.width() - static_cast<int16_t>(std::strlen(fps) * 6) - 8,
                         canvas.height() - 14);
        canvas.print(fps);
    }

    if constexpr (MinimalUiProfile::kShowFrameStats) {
        char stats[24];
        std::snprintf(stats,
                      sizeof(stats),
                      "%lu/%lu",
                      static_cast<unsigned long>(model.renderedFrames),
                      static_cast<unsigned long>(model.droppedFrames));
        canvas.setCursor(8, 22);
        canvas.print(stats);
    }
}

void MinimalUiRenderer::renderError(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(MinimalTheme::kBackground);
    drawHeader(canvas, "ERROR", MinimalTheme::kError);
    canvas.setTextColor(MinimalTheme::kError, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, 42);
    canvas.print(safeText(model.errorMessage, "Unknown error"));
    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setCursor(MinimalLayout::kMargin, 66);
    canvas.print(safeText(model.detail));
    canvas.setCursor(MinimalLayout::kMargin, 105);
    canvas.print("BtnA: retry");
}

void MinimalUiRenderer::renderShutdown(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(MinimalTheme::kBackground);
    canvas.setTextSize(2);
    canvas.setTextColor(MinimalTheme::kPrimary, MinimalTheme::kBackground);
    canvas.setCursor(42, 44);
    canvas.print("POWERING OFF");
    canvas.setTextSize(1);
    canvas.setCursor(70, 78);
    canvas.print(safeText(model.detail, "Please wait"));
}

void MinimalUiRenderer::drawFocusBracket(LovyanGFX& canvas) {
    const int16_t cx = canvas.width() / 2;
    const int16_t cy = canvas.height() / 2;
    const int16_t halfWidth = MinimalLayout::kFocusHalfWidth;
    const int16_t halfHeight = MinimalLayout::kFocusHalfHeight;
    canvas.drawRect(cx - halfWidth,
                    cy - halfHeight,
                    halfWidth * 2,
                    halfHeight * 2,
                    MinimalTheme::kSuccess);
}

void MinimalUiRenderer::drawWifiBars(LovyanGFX& canvas, int16_t x, int16_t y, int32_t rssi) {
    uint8_t bars = 0;
    if (rssi < 0) {
        if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else bars = 1;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t height = 2 + (i * 2);
        canvas.fillRect(x + (i * 3),
                        y + 8 - height,
                        2,
                        height,
                        i < bars ? MinimalTheme::kSuccess : MinimalTheme::kMuted);
    }
}

}  // namespace rvf::ui
