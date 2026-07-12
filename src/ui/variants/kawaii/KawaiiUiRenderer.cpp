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
    const int16_t x = KawaiiLayout::kHeaderX;
    const int16_t y = KawaiiLayout::kHeaderY;
    const int16_t w = KawaiiLayout::kHeaderW;
    const int16_t h = KawaiiLayout::kHeaderH;
    canvas.fillRoundRect(x + 1, y + 2, w, h, KawaiiLayout::kHeaderRadius,
                         KawaiiTheme::kPanelShadow);
    canvas.fillRoundRect(x, y, w, h, KawaiiLayout::kHeaderRadius,
                         KawaiiTheme::kPanelSoft);
    canvas.fillTriangle(96, y + 2, 103, 0, 111, y + 2, KawaiiTheme::kPanelSoft);
    canvas.fillTriangle(129, y + 2, 137, 0, 144, y + 2, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(x, y, w, h, KawaiiLayout::kHeaderRadius,
                         KawaiiTheme::kPanelBorder);
    canvas.drawFastHLine(x + 12, y + 2, w - 24, KawaiiTheme::kWhite);
    printCentered(canvas, title, y + 6, 1,
                  KawaiiTheme::kPrimary, KawaiiTheme::kPanelSoft);
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
                                   bool crying,
                                   uint8_t scale) {
    const int16_t s = scale == 0 ? 1 : static_cast<int16_t>(scale);
    // Wide cat hood and raised paws follow the approved character silhouette.
    canvas.fillRoundRect(centerX - 24 * s, centerY + 10 * s,
                         48 * s, 26 * s, 13 * s, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX - 20 * s, centerY + 30 * s, 8 * s, KawaiiTheme::kSuit);
    canvas.fillCircle(centerX + 20 * s, centerY + 30 * s, 8 * s, KawaiiTheme::kSuit);

    canvas.fillTriangle(centerX - 31 * s, centerY - 10 * s,
                        centerX - 24 * s, centerY - 34 * s,
                        centerX - 7 * s, centerY - 15 * s,
                        KawaiiTheme::kSuit);
    canvas.fillTriangle(centerX + 7 * s, centerY - 15 * s,
                        centerX + 24 * s, centerY - 34 * s,
                        centerX + 31 * s, centerY - 10 * s,
                        KawaiiTheme::kSuit);
    canvas.fillTriangle(centerX - 25 * s, centerY - 12 * s,
                        centerX - 22 * s, centerY - 27 * s,
                        centerX - 12 * s, centerY - 15 * s,
                        KawaiiTheme::kWhite);
    canvas.fillTriangle(centerX + 12 * s, centerY - 15 * s,
                        centerX + 22 * s, centerY - 27 * s,
                        centerX + 25 * s, centerY - 12 * s,
                        KawaiiTheme::kWhite);
    canvas.fillEllipse(centerX, centerY - 1 * s,
                       30 * s, 31 * s, KawaiiTheme::kSuit);

    // Leopard-like hood patches remain deliberately bold at 240 x 135.
    canvas.fillEllipse(centerX - 20 * s, centerY - 18 * s,
                       7 * s, 5 * s, KawaiiTheme::kSuitSpot);
    canvas.fillEllipse(centerX + 10 * s, centerY - 23 * s,
                       8 * s, 5 * s, KawaiiTheme::kSuitSpot);
    canvas.fillEllipse(centerX + 23 * s, centerY - 5 * s,
                       6 * s, 8 * s, KawaiiTheme::kSuitSpot);
    canvas.fillEllipse(centerX - 25 * s, centerY + 8 * s,
                       5 * s, 7 * s, KawaiiTheme::kSuitSpot);
    canvas.fillCircle(centerX - 15 * s, centerY + 29 * s,
                      5 * s, KawaiiTheme::kSuitSpot);
    canvas.fillCircle(centerX + 18 * s, centerY + 27 * s,
                      5 * s, KawaiiTheme::kSuitSpot);

    // Face, orange fringe, closed eyes and puffed cheeks.
    canvas.fillEllipse(centerX, centerY + 2 * s,
                       21 * s, 22 * s, KawaiiTheme::kSkin);
    canvas.fillEllipse(centerX, centerY - 9 * s,
                       20 * s, 14 * s, KawaiiTheme::kHair);
    if (mirrored) {
        canvas.fillTriangle(centerX - 19 * s, centerY - 8 * s,
                            centerX + 7 * s, centerY - 15 * s,
                            centerX + 18 * s, centerY - 2 * s,
                            KawaiiTheme::kHair);
    } else {
        canvas.fillTriangle(centerX + 19 * s, centerY - 8 * s,
                            centerX - 7 * s, centerY - 15 * s,
                            centerX - 18 * s, centerY - 2 * s,
                            KawaiiTheme::kHair);
    }

    canvas.drawLine(centerX - 13 * s, centerY - 1 * s,
                    centerX - 6 * s, centerY + 3 * s,
                    KawaiiTheme::kPrimaryDark);
    canvas.drawLine(centerX - 12 * s, centerY + s,
                    centerX - 6 * s, centerY + 3 * s,
                    KawaiiTheme::kPrimaryDark);
    canvas.drawLine(centerX + 6 * s, centerY + 3 * s,
                    centerX + 13 * s, centerY - 1 * s,
                    KawaiiTheme::kPrimaryDark);
    canvas.drawLine(centerX + 6 * s, centerY + 3 * s,
                    centerX + 12 * s, centerY + s,
                    KawaiiTheme::kPrimaryDark);
    canvas.fillCircle(centerX, centerY + 7 * s, 3 * s, KawaiiTheme::kDanger);
    canvas.fillCircle(centerX, centerY + 13 * s, 2 * s, KawaiiTheme::kWhite);

    canvas.fillCircle(centerX - 17 * s, centerY + 18 * s,
                      11 * s, KawaiiTheme::kSkin);
    canvas.fillCircle(centerX + 17 * s, centerY + 18 * s,
                      11 * s, KawaiiTheme::kSkin);
    canvas.fillCircle(centerX - 17 * s, centerY + 18 * s,
                      5 * s, KawaiiTheme::kCheek);
    canvas.fillCircle(centerX + 17 * s, centerY + 18 * s,
                      5 * s, KawaiiTheme::kCheek);

    if (crying) {
        canvas.fillEllipse(centerX - 25 * s, centerY + 5 * s,
                           7 * s, 5 * s,
                          KawaiiTheme::kAccentGlow);
        canvas.fillEllipse(centerX + 25 * s, centerY + 5 * s,
                           7 * s, 5 * s,
                          KawaiiTheme::kAccentGlow);
        canvas.fillCircle(centerX - 27 * s, centerY + 3 * s,
                          2 * s, KawaiiTheme::kWhite);
        canvas.fillCircle(centerX + 23 * s, centerY + 3 * s,
                          2 * s, KawaiiTheme::kWhite);
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

void KawaiiUiRenderer::drawCompactStatusRow(LovyanGFX& canvas,
                                             int16_t y,
                                             StatusGlyph glyph,
                                             bool active,
                                             const char* value) {
    const int16_t x = KawaiiLayout::kStatusX + 3;
    const int16_t w = KawaiiLayout::kStatusW - 6;
    canvas.fillRoundRect(x, y, w, KawaiiLayout::kStatusRowH,
                         KawaiiLayout::kStatusRowRadius, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(x, y, w, KawaiiLayout::kStatusRowH,
                         KawaiiLayout::kStatusRowRadius, KawaiiTheme::kPanelBorder);

    const int16_t iconX = x + 5;
    switch (glyph) {
        case StatusGlyph::Camera:
        case StatusGlyph::Ready:
            canvas.drawRoundRect(iconX, y + 2, 12, 7, 2, KawaiiTheme::kPrimary);
            canvas.fillRect(iconX + 2, y + 1, 4, 2, KawaiiTheme::kPrimary);
            canvas.drawCircle(iconX + 6, y + 5, 2, KawaiiTheme::kPrimary);
            if (glyph == StatusGlyph::Ready) {
                canvas.drawFastHLine(iconX + 14, y + 3, 3, KawaiiTheme::kPrimary);
                canvas.drawFastHLine(iconX + 14, y + 7, 3, KawaiiTheme::kPrimary);
            }
            break;
        case StatusGlyph::Bluetooth:
            canvas.drawFastVLine(iconX + 6, y + 1, 9, KawaiiTheme::kPrimary);
            canvas.drawLine(iconX + 6, y + 1, iconX + 10, y + 4, KawaiiTheme::kPrimary);
            canvas.drawLine(iconX + 10, y + 4, iconX + 4, y + 9, KawaiiTheme::kPrimary);
            canvas.drawLine(iconX + 4, y + 2, iconX + 10, y + 7, KawaiiTheme::kPrimary);
            canvas.drawLine(iconX + 10, y + 7, iconX + 6, y + 10, KawaiiTheme::kPrimary);
            break;
        case StatusGlyph::Wifi:
            drawWifiBadge(canvas, iconX + 1, y + 1, active, active ? -55 : 0);
            break;
        case StatusGlyph::Battery:
            canvas.drawRect(iconX, y + 2, 13, 7, KawaiiTheme::kPrimary);
            canvas.fillRect(iconX + 13, y + 4, 2, 3, KawaiiTheme::kPrimary);
            if (active) {
                canvas.fillRect(iconX + 2, y + 4, 8, 3, KawaiiTheme::kLampGreen);
            }
            printTruncated(canvas, safeText(value, "--"), x + 22, y + 2, 4,
                           KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
            break;
    }

    canvas.fillCircle(x + w - 7,
                      y + KawaiiLayout::kStatusRowH / 2,
                      2,
                      active ? KawaiiTheme::kLampGreen : KawaiiTheme::kLampGray);
}

void KawaiiUiRenderer::drawButtonHint(LovyanGFX& canvas,
                                       int16_t x,
                                       int16_t y,
                                       int16_t width,
                                       char key,
                                       const char* label) {
    canvas.fillRoundRect(x + 1, y + 1, width, KawaiiLayout::kButtonH,
                         KawaiiLayout::kButtonH / 2, KawaiiTheme::kPanelShadow);
    canvas.fillRoundRect(x, y, width, KawaiiLayout::kButtonH,
                         KawaiiLayout::kButtonH / 2, KawaiiTheme::kPanelSoft);
    canvas.drawRoundRect(x, y, width, KawaiiLayout::kButtonH,
                         KawaiiLayout::kButtonH / 2, KawaiiTheme::kPanelBorder);
    canvas.fillCircle(x + 7, y + KawaiiLayout::kButtonH / 2, 5, KawaiiTheme::kPrimary);
    canvas.setTextSize(1);
    canvas.setTextColor(KawaiiTheme::kWhite, KawaiiTheme::kPrimary);
    canvas.setCursor(x + 4, y + 2);
    canvas.print(key);
    canvas.setTextColor(KawaiiTheme::kText, KawaiiTheme::kPanelSoft);
    canvas.setCursor(x + 15, y + 2);
    canvas.print(safeText(label));
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
    char label[20];
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
                const float fps = model.fps < 0.0f ? 0.0f : model.fps > 99.0f ? 99.0f : model.fps;
                std::snprintf(label, sizeof(label), "LIVE FPS %.0f", static_cast<double>(fps));
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
