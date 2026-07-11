#include "KawaiiUiRenderer.h"

#include <cstdio>
#include <cstring>

#include "KawaiiAssets.h"
#include "KawaiiLayout.h"
#include "KawaiiTheme.h"
#include "KawaiiUiProfile.h"

namespace rvf::ui {

void KawaiiUiRenderer::renderBoot(LovyanGFX& canvas, const UiModel& model) {
    drawPatternBackground(canvas);
    drawOuterShell(canvas);
    drawHeader(canvas, "RICOH GR");

    if constexpr (KawaiiUiProfile::kShowMascots) {
        drawMascot(canvas, 26, 61, false, true);
        drawMascot(canvas, 214, 61, true, true);
    }

    printCentered(canvas, "GR LIVE VIEW", 35, 2,
                  KawaiiTheme::kPrimary, KawaiiTheme::kPanel);
    printCentered(canvas, KawaiiAssets::kThemeLabel, 58, 1,
                  KawaiiTheme::kTextSoft, KawaiiTheme::kPanel);
    drawHeart(canvas, 75, 58, KawaiiTheme::kAccentPink);
    drawHeart(canvas, 165, 58, KawaiiTheme::kAccentPink);

    char bootText[22];
    std::snprintf(bootText, sizeof(bootText), "%.21s", safeText(model.detail, "Booting..."));
    printCentered(canvas, bootText, 76, 1,
                  KawaiiTheme::kText, KawaiiTheme::kPanel);

    constexpr int16_t progressX = 58;
    constexpr int16_t progressY = 90;
    constexpr int16_t progressW = 124;
    constexpr int16_t progressH = 13;
    constexpr int16_t progressPercent = 68;
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
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kPrimary, KawaiiTheme::kPanelSoft);
    canvas.setCursor(progressX + progressW - 25, progressY + 3);
    canvas.print("68%");

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
    drawPatternBackground(canvas);
    drawOuterShell(canvas);
    drawHeader(canvas, "Camera Connect");

    canvas.fillRoundRect(KawaiiLayout::kStatusCardX,
                         KawaiiLayout::kStatusCardY,
                         KawaiiLayout::kStatusCardW,
                         KawaiiLayout::kStatusCardH,
                         KawaiiLayout::kCardRadius,
                         KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(KawaiiLayout::kStatusCardX,
                         KawaiiLayout::kStatusCardY,
                         KawaiiLayout::kStatusCardW,
                         KawaiiLayout::kStatusCardH,
                         KawaiiLayout::kCardRadius,
                         KawaiiTheme::kPanelBorder);

    canvas.fillRoundRect(18, 34, 47, 21, 4, KawaiiTheme::kPrimary);
    canvas.fillRect(24, 30, 15, 5, KawaiiTheme::kPrimary);
    canvas.fillCircle(42, 44, 7, KawaiiTheme::kPanelSoft);
    canvas.drawCircle(42, 44, 7, KawaiiTheme::kWhite);
    canvas.fillCircle(61, 36, 2,
                      model.bleConnected ? KawaiiTheme::kSuccess : KawaiiTheme::kWarning);

    if constexpr (KawaiiUiProfile::kShowMascots) {
        drawMiniMascot(canvas, 42, 78);
    }

    char leftStatus[11];
    std::snprintf(leftStatus, sizeof(leftStatus), "%.10s", bleStatusText(model));
    const int16_t statusWidth = static_cast<int16_t>(std::strlen(leftStatus) * 6U);
    canvas.setTextSize(1);
    canvas.setTextColor(model.bleConnected ? KawaiiTheme::kSuccess : KawaiiTheme::kText,
                        KawaiiTheme::kPanelSoft);
    canvas.setCursor(42 - statusWidth / 2, 99);
    canvas.print(leftStatus);

    const char* cameraValue = safeText(model.cameraModel, safeText(model.cameraName));
    const bool hasCameraIdentity = cameraValue[0] != '\0';
    const char* camera = hasCameraIdentity ? cameraValue : "RICOH GR";
    const char* battery = safeText(model.battery);
    constexpr int16_t rowStride = KawaiiLayout::kStatusRowH + KawaiiLayout::kStatusRowGap;
    int16_t rowY = KawaiiLayout::kStatusRowsY;
    drawStatusRow(canvas, rowY, "BLE", bleStatusText(model), model.bleConnected);
    rowY += rowStride;
    drawStatusRow(canvas, rowY, "Wi-Fi", wifiStatusText(model), model.wifiConnected);
    rowY += rowStride;
    if constexpr (KawaiiUiProfile::kShowCameraModel) {
        drawStatusRow(canvas, rowY, "Camera", camera,
                      model.bleConnected && hasCameraIdentity);
        rowY += rowStride;
    }
    if constexpr (KawaiiUiProfile::kShowBattery) {
        drawStatusRow(canvas, rowY, "Battery", safeText(battery, "--"), battery[0] != '\0');
        rowY += rowStride;
    }
    if constexpr (KawaiiUiProfile::kShowWifiRssi) {
        drawStatusRow(canvas, rowY, "Signal", signalText(model),
                      model.wifiConnected && model.wifiRssi < 0);
    }

    drawFooterPanel(canvas);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(9, 121);
    canvas.print(model.cameraStandby ? "A Wake" : model.shutterReady ? "A Shoot" : "A Retry");
    drawPaw(canvas, 120, 124, KawaiiTheme::kPrimary);
    canvas.setCursor(154, 121);
    canvas.print("Hold B Reset");
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
