#include "RicohUiRenderer.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "RicohLayout.h"
#include "RicohTheme.h"
#include "RicohUiProfile.h"

namespace rvf::ui {

namespace {

const char* safeText(const char* value, const char* fallback = "") {
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

const char* connectionLabel(const UiModel& model) {
    switch (model.shutterStatus) {
        case UiShutterStatus::Shooting: return "BLE SHOOTING...";
        case UiShutterStatus::Succeeded: return "BLE SHOT OK";
        case UiShutterStatus::Failed: return "BLE SHOT FAILED";
        case UiShutterStatus::Idle: break;
    }
    if (model.cameraStandby || model.phase == UiPhase::CameraStandby) {
        return "BLE PAUSED";
    }
    switch (model.phase) {
        case UiPhase::BleConnecting:
            return "BLE CONNECTING...";
        case UiPhase::BleConnected:
        case UiPhase::CheckingCameraPower:
        case UiPhase::ActivatingWifi:
        case UiPhase::WifiConnecting:
        case UiPhase::WifiConnected:
        case UiPhase::HttpProbing:
        case UiPhase::StartingPreview:
        case UiPhase::PreviewRunning:
        case UiPhase::Shooting:
            return "BLE CONNECTED";
        case UiPhase::Recovering:
            return model.bleConnected ? "BLE CONNECTED" : "BLE SEARCHING...";
        case UiPhase::PairingReset:
            return "BLE SEARCHING...";
        case UiPhase::Error:
            return "BLE PAUSED";
        case UiPhase::Booting:
        case UiPhase::BleScanning:
            return "BLE SEARCHING...";
        case UiPhase::CameraStandby:
            return "BLE PAUSED";
    }
    return "BLE SEARCHING...";
}

}  // namespace

void RicohUiRenderer::renderBoot(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(RicohTheme::kBackground);
    drawGeometricCard(canvas);

    canvas.setTextColor(RicohTheme::kPrimary);
    canvas.setTextSize(3);
    canvas.setCursor(102, 20);
    canvas.print("GR");

    canvas.setTextSize(1);
    canvas.setTextColor(RicohTheme::kText);
    canvas.setCursor(84, 52);
    canvas.print("VIEWFINDER");

    canvas.drawRoundRect(40, 70, 160, 6, 3, RicohTheme::kGraphite);
    canvas.fillRoundRect(42, 72, 60, 2, 1, RicohTheme::kPrimary);

    const char* message = safeText(model.detail, "Booting...");
    const size_t length = std::strlen(message);
    canvas.setTextColor(RicohTheme::kGray);
    canvas.setCursor(static_cast<int16_t>((canvas.width() - static_cast<int16_t>(length * 6)) / 2), 86);
    canvas.print(message);

    canvas.drawFastHLine(20, canvas.height() - 24, canvas.width() - 40, RicohTheme::kSlate);
    canvas.setCursor(60, canvas.height() - 16);
    canvas.print("BtnA: Shutter / Retry");
}

void RicohUiRenderer::renderStatus(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(RicohTheme::kBackground);
    drawGeometricCard(canvas);

    const int16_t cardHeight = canvas.height() - (RicohLayout::kCardVerticalInset * 2);
    const char* title = "GR IV";
    canvas.setTextSize(2);
    canvas.setTextColor(RicohTheme::kText, RicohTheme::kCard);
    const int16_t titleWidth = static_cast<int16_t>(std::strlen(title) * 12);
    const int16_t titleX = static_cast<int16_t>((canvas.width() - titleWidth) / 2 - 4);
    canvas.setCursor(titleX, RicohLayout::kCardY + 14);
    canvas.print(title);
    canvas.fillCircle(titleX + titleWidth + 11,
                      RicohLayout::kCardY + 21,
                      4,
                      model.bleConnected ? RicohTheme::kSuccess : RicohTheme::kError);

    const char* status = connectionLabel(model);
    const int16_t statusWidth = static_cast<int16_t>(std::strlen(status) * 12);
    const int16_t statusX = static_cast<int16_t>((canvas.width() - statusWidth) / 2);
    canvas.setCursor(statusX, RicohLayout::kCardY + cardHeight - 29);
    canvas.print(status);
}

void RicohUiRenderer::renderSettings(LovyanGFX& canvas, const UiModel& model) {
    (void)model;
    canvas.fillScreen(RicohTheme::kBackground);
    canvas.fillRoundRect(8, 6, canvas.width() - 16, canvas.height() - 12, 10, RicohTheme::kCard);
    canvas.drawRoundRect(8, 6, canvas.width() - 16, canvas.height() - 12, 10, RicohTheme::kGraphite);
    canvas.setTextSize(1);
    canvas.setTextColor(RicohTheme::kPrimary, RicohTheme::kCard);
    canvas.setCursor(14, 13);
    canvas.print("QUICK CONTROLS");
    canvas.drawFastHLine(14, 25, canvas.width() - 28, RicohTheme::kSlate);
    canvas.setTextColor(RicohTheme::kText, RicohTheme::kCard);
    canvas.setCursor(18, 36);
    canvas.print("SHUTTER       1/125s");
    canvas.setCursor(18, 52);
    canvas.print("FILTER        STANDARD");
    canvas.setCursor(18, 68);
    canvas.print("EXPOSURE      +/-0.0 EV");
    canvas.setCursor(18, 84);
    canvas.print("WIFI          ON");
    canvas.setTextColor(RicohTheme::kGray, RicohTheme::kCard);
    canvas.setCursor(18, 108);
    canvas.print("Static preview / no navigation");
}

void RicohUiRenderer::renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model) {
    const int16_t vx = RicohLayout::kViewportX;
    const int16_t vy = RicohLayout::kViewportY;
    const int16_t vw = RicohLayout::kViewportWidth;
    const int16_t vh = RicohLayout::kViewportHeight;
    const int16_t mark = RicohLayout::kCropMarkLength;

    canvas.drawFastHLine(vx, vy, mark, RicohTheme::kText);
    canvas.drawFastVLine(vx, vy, mark, RicohTheme::kText);
    canvas.drawFastHLine(vx + vw - mark, vy, mark, RicohTheme::kText);
    canvas.drawFastVLine(vx + vw - 1, vy, mark, RicohTheme::kText);
    canvas.drawFastHLine(vx, vy + vh - 1, mark, RicohTheme::kText);
    canvas.drawFastVLine(vx, vy + vh - mark, mark, RicohTheme::kText);
    canvas.drawFastHLine(vx + vw - mark, vy + vh - 1, mark, RicohTheme::kText);
    canvas.drawFastVLine(vx + vw - 1, vy + vh - mark, mark, RicohTheme::kText);

    if constexpr (RicohUiProfile::kShowFocusBracket) {
        drawFocusBracket(canvas);
    }

    canvas.setTextSize(1);
    const char* liveLabel = model.previewRunning ? "LIVE" : "IDLE";
    uint16_t liveColor = model.previewRunning ? RicohTheme::kSuccess : RicohTheme::kWarning;
    switch (model.shutterStatus) {
        case UiShutterStatus::Shooting:
            liveLabel = "SHOOT";
            liveColor = RicohTheme::kWarning;
            break;
        case UiShutterStatus::Succeeded:
            liveLabel = "SHOT OK";
            liveColor = RicohTheme::kSuccess;
            break;
        case UiShutterStatus::Failed:
            liveLabel = "SHOT ERR";
            liveColor = RicohTheme::kError;
            break;
        case UiShutterStatus::Idle:
            break;
    }
    canvas.fillCircle(14, 10, 3, liveColor);
    canvas.setTextColor(RicohTheme::kText);
    canvas.setCursor(22, 6);
    canvas.print(liveLabel);

    if constexpr (RicohUiProfile::kShowFps) {
        char fpsText[16];
        if (model.fps >= 0.0f) {
            std::snprintf(fpsText, sizeof(fpsText), "%.1f FPS", static_cast<double>(model.fps));
        } else {
            std::snprintf(fpsText, sizeof(fpsText), "-- FPS");
        }
        canvas.setTextColor(RicohTheme::kGray);
        canvas.setCursor(14, 18);
        canvas.print(fpsText);
    }

    if constexpr (RicohUiProfile::kShowCameraModel) {
        char modelText[9];
        std::snprintf(modelText, sizeof(modelText), "%.8s", safeText(model.cameraModel, "RICOH GR"));
        canvas.setTextColor(RicohTheme::kText);
        canvas.setCursor(14, canvas.height() - 24);
        canvas.print(modelText);
    }

    if constexpr (RicohUiProfile::kShowBattery) {
        drawBatteryIcon(canvas, 14, canvas.height() - 12, model.battery);
    }

    if constexpr (RicohUiProfile::kShowWifiRssi) {
        drawWifiIcon(canvas, canvas.width() - 24, canvas.height() - 12, model.wifiRssi);
    }

    if constexpr (RicohUiProfile::kShowFrameStats) {
        char statsText[24];
        std::snprintf(statsText,
                      sizeof(statsText),
                      "%lu/%lu",
                      static_cast<unsigned long>(model.renderedFrames),
                      static_cast<unsigned long>(model.droppedFrames));
        canvas.setTextColor(model.droppedFrames == 0 ? RicohTheme::kText : RicohTheme::kWarning);
        canvas.setCursor(canvas.width() - static_cast<int16_t>(std::strlen(statsText) * 6) - 14, 6);
        canvas.print(statsText);
    }
}

void RicohUiRenderer::renderError(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(RicohTheme::kBackground);
    canvas.drawFastHLine(10, 24, canvas.width() - 20, RicohTheme::kSlate);
    canvas.setTextSize(1);
    canvas.setTextColor(RicohTheme::kError, RicohTheme::kBackground);
    canvas.setCursor(10, 8);
    canvas.print("SYSTEM ERROR");
    canvas.drawRoundRect(10, 32, canvas.width() - 20, 78, 4, RicohTheme::kError);
    canvas.setCursor(20, 42);
    canvas.print(safeText(model.errorMessage, "Unknown error"));
    if (model.detail != nullptr && model.detail[0] != '\0') {
        canvas.setTextColor(RicohTheme::kText, RicohTheme::kBackground);
        canvas.setCursor(20, 62);
        canvas.print(model.detail);
    }
    canvas.drawFastHLine(10, canvas.height() - 20, canvas.width() - 20, RicohTheme::kSlate);
    canvas.setTextColor(RicohTheme::kText, RicohTheme::kBackground);
    canvas.setCursor(50, canvas.height() - 14);
    canvas.print("Press BtnA to retry");
}

void RicohUiRenderer::renderShutdown(LovyanGFX& canvas, const UiModel& model) {
    // Preserve the established boot-card treatment for both shutdown prompts.
    renderBoot(canvas, model);
}

void RicohUiRenderer::drawGeometricCard(LovyanGFX& canvas) {
    const int16_t x = RicohLayout::kCardX;
    const int16_t y = RicohLayout::kCardY;
    const int16_t width = canvas.width() - (RicohLayout::kCardInset * 2);
    const int16_t height = canvas.height() - (RicohLayout::kCardVerticalInset * 2);

    canvas.fillRoundRect(x, y, width, height, RicohLayout::kCardRadius, RicohTheme::kCard);
    canvas.drawRoundRect(x, y, width, height, RicohLayout::kCardRadius, RicohTheme::kGraphite);
    canvas.drawRoundRect(x + 2, y + 2, width - 4, height - 4, 8, RicohTheme::kSlate);
    canvas.fillTriangle(x + 10, y + 16, x + 94, y + 16, x + 45, y + 106, RicohTheme::kPanel);
    canvas.fillTriangle(x + 92, y + 18, x + 142, y + 68, x + 98, y + 112, RicohTheme::kDark);
    canvas.fillTriangle(x + 120, y + 60, x + 205, y + 16, x + 202, y + 108, RicohTheme::kPanel);
    canvas.fillTriangle(x + 118, y + 62, x + 166, y + 112, x + 210, y + 80, RicohTheme::kSlate);
    canvas.fillTriangle(x + 142, y + 72, x + 184, y + 34, x + 216, y + 74, RicohTheme::kDark);
    canvas.fillTriangle(x + 72, y + 24, x + 108, y + 110, x + 56, y + 112, RicohTheme::kDark);

    const int16_t hx1 = x + 14;
    const int16_t hy1 = y + 28;
    const int16_t hx2 = x + 214;
    const int16_t hy2 = y + 86;
    constexpr int16_t band = 12;
    canvas.fillTriangle(hx1, hy1, hx2, hy2, hx2, hy2 + band, RicohTheme::kHighlight);
    canvas.fillTriangle(hx1, hy1, hx2, hy2 + band, hx1, hy1 + band, RicohTheme::kHighlight);
    canvas.fillTriangle(x + 150, y + 78, x + 184, y + 34, x + 216, y + 80, RicohTheme::kDark);
    canvas.fillTriangle(x + 146, y + 82, x + 210, y + 82, x + 134, y + 112, RicohTheme::kDark);
}

void RicohUiRenderer::drawWifiIcon(LovyanGFX& canvas, int16_t x, int16_t y, int32_t rssi) {
    uint8_t bars = 0;
    if (rssi < 0) {
        if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else bars = 1;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const uint16_t color = i < bars ? RicohTheme::kSuccess : RicohTheme::kSlate;
        const int16_t barHeight = 2 + (i * 2);
        canvas.fillRect(x + (i * 3), y + (8 - barHeight), 2, barHeight, color);
    }
}

void RicohUiRenderer::drawBatteryIcon(LovyanGFX& canvas, int16_t x, int16_t y, const char* battery) {
    int percent = -1;
    const char* text = safeText(battery);
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
            if (percent < 0) percent = 0;
            percent = percent * 10 + (text[i] - '0');
        } else if (text[i] == '%' || text[i] == ' ') {
            break;
        }
    }

    canvas.drawRect(x, y, 14, 8, RicohTheme::kText);
    canvas.fillRect(x + 14, y + 2, 1, 4, RicohTheme::kText);
    if (percent < 0) {
        canvas.drawLine(x + 2, y + 2, x + 11, y + 5, RicohTheme::kGray);
        return;
    }

    if (percent > 100) percent = 100;
    const int fillWidth = (percent * 10) / 100;
    uint16_t color = RicohTheme::kSuccess;
    if (percent < 20) color = RicohTheme::kError;
    else if (percent < 50) color = RicohTheme::kWarning;
    canvas.fillRect(x + 2, y + 2, fillWidth, 4, color);
}

void RicohUiRenderer::drawFocusBracket(LovyanGFX& canvas) {
    const int16_t cx = canvas.width() / 2;
    const int16_t cy = canvas.height() / 2;
    const int16_t halfWidth = RicohLayout::kFocusHalfWidth;
    const int16_t halfHeight = RicohLayout::kFocusHalfHeight;
    canvas.drawFastVLine(cx - halfWidth, cy - halfHeight, halfHeight * 2, RicohTheme::kSuccess);
    canvas.drawFastHLine(cx - halfWidth, cy - halfHeight, 4, RicohTheme::kSuccess);
    canvas.drawFastHLine(cx - halfWidth, cy + halfHeight - 1, 4, RicohTheme::kSuccess);
    canvas.drawFastVLine(cx + halfWidth - 1, cy - halfHeight, halfHeight * 2, RicohTheme::kSuccess);
    canvas.drawFastHLine(cx + halfWidth - 4, cy - halfHeight, 4, RicohTheme::kSuccess);
    canvas.drawFastHLine(cx + halfWidth - 4, cy + halfHeight - 1, 4, RicohTheme::kSuccess);
}

}  // namespace rvf::ui
