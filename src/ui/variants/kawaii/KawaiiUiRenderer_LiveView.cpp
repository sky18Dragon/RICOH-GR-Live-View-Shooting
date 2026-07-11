#include "KawaiiUiRenderer.h"

#include <cstdio>
#include <cstring>

#include "KawaiiLayout.h"
#include "KawaiiTheme.h"
#include "KawaiiUiProfile.h"

namespace rvf::ui {

void KawaiiUiRenderer::renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model) {
    // The JPEG decoder owns all base pixels. This method intentionally draws
    // only edge HUD elements and never clears or presents the surface.
    drawLiveBadge(canvas, model);

    canvas.drawFastHLine(76, 1, 31, KawaiiTheme::kAccentGlow);
    canvas.drawLine(107, 1, 112, 6, KawaiiTheme::kAccentGlow);
    canvas.drawLine(112, 6, 117, 1, KawaiiTheme::kAccentGlow);
    canvas.drawFastHLine(117, 1, 6, KawaiiTheme::kAccentGlow);
    canvas.drawLine(123, 1, 128, 6, KawaiiTheme::kAccentGlow);
    canvas.drawLine(128, 6, 133, 1, KawaiiTheme::kAccentGlow);
    canvas.drawFastHLine(133, 1, 30, KawaiiTheme::kAccentGlow);
    canvas.fillRoundRect(89, 2, 62, 15, 7, KawaiiTheme::kPrimary);
    canvas.drawRoundRect(89, 2, 62, 15, 7, KawaiiTheme::kAccentGlow);
    printCentered(canvas, "RICOH GR", 5, 1, KawaiiTheme::kWhite, KawaiiTheme::kPrimary);

    canvas.fillRoundRect(186, 2, 51, 15, 7, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(186, 2, 51, 15, 7, KawaiiTheme::kPanelBorder);
    if constexpr (KawaiiUiProfile::kShowWifiRssi) {
        drawWifiBadge(canvas, 191, 5, model.wifiConnected, model.wifiRssi);
    }
    if constexpr (KawaiiUiProfile::kShowBattery) {
        drawBatteryBadge(canvas, 218, 4, model.battery);
    }

    if constexpr (KawaiiUiProfile::kShowCameraModel) {
        canvas.fillRoundRect(3, 21, 55, 13, 6, KawaiiTheme::kPanelSoft);
        canvas.drawRoundRect(3, 21, 55, 13, 6, KawaiiTheme::kPanelBorder);
        printTruncated(canvas,
                       safeText(model.cameraModel,
                                safeText(model.cameraName, "RICOH GR")),
                       8, 25, 7, KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    }

    if constexpr (KawaiiUiProfile::kShowFrameStats) {
        char stats[12];
        std::snprintf(stats,
                      sizeof(stats),
                      "%03lu/%03lu",
                      static_cast<unsigned long>(model.renderedFrames % 1000U),
                      static_cast<unsigned long>(model.droppedFrames % 1000U));
        constexpr int16_t width = 55;
        const int16_t x = canvas.width() - width - 3;
        canvas.fillRoundRect(x, 21, width, 13, 6,
                             KawaiiTheme::kPanelSoft);
        canvas.drawRoundRect(x, 21, width, 13, 6, KawaiiTheme::kPanelBorder);
        canvas.setTextSize(1);
        canvas.setTextColor(model.droppedFrames == 0 ? KawaiiTheme::kText
                                                     : KawaiiTheme::kWarning,
                            KawaiiTheme::kPanelSoft);
        canvas.setCursor(x + 7, 25);
        canvas.print(stats);
    }

    if constexpr (KawaiiUiProfile::kShowFocusBracket) {
        drawFocusBracket(canvas);
    }

    if constexpr (KawaiiUiProfile::kShowMascots) {
        drawMiniMascot(canvas, 11, 106);
        drawMiniMascot(canvas, canvas.width() - 11, 106);
    }

    canvas.fillRoundRect(3, 117, canvas.width() - 6, 16, 7, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(3, 117, canvas.width() - 6, 16, 7, KawaiiTheme::kPanelBorder);
    canvas.drawFastHLine(12, 119, canvas.width() - 24, KawaiiTheme::kWhite);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(8, 123);
    canvas.print("A Shoot");
    drawPaw(canvas, 120, 125, KawaiiTheme::kPrimary);
    canvas.setCursor(160, 123);
    canvas.print("Hold B Reset");
}

}  // namespace rvf::ui
