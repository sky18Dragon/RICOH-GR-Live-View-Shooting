#include "KawaiiUiRenderer.h"

#include <cstdio>
#include <cstring>

#include "KawaiiAssets.h"
#include "KawaiiBootBackground.h"
#include "KawaiiLayout.h"
#include "KawaiiStatusBackground.h"
#include "KawaiiTheme.h"
#include "KawaiiUiProfile.h"

namespace rvf::ui {

void KawaiiUiRenderer::renderBoot(LovyanGFX& canvas, const UiModel& model) {
    if constexpr (KawaiiUiProfile::kShowMascots &&
                  KawaiiUiProfile::kShowPatternBackground) {
        canvas.drawJpg(kKawaiiBootBackgroundJpeg,
                       kKawaiiBootBackgroundJpeg_len,
                       0,
                       0,
                       KawaiiLayout::kScreenW,
                       KawaiiLayout::kScreenH);
    } else {
        drawPatternBackground(canvas);
        if constexpr (KawaiiUiProfile::kShowMascots) {
            drawMascot(canvas, 211, 72, true, true);
        }
    }
    drawHeader(canvas, "RICOH GR");

    canvas.setTextSize(2);
    canvas.setTextColor(KawaiiTheme::kWhite);
    canvas.setCursor(KawaiiLayout::kBootTitleX, KawaiiLayout::kBootTitleY);
    canvas.print("GR Live View");
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText);
    canvas.setCursor(KawaiiLayout::kBootThemeX, KawaiiLayout::kBootThemeY);
    canvas.print(KawaiiAssets::kThemeLabel);
    drawHeart(canvas, KawaiiLayout::kBootThemeX - 10,
              KawaiiLayout::kBootThemeY, KawaiiTheme::kAccentGlow);
    drawHeart(canvas, KawaiiLayout::kBootThemeX + 82,
              KawaiiLayout::kBootThemeY, KawaiiTheme::kAccentGlow);

    constexpr int16_t progressX = KawaiiLayout::kBootProgressX;
    constexpr int16_t progressY = KawaiiLayout::kBootProgressY;
    constexpr int16_t progressW = KawaiiLayout::kBootProgressW;
    constexpr int16_t progressH = KawaiiLayout::kBootProgressH;
    constexpr int16_t progressPercent = 67;
    canvas.fillRoundRect(progressX + 1, progressY + 2, progressW, progressH, 6,
                         KawaiiTheme::kPanelShadow);
    canvas.fillRoundRect(progressX, progressY, progressW, progressH, 6,
                         KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(progressX, progressY, progressW, progressH, 6,
                         KawaiiTheme::kPrimary);
    const int16_t fillWidth = static_cast<int16_t>(((progressW - 6) * progressPercent) / 100);
    canvas.fillRoundRect(progressX + 3, progressY + 3, fillWidth, progressH - 6, 3,
                         KawaiiTheme::kPrimary);
    canvas.drawFastHLine(progressX + 7, progressY + 4,
                         fillWidth > 12 ? fillWidth - 10 : 2,
                         KawaiiTheme::kAccentGlow);

    char bootText[12];
    std::snprintf(bootText, sizeof(bootText), "%.11s", safeText(model.detail, "Booting..."));
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kWhite);
    canvas.setCursor(progressX + 8, progressY + 3);
    canvas.print(bootText);
    canvas.setTextColor(KawaiiTheme::kPrimary, KawaiiTheme::kPanelSoft);
    canvas.setCursor(progressX + progressW - 24, progressY + 3);
    canvas.print("67%");

    drawFooterPanel(canvas);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(8, 121);
    canvas.print("StickS3");
    canvas.setTextColor(KawaiiTheme::kTextSoft, KawaiiTheme::kPanelSoft);
    canvas.setCursor(54, 121);
    canvas.print(KawaiiAssets::kFirmwareLabel);
    drawPaw(canvas, 121, 124, KawaiiTheme::kPrimary);
    if constexpr (KawaiiUiProfile::kShowWifiRssi) {
        drawWifiBadge(canvas, 183, 120, model.wifiConnected, model.wifiRssi);
    }
    if constexpr (KawaiiUiProfile::kShowBattery) {
        drawBatteryBadge(canvas, 215, 120, model.battery);
    }
}

void KawaiiUiRenderer::renderStatus(LovyanGFX& canvas, const UiModel& model) {
    // The approved illustration is pre-rendered at the native panel size. UI
    // controls remain code-drawn below so all lamps and labels stay dynamic.
    if constexpr (KawaiiUiProfile::kShowMascots) {
        canvas.drawJpg(kKawaiiStatusBackgroundJpeg,
                       kKawaiiStatusBackgroundJpeg_len,
                       0,
                       0,
                       KawaiiLayout::kScreenW,
                       KawaiiLayout::kScreenH);
    } else {
        drawPatternBackground(canvas);
    }
    drawHeader(canvas, "RICOH GR");

    canvas.fillRoundRect(KawaiiLayout::kStatusX + 1,
                         KawaiiLayout::kStatusY + 2,
                         KawaiiLayout::kStatusW,
                         KawaiiLayout::kStatusH,
                         9,
                         KawaiiTheme::kPanelShadow);
    canvas.fillRoundRect(KawaiiLayout::kStatusX,
                         KawaiiLayout::kStatusY,
                         KawaiiLayout::kStatusW,
                         KawaiiLayout::kStatusH,
                         9,
                         KawaiiTheme::kPanel);
    canvas.drawRoundRect(KawaiiLayout::kStatusX,
                         KawaiiLayout::kStatusY,
                         KawaiiLayout::kStatusW,
                         KawaiiLayout::kStatusH,
                         9,
                         KawaiiTheme::kPanelBorder);

    const char* battery = safeText(model.battery);
    const bool connectionOk = model.bleConnected && model.wifiConnected;
    const bool batteryKnown = battery[0] != '\0';
    const bool cameraReady = model.previewRunning || model.shutterReady;
    constexpr int16_t rowStride = KawaiiLayout::kStatusRowH + KawaiiLayout::kStatusRowGap;
    int16_t rowY = KawaiiLayout::kStatusRowY;
    drawCompactStatusRow(canvas, rowY, StatusGlyph::Camera, connectionOk);
    rowY += rowStride;
    drawCompactStatusRow(canvas, rowY, StatusGlyph::Bluetooth, model.bleConnected);
    rowY += rowStride;
    drawCompactStatusRow(canvas, rowY, StatusGlyph::Wifi, model.wifiConnected);
    rowY += rowStride;
    if constexpr (KawaiiUiProfile::kShowBattery) {
        drawCompactStatusRow(canvas, rowY, StatusGlyph::Battery,
                             batteryKnown, safeText(battery, "--"));
        rowY += rowStride;
    }
    drawCompactStatusRow(canvas, rowY, StatusGlyph::Ready, cameraReady);

    const char* primaryAction = model.cameraStandby ? "Wake"
                                : model.shutterReady ? "Shoot"
                                                     : "Retry";
    drawButtonHint(canvas, KawaiiLayout::kButtonX, KawaiiLayout::kButtonY,
                   KawaiiLayout::kButtonW, 'A', primaryAction);
    drawButtonHint(canvas,
                   KawaiiLayout::kButtonX,
                   KawaiiLayout::kButtonY +
                       KawaiiLayout::kButtonH + KawaiiLayout::kButtonGap,
                   KawaiiLayout::kButtonW,
                   'B',
                   "Reset Pair");
}

void KawaiiUiRenderer::renderSettings(LovyanGFX& canvas, const UiModel& model) {
    drawPatternBackground(canvas);
    drawOuterShell(canvas);
    drawHeader(canvas, "Quick Controls");

    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kTextSoft, KawaiiTheme::kPanel);
    canvas.setCursor(10, 24);
    canvas.print("Settings");
    canvas.setCursor(205, 24);
    canvas.print("Menu");

    const int16_t row = KawaiiLayout::kSettingsTileH + KawaiiLayout::kSettingsTileGap;
    drawSettingTile(canvas, KawaiiLayout::kSettingsLeftX,
                    KawaiiLayout::kSettingsTopY, "Shutter", "1/125s");
    drawSettingTile(canvas, KawaiiLayout::kSettingsRightX,
                    KawaiiLayout::kSettingsTopY, "Filter", "Standard");
    drawSettingTile(canvas, KawaiiLayout::kSettingsLeftX,
                    KawaiiLayout::kSettingsTopY + row, "Exposure", "+/-0.0");
    drawSettingTile(canvas, KawaiiLayout::kSettingsRightX,
                    KawaiiLayout::kSettingsTopY + row, "Wi-Fi",
                    model.wifiConnected ? "On" : "Off");
    drawSettingTile(canvas, KawaiiLayout::kSettingsLeftX,
                    KawaiiLayout::kSettingsTopY + row * 2, "Theme", "Kawaii GR");
    drawSettingTile(canvas, KawaiiLayout::kSettingsRightX,
                    KawaiiLayout::kSettingsTopY + row * 2, "Pair", "Reset All");
    drawSettingTile(canvas, KawaiiLayout::kSettingsLeftX,
                    KawaiiLayout::kSettingsTopY + row * 3, "Sleep", "2 min");

    canvas.fillRoundRect(KawaiiLayout::kSettingsRightX,
                         KawaiiLayout::kSettingsTopY + row * 3,
                         KawaiiLayout::kSettingsTileW,
                         KawaiiLayout::kSettingsTileH,
                         KawaiiLayout::kCardRadius,
                         KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(KawaiiLayout::kSettingsRightX,
                         KawaiiLayout::kSettingsTopY + row * 3,
                         KawaiiLayout::kSettingsTileW,
                         KawaiiLayout::kSettingsTileH,
                         KawaiiLayout::kCardRadius,
                         KawaiiTheme::kPanelBorder);
    drawPaw(canvas, 178, KawaiiLayout::kSettingsTopY + row * 3 + 8,
            KawaiiTheme::kAccentPink);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(188, KawaiiLayout::kSettingsTopY + row * 3 + 4);
    canvas.print("Ready");

    drawFooterPanel(canvas);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(8, 121);
    canvas.print("Camera: GR");
    canvas.setTextColor(KawaiiTheme::kTextSoft, KawaiiTheme::kPanelSoft);
    canvas.setCursor(75, 121);
    canvas.print(KawaiiAssets::kFirmwareLabel);
    if constexpr (KawaiiUiProfile::kShowWifiRssi) {
        drawWifiBadge(canvas, 183, 120, model.wifiConnected, model.wifiRssi);
    }
    if constexpr (KawaiiUiProfile::kShowBattery) {
        drawBatteryBadge(canvas, 215, 120, model.battery);
    }
}

void KawaiiUiRenderer::renderError(LovyanGFX& canvas, const UiModel& model) {
    drawPatternBackground(canvas);
    drawOuterShell(canvas);
    drawHeader(canvas, "Oops!");

    if constexpr (KawaiiUiProfile::kShowMascots) {
        drawMascot(canvas, 28, 68, false, true);
    }
    printCentered(canvas, "Camera Error", 31, 2,
                  KawaiiTheme::kDanger, KawaiiTheme::kPanel);

    canvas.fillRoundRect(62, 54, 166, 43, 8, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(62, 54, 166, 43, 8, KawaiiTheme::kDanger);
    printTruncated(canvas, safeText(model.errorMessage, "Unknown camera error"),
                   69, 62, 25, KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    printTruncated(canvas, model.detail,
                   69, 79, 25, KawaiiTheme::kTextSoft, KawaiiTheme::kPanelSoft);

    drawFooterPanel(canvas);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(10, 121);
    canvas.print(model.cameraStandby ? "A Wake" : model.shutterReady ? "A Shoot" : "A Retry");
    drawHeart(canvas, 120, 121, KawaiiTheme::kAccentPink);
    canvas.setCursor(148, 121);
    canvas.print("Hold B Reset");
}

void KawaiiUiRenderer::renderShutdown(LovyanGFX& canvas, const UiModel& model) {
    drawPatternBackground(canvas);
    drawOuterShell(canvas);
    drawHeader(canvas, "RICOH GR");

    if constexpr (KawaiiUiProfile::kShowMascots) {
        drawMascot(canvas, 34, 67, false, false);
        drawMascot(canvas, 206, 67, true, false);
    }
    printCentered(canvas, "GOOD NIGHT", 43, 2,
                  KawaiiTheme::kPrimary, KawaiiTheme::kPanel);
    drawPaw(canvas, 120, 73, KawaiiTheme::kAccentPink);
    char message[21];
    std::snprintf(message, sizeof(message), "%.20s", safeText(model.detail, "Powering off..."));
    printCentered(canvas, message, 88, 1,
                  KawaiiTheme::kTextSoft, KawaiiTheme::kPanel);

    drawFooterPanel(canvas);
    printCentered(canvas, "See you next time", 121, 1,
                  KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
}

}  // namespace rvf::ui
