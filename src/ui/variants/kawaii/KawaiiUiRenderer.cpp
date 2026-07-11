#include "KawaiiUiRenderer.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "KawaiiAssets.h"
#include "KawaiiLayout.h"
#include "KawaiiTheme.h"
#include "KawaiiUiProfile.h"

namespace rvf::ui {

const char* KawaiiUiRenderer::safeText(const char* value, const char* fallback) {
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

const char* KawaiiUiRenderer::bleStatusText(const UiModel& model) {
    if (model.bleConnected) {
        return "Connected";
    }
    if (model.cameraStandby || model.phase == UiPhase::CameraStandby) {
        return "Paused";
    }
    if (model.phase == UiPhase::BleConnecting) {
        return "Connecting";
    }
    if (model.phase == UiPhase::Recovering) {
        return "Recovering";
    }
    return "Scanning";
}

const char* KawaiiUiRenderer::wifiStatusText(const UiModel& model) {
    if (model.wifiConnected) {
        return "Connected";
    }
    switch (model.phase) {
        case UiPhase::ActivatingWifi:
        case UiPhase::WifiConnecting:
        case UiPhase::WifiConnected:
        case UiPhase::HttpProbing:
        case UiPhase::StartingPreview:
            return "Connecting";
        default:
            return "Not Ready";
    }
}

const char* KawaiiUiRenderer::signalText(const UiModel& model) {
    if (!model.wifiConnected || model.wifiRssi >= 0) {
        return "--";
    }
    if (model.wifiRssi >= -60) {
        return "Strong";
    }
    if (model.wifiRssi >= -75) {
        return "Medium";
    }
    return "Weak";
}

void KawaiiUiRenderer::printCentered(LovyanGFX& canvas,
                                      const char* text,
                                      int16_t y,
                                      uint8_t textSize,
                                      uint16_t color,
                                      uint16_t background) {
    const char* value = safeText(text);
    const int16_t width = static_cast<int16_t>(std::strlen(value) * 6U * textSize);
    canvas.setTextSize(textSize);
    canvas.setTextColor(color, background);
    canvas.setCursor(static_cast<int16_t>((canvas.width() - width) / 2), y);
    canvas.print(value);
}

void KawaiiUiRenderer::printTruncated(LovyanGFX& canvas,
                                       const char* text,
                                       int16_t x,
                                       int16_t y,
                                       size_t maxChars,
                                       uint16_t color,
                                       uint16_t background) {
    char clipped[40];
    if (maxChars >= sizeof(clipped)) {
        maxChars = sizeof(clipped) - 1;
    }
    const char* value = safeText(text);
    std::strncpy(clipped, value, maxChars);
    clipped[maxChars] = '\0';
    canvas.setTextSize(1);
    canvas.setTextColor(color, background);
    canvas.setCursor(x, y);
    canvas.print(clipped);
}

void KawaiiUiRenderer::drawKawaiiBlob(LovyanGFX& canvas,
                                       int16_t x,
                                       int16_t y,
                                       int16_t radius,
                                       uint16_t color) {
    const int16_t small = radius > 3 ? static_cast<int16_t>(radius / 2) : 2;
    canvas.fillCircle(x, y, radius, color);
    canvas.fillCircle(x - radius, y + small, small, color);
    canvas.fillCircle(x + radius, y - small, small, color);
    canvas.fillCircle(x + small, y + radius, small, color);
}

void KawaiiUiRenderer::drawPatternBackground(LovyanGFX& canvas) {
    canvas.fillScreen(KawaiiTheme::kBackground);
    if constexpr (!KawaiiUiProfile::kShowPatternBackground) {
        return;
    }

    for (size_t i = 0; i < KawaiiAssets::kPatternBlobCount; ++i) {
        const KawaiiPatternBlob& blob = KawaiiAssets::kPatternBlobs[i];
        const uint16_t color = (i % 3U == 0U) ? KawaiiTheme::kPattern
                                              : KawaiiTheme::kPatternSoft;
        drawKawaiiBlob(canvas, blob.x, blob.y, blob.radius, color);
    }
}

void KawaiiUiRenderer::drawOuterShell(LovyanGFX& canvas) {
    const int16_t x = KawaiiLayout::kShellX;
    const int16_t y = KawaiiLayout::kShellY;
    const int16_t w = KawaiiLayout::kShellW;
    const int16_t h = KawaiiLayout::kShellH;
    const int16_t earTop = KawaiiLayout::kEarTopY;

    canvas.fillTriangle(82, y + 3, 94, earTop, 107, y + 3, KawaiiTheme::kPanelShadow);
    canvas.fillTriangle(133, y + 3, 146, earTop, 158, y + 3, KawaiiTheme::kPanelShadow);
    canvas.fillRoundRect(x + 2, y + 3, w, h, KawaiiLayout::kShellRadius,
                         KawaiiTheme::kPanelShadow);
    canvas.fillTriangle(82, y + 2, 94, earTop, 106, y + 2, KawaiiTheme::kPanel);
    canvas.fillTriangle(134, y + 2, 146, earTop, 158, y + 2, KawaiiTheme::kPanel);
    canvas.fillRoundRect(x, y, w, h, KawaiiLayout::kShellRadius, KawaiiTheme::kPanel);
    canvas.drawRoundRect(x, y, w, h, KawaiiLayout::kShellRadius,
                         KawaiiTheme::kPanelBorder);
    canvas.drawRoundRect(x + 2, y + 2, w - 4, h - 4, KawaiiLayout::kShellRadius - 2,
                         KawaiiTheme::kWhite);
    canvas.drawLine(82, y + 2, 94, earTop, KawaiiTheme::kPanelBorder);
    canvas.drawLine(94, earTop, 106, y + 2, KawaiiTheme::kPanelBorder);
    canvas.drawLine(134, y + 2, 146, earTop, KawaiiTheme::kPanelBorder);
    canvas.drawLine(146, earTop, 158, y + 2, KawaiiTheme::kPanelBorder);
    canvas.drawFastHLine(x + 18, y + 4, 42, KawaiiTheme::kWhite);
}

void KawaiiUiRenderer::drawHeader(LovyanGFX& canvas, const char* title) {
    printCentered(canvas, title, 13, 1, KawaiiTheme::kPrimary, KawaiiTheme::kPanel);
}

void KawaiiUiRenderer::drawFooterPanel(LovyanGFX& canvas) {
    canvas.fillRoundRect(2,
                         KawaiiLayout::kFooterY,
                         canvas.width() - 4,
                         KawaiiLayout::kFooterH,
                         7,
                         KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(2,
                         KawaiiLayout::kFooterY,
                         canvas.width() - 4,
                         KawaiiLayout::kFooterH,
                         7,
                         KawaiiTheme::kPanelBorder);
    canvas.drawFastHLine(10, KawaiiLayout::kFooterY + 2, canvas.width() - 20,
                         KawaiiTheme::kWhite);
}

void KawaiiUiRenderer::drawPaw(LovyanGFX& canvas,
                               int16_t x,
                               int16_t y,
                               uint16_t color) {
    canvas.fillCircle(x, y + 3, 4, color);
    canvas.fillCircle(x - 5, y - 2, 2, color);
    canvas.fillCircle(x - 1, y - 4, 2, color);
    canvas.fillCircle(x + 4, y - 3, 2, color);
}

void KawaiiUiRenderer::drawHeart(LovyanGFX& canvas,
                                 int16_t x,
                                 int16_t y,
                                 uint16_t color) {
    canvas.fillCircle(x - 2, y, 3, color);
    canvas.fillCircle(x + 2, y, 3, color);
    canvas.fillTriangle(x - 5, y + 1, x + 5, y + 1, x, y + 8, color);
}

void KawaiiUiRenderer::drawMascot(LovyanGFX& canvas,
                                   int16_t centerX,
                                   int16_t centerY,
                                   bool mirrored,
                                   bool crying) {
    canvas.fillRoundRect(centerX - 12, centerY + 10, 24, 25, 8, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX - 9, centerY + 29, 6, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX + 9, centerY + 29, 6, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX - 8, centerY + 21, 3, KawaiiTheme::kSuitSpot);
    canvas.fillCircle(centerX + 7, centerY + 27, 4, KawaiiTheme::kSuitSpot);

    canvas.fillTriangle(centerX - 18, centerY - 8,
                        centerX - 12, centerY - 24,
                        centerX - 4, centerY - 9,
                        KawaiiTheme::kSuit);
    canvas.fillTriangle(centerX + 4, centerY - 9,
                        centerX + 12, centerY - 24,
                        centerX + 18, centerY - 8,
                        KawaiiTheme::kSuit);
    canvas.fillCircle(centerX, centerY - 3, 19, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX - 13, centerY - 9, 4, KawaiiTheme::kSuitSpot);
    canvas.fillCircle(centerX + 12, centerY - 2, 5, KawaiiTheme::kSuitSpot);
    canvas.fillCircle(centerX, centerY - 2, 13, KawaiiTheme::kSkin);

    canvas.fillCircle(centerX, centerY - 10, 11, KawaiiTheme::kHair);
    if (mirrored) {
        canvas.fillTriangle(centerX - 12, centerY - 7,
                            centerX + 8, centerY - 11,
                            centerX + 12, centerY - 4,
                            KawaiiTheme::kHair);
    } else {
        canvas.fillTriangle(centerX + 12, centerY - 7,
                            centerX - 8, centerY - 11,
                            centerX - 12, centerY - 4,
                            KawaiiTheme::kHair);
    }

    canvas.drawLine(centerX - 8, centerY - 2,
                    centerX - 3, centerY + 1,
                    KawaiiTheme::kPrimaryDark);
    canvas.drawLine(centerX + 3, centerY + 1,
                    centerX + 8, centerY - 2,
                    KawaiiTheme::kPrimaryDark);
    canvas.fillCircle(centerX, centerY + 4, 2, KawaiiTheme::kDanger);
    canvas.fillCircle(centerX - 8, centerY + 5, 3, KawaiiTheme::kCheek);
    canvas.fillCircle(centerX + 8, centerY + 5, 3, KawaiiTheme::kCheek);
    if (crying) {
        canvas.fillCircle(centerX - 12, centerY + 2, 3, KawaiiTheme::kAccentGlow);
        canvas.fillCircle(centerX + 12, centerY + 2, 3, KawaiiTheme::kAccentGlow);
        canvas.fillTriangle(centerX - 15, centerY + 3,
                            centerX - 9, centerY + 3,
                            centerX - 12, centerY + 9,
                            KawaiiTheme::kAccentGlow);
        canvas.fillTriangle(centerX + 9, centerY + 3,
                            centerX + 15, centerY + 3,
                            centerX + 12, centerY + 9,
                            KawaiiTheme::kAccentGlow);
    }
}

void KawaiiUiRenderer::drawMiniMascot(LovyanGFX& canvas,
                                       int16_t centerX,
                                       int16_t centerY) {
    canvas.fillTriangle(centerX - 8, centerY - 4,
                        centerX - 5, centerY - 12,
                        centerX - 1, centerY - 5,
                        KawaiiTheme::kSuit);
    canvas.fillTriangle(centerX + 1, centerY - 5,
                        centerX + 5, centerY - 12,
                        centerX + 8, centerY - 4,
                        KawaiiTheme::kSuit);
    canvas.fillCircle(centerX, centerY, 8, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX, centerY, 5, KawaiiTheme::kSkin);
    canvas.fillCircle(centerX - 2, centerY, 1, KawaiiTheme::kPrimaryDark);
    canvas.fillCircle(centerX + 2, centerY, 1, KawaiiTheme::kPrimaryDark);
    canvas.fillCircle(centerX, centerY + 3, 1, KawaiiTheme::kDanger);
}

void KawaiiUiRenderer::drawWifiBadge(LovyanGFX& canvas,
                                      int16_t x,
                                      int16_t y,
                                      bool connected,
                                      int32_t rssi) {
    uint8_t bars = 0;
    if (connected) {
        if (rssi >= 0) bars = 1;
        else if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else bars = 1;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t barHeight = static_cast<int16_t>(2 + i * 2);
        canvas.fillRoundRect(x + static_cast<int16_t>(i * 3),
                             y + 8 - barHeight,
                             2,
                             barHeight,
                             1,
                             i < bars ? KawaiiTheme::kPrimary : KawaiiTheme::kTextSoft);
    }
}

void KawaiiUiRenderer::drawBatteryBadge(LovyanGFX& canvas,
                                         int16_t x,
                                         int16_t y,
                                         const char* battery) {
    int percent = -1;
    const char* text = safeText(battery);
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (std::isdigit(static_cast<unsigned char>(text[i])) != 0) {
            if (percent < 0) percent = 0;
            const int digit = text[i] - '0';
            if (percent > (100 - digit) / 10) {
                percent = 100;
                break;
            }
            percent = percent * 10 + digit;
        } else if (percent >= 0) {
            break;
        }
    }
    if (percent > 100) percent = 100;

    canvas.drawRoundRect(x, y, 16, 9, 2, KawaiiTheme::kPrimary);
    canvas.fillRect(x + 16, y + 3, 2, 3, KawaiiTheme::kPrimary);
    if (percent < 0) {
        canvas.drawLine(x + 3, y + 2, x + 12, y + 6, KawaiiTheme::kTextSoft);
        return;
    }
    const int16_t fillWidth = static_cast<int16_t>((percent * 12) / 100);
    const uint16_t color = percent < 20 ? KawaiiTheme::kDanger
                             : percent < 50 ? KawaiiTheme::kWarning
                                            : KawaiiTheme::kSuccess;
    canvas.fillRoundRect(x + 2, y + 2, fillWidth, 5, 1, color);
}

void KawaiiUiRenderer::drawStatusRow(LovyanGFX& canvas,
                                      int16_t y,
                                      const char* label,
                                      const char* value,
                                      bool ok) {
    const int16_t x = KawaiiLayout::kStatusRowsX;
    const int16_t w = KawaiiLayout::kStatusRowsW;
    canvas.fillRoundRect(x, y, w, KawaiiLayout::kStatusRowH,
                         KawaiiLayout::kStatusRowRadius, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(x, y, w, KawaiiLayout::kStatusRowH,
                         KawaiiLayout::kStatusRowRadius, KawaiiTheme::kPanelBorder);
    canvas.fillCircle(x + 7, y + 6, 2, ok ? KawaiiTheme::kSuccess : KawaiiTheme::kTextSoft);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kTextSoft, KawaiiTheme::kPanelSoft);
    canvas.setCursor(x + 13, y + 3);
    canvas.print(safeText(label));

    char clipped[14];
    std::snprintf(clipped, sizeof(clipped), "%.13s", safeText(value, "--"));
    const int16_t valueWidth = static_cast<int16_t>(std::strlen(clipped) * 6U);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(x + w - valueWidth - 5, y + 3);
    canvas.print(clipped);
}

void KawaiiUiRenderer::drawSettingTile(LovyanGFX& canvas,
                                        int16_t x,
                                        int16_t y,
                                        const char* label,
                                        const char* value) {
    canvas.fillRoundRect(x, y, KawaiiLayout::kSettingsTileW,
                         KawaiiLayout::kSettingsTileH, KawaiiLayout::kCardRadius,
                         KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(x, y, KawaiiLayout::kSettingsTileW,
                         KawaiiLayout::kSettingsTileH, KawaiiLayout::kCardRadius,
                         KawaiiTheme::kPanelBorder);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kTextSoft, KawaiiTheme::kPanelSoft);
    canvas.setCursor(x + 5, y + 4);
    canvas.print(safeText(label));

    char clipped[10];
    std::snprintf(clipped, sizeof(clipped), "%.9s", safeText(value, "--"));
    const int16_t valueWidth = static_cast<int16_t>(std::strlen(clipped) * 6U);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(x + KawaiiLayout::kSettingsTileW - valueWidth - 5, y + 4);
    canvas.print(clipped);
}

void KawaiiUiRenderer::drawLiveBadge(LovyanGFX& canvas, const UiModel& model) {
    char label[16];
    uint16_t color = model.previewRunning ? KawaiiTheme::kAccentPink
                                          : KawaiiTheme::kWarning;
    switch (model.shutterStatus) {
        case UiShutterStatus::Shooting:
            std::snprintf(label, sizeof(label), "SHOOTING");
            color = KawaiiTheme::kWarning;
            break;
        case UiShutterStatus::Succeeded:
            std::snprintf(label, sizeof(label), "SHOT OK");
            color = KawaiiTheme::kSuccess;
            break;
        case UiShutterStatus::Failed:
            std::snprintf(label, sizeof(label), "SHOT FAIL");
            color = KawaiiTheme::kDanger;
            break;
        case UiShutterStatus::Idle:
            if constexpr (KawaiiUiProfile::kShowFps) {
                std::snprintf(label, sizeof(label), "LIVE %.1f", static_cast<double>(model.fps));
            } else {
                std::snprintf(label, sizeof(label), "LIVE");
            }
            break;
    }

    int16_t width = static_cast<int16_t>(std::strlen(label) * 6U + 14);
    if (width < 43) width = 43;
    canvas.fillRoundRect(3, 3, width, 15, 7, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(3, 3, width, 15, 7, KawaiiTheme::kPanelBorder);
    canvas.fillCircle(10, 10, 2, color);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(15, 7);
    canvas.print(label);
}

void KawaiiUiRenderer::drawFocusBracket(LovyanGFX& canvas) {
    const int16_t cx = canvas.width() / 2;
    const int16_t cy = canvas.height() / 2;
    const int16_t left = cx - KawaiiLayout::kFocusBoxW / 2;
    const int16_t right = cx + KawaiiLayout::kFocusBoxW / 2;
    const int16_t top = cy - KawaiiLayout::kFocusBoxH / 2;
    const int16_t bottom = cy + KawaiiLayout::kFocusBoxH / 2;
    constexpr int16_t mark = 6;

    canvas.drawFastHLine(left, top, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastVLine(left, top, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastHLine(right - mark, top, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastVLine(right, top, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastHLine(left, bottom, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastVLine(left, bottom - mark, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastHLine(right - mark, bottom, mark, KawaiiTheme::kAccentGlow);
    canvas.drawFastVLine(right, bottom - mark, mark, KawaiiTheme::kAccentGlow);

    canvas.drawFastHLine(left + 1, top + 1, mark - 2, KawaiiTheme::kPrimary);
    canvas.drawFastHLine(right - mark + 1, top + 1, mark - 2, KawaiiTheme::kPrimary);
    canvas.drawFastHLine(left + 1, bottom - 1, mark - 2, KawaiiTheme::kPrimary);
    canvas.drawFastHLine(right - mark + 1, bottom - 1, mark - 2, KawaiiTheme::kPrimary);
}

}  // namespace rvf::ui
