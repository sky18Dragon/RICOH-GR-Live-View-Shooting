#include "display.h"

namespace {
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_YELLOW = 0xFFE0;

// RICOH styling additions
constexpr uint16_t COLOR_BG = 0x1082;     // Matte Charcoal Black (#121212)
constexpr uint16_t COLOR_AMBER = 0xFD20;  // Signature Amber Orange (#FF9500)
constexpr uint16_t COLOR_SLATE = 0x2104;  // Slate Gray (#212121)
constexpr uint16_t COLOR_GRAY = 0x7BEF;   // Mid Gray (#7B7B7B)
constexpr uint16_t COLOR_CARD = 0x0841;   // Deep black panel
constexpr uint16_t COLOR_PANEL = 0x18E3;  // Dark graphite
constexpr uint16_t COLOR_GRAPHITE = 0x31A6;
constexpr uint16_t COLOR_DARK = 0x0000;
constexpr uint16_t COLOR_HILITE = 0xE71C;

const char* safeText(const char* value, const char* fallback = "") {
    return value != nullptr ? value : fallback;
}


char upperAscii(char ch) {
    return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - ('a' - 'A')) : ch;
}

bool containsIgnoreCase(const char* value, const char* needle) {
    if (needle == nullptr || needle[0] == '\0') {
        return true;
    }
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    for (const char* pos = value; *pos != '\0'; ++pos) {
        const char* hay = pos;
        const char* pat = needle;
        while (*hay != '\0' && *pat != '\0' && upperAscii(*hay) == upperAscii(*pat)) {
            ++hay;
            ++pat;
        }
        if (*pat == '\0') {
            return true;
        }
    }
    return false;
}

bool statusContains(const char* line1,
                    const char* line2,
                    const char* line3,
                    const char* line4,
                    const char* needle) {
    return containsIgnoreCase(line1, needle) ||
           containsIgnoreCase(line2, needle) ||
           containsIgnoreCase(line3, needle) ||
           containsIgnoreCase(line4, needle);
}


}  // namespace

bool DisplayUi::begin() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    M5.Display.setRotation(1);
    _width = M5.Display.width();
    _height = M5.Display.height();

    // StickS3 is physically 135x240; rotation 1/3 should expose landscape 240x135.
    // If a board package reports the opposite, rotate once more to keep callers in landscape.
    if (_width < _height) {
        M5.Display.setRotation(3);
        _width = M5.Display.width();
        _height = M5.Display.height();
    }

    // Initialize Canvas sprite for double-buffered flicker-free rendering
    _canvas.setColorDepth(16); // 16-bit RGB565
    _canvas.createSprite(_width, _height);

    clear(COLOR_BG);
    _canvas.setTextSize(1);
    _canvas.setTextWrap(false);

    // Initial push to screen
    pushCanvas();
    return true;
}

void DisplayUi::showBoot(const char* message) {
    clear(COLOR_BG);

    // Outer card border (matching BLE status screen)
    const int16_t x = 8;
    const int16_t y = 6;
    const int16_t w = _width - 16;
    const int16_t h = _height - 12;
    _canvas.fillRoundRect(x, y, w, h, 10, COLOR_CARD);
    _canvas.drawRoundRect(x, y, w, h, 10, COLOR_GRAPHITE);
    _canvas.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 8, COLOR_SLATE);

    // Abstract dark geometric lens elements
    _canvas.fillTriangle(x + 10, y + 16, x + 94, y + 16, x + 45, y + 106, COLOR_PANEL);
    _canvas.fillTriangle(x + 92, y + 18, x + 142, y + 68, x + 98, y + 112, COLOR_DARK);
    _canvas.fillTriangle(x + 120, y + 60, x + 205, y + 16, x + 202, y + 108, COLOR_PANEL);
    _canvas.fillTriangle(x + 118, y + 62, x + 166, y + 112, x + 210, y + 80, COLOR_SLATE);
    _canvas.fillTriangle(x + 142, y + 72, x + 184, y + 34, x + 216, y + 74, COLOR_DARK);
    _canvas.fillTriangle(x + 72, y + 24, x + 108, y + 110, x + 56, y + 112, COLOR_DARK);

    // Diagonal light leak highlight band
    const int16_t hx1 = x + 14;
    const int16_t hy1 = y + 28;
    const int16_t hx2 = x + 214;
    const int16_t hy2 = y + 86;
    const int16_t band = 12;
    _canvas.fillTriangle(hx1, hy1, hx2, hy2, hx2, hy2 + band, COLOR_HILITE);
    _canvas.fillTriangle(hx1, hy1, hx2, hy2 + band, hx1, hy1 + band, COLOR_HILITE);

    // Layered aperture overlays
    _canvas.fillTriangle(x + 150, y + 78, x + 184, y + 34, x + 216, y + 80, COLOR_DARK);
    _canvas.fillTriangle(x + 146, y + 82, x + 210, y + 82, x + 134, y + 112, COLOR_DARK);

    // Overlaid brand content with transparent background text
    _canvas.setTextColor(COLOR_AMBER);
    _canvas.setTextSize(3);
    _canvas.setCursor(102, 20);
    _canvas.print("GR");

    // Subtitle
    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_WHITE);
    _canvas.setCursor(84, 52);
    _canvas.print("VIEWFINDER");

    // Progress bar matching the layout
    _canvas.drawRoundRect(40, 70, 160, 6, 3, COLOR_GRAPHITE);
    _canvas.fillRoundRect(42, 72, 60, 2, 1, COLOR_AMBER);

    // Dynamic boot status message
    _canvas.setTextColor(COLOR_GRAY);
    const char* msg = safeText(message, "Booting...");
    int msgLen = strlen(msg);
    _canvas.setCursor((240 - (msgLen * 6)) / 2, 86);
    _canvas.print(msg);

    // Footer divider and hotkeys hint
    _canvas.drawFastHLine(20, _height - 24, 200, COLOR_SLATE);
    _canvas.setCursor(60, _height - 16);
    _canvas.print("BtnA: Shutter / Retry");

    pushCanvas();
}

void DisplayUi::showStatus(const char* line1, const char* line2, const char* line3, const char* line4) {
    clear(COLOR_BG);

    _canvas.drawFastHLine(10, 24, _width - 20, COLOR_SLATE);
    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_AMBER, COLOR_BG);
    _canvas.setCursor(10, 8);
    _canvas.print("GR VIEWFINDER");

    drawStatusLines(line1, line2, line3, line4);

    pushCanvas();
}

void DisplayUi::showStatus(const String& line1, const String& line2, const String& line3, const String& line4) {
    showStatus(line1.c_str(),
               line2.length() ? line2.c_str() : nullptr,
               line3.length() ? line3.c_str() : nullptr,
               line4.length() ? line4.c_str() : nullptr);
}

void DisplayUi::showError(const char* message, const char* detail) {
    clear(COLOR_BG);

    // Header
    _canvas.drawFastHLine(10, 24, _width - 20, COLOR_SLATE);
    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_RED, COLOR_BG);
    _canvas.setCursor(10, 8);
    _canvas.print("SYSTEM ERROR");

    // Outlined error card
    _canvas.drawRoundRect(10, 32, 220, 78, 4, COLOR_RED);

    _canvas.setTextColor(COLOR_RED, COLOR_BG);
    _canvas.setCursor(20, 42);
    _canvas.print(safeText(message, "Unknown error"));

    if (detail != nullptr && detail[0] != '\0') {
        _canvas.setTextColor(COLOR_WHITE, COLOR_BG);
        _canvas.setCursor(20, 62);
        _canvas.print(detail);
    }

    // Footer action hint
    _canvas.drawFastHLine(10, _height - 20, _width - 20, COLOR_SLATE);
    _canvas.setTextColor(COLOR_WHITE, COLOR_BG);
    _canvas.setCursor(50, _height - 14);
    _canvas.print("Press BtnA to retry");

    pushCanvas();
}

void DisplayUi::showError(const String& message, const String& detail) {
    showError(message.c_str(), detail.length() ? detail.c_str() : nullptr);
}

// Redesigned transparent HUD overlay for Live Viewfinder (rendered onto _canvas)
void DisplayUi::drawOverlay(const String& wifiStatus,
                            const String& liveviewStatus,
                            const String& model,
                            const String& battery,
                            float fps,
                            int32_t rssi,
                            uint32_t frames,
                            uint32_t droppedFrames) {
    // 1. Draw corner crop marks of the viewport (Assuming standard 4:3 centered image width 180, X=30..210)
    const int16_t vx = 30;
    const int16_t vy = 0;
    const int16_t vw = 180;
    const int16_t vh = 135;
    const int16_t len = 8;

    // Top-Left corner
    _canvas.drawFastHLine(vx, vy, len, COLOR_WHITE);
    _canvas.drawFastVLine(vx, vy, len, COLOR_WHITE);
    // Top-Right corner
    _canvas.drawFastHLine(vx + vw - len, vy, len, COLOR_WHITE);
    _canvas.drawFastVLine(vx + vw - 1, vy, len, COLOR_WHITE);
    // Bottom-Left corner
    _canvas.drawFastHLine(vx, vy + vh - 1, len, COLOR_WHITE);
    _canvas.drawFastVLine(vx, vy + vh - len, len, COLOR_WHITE);
    // Bottom-Right corner
    _canvas.drawFastHLine(vx + vw - len, vy + vh - 1, len, COLOR_WHITE);
    _canvas.drawFastVLine(vx + vw - 1, vy + vh - len, len, COLOR_WHITE);

    // 2. Draw autofocus bracket in the center (green, X=108..132, Y=59..75)
    const int16_t cx = _width / 2;
    const int16_t cy = _height / 2;
    const int16_t bw = 12;
    const int16_t bh = 8;

    _canvas.drawFastVLine(cx - bw, cy - bh, bh * 2, COLOR_GREEN);
    _canvas.drawFastHLine(cx - bw, cy - bh, 4, COLOR_GREEN);
    _canvas.drawFastHLine(cx - bw, cy + bh - 1, 4, COLOR_GREEN);

    _canvas.drawFastVLine(cx + bw - 1, cy - bh, bh * 2, COLOR_GREEN);
    _canvas.drawFastHLine(cx + bw - 4, cy - bh, 4, COLOR_GREEN);
    _canvas.drawFastHLine(cx + bw - 4, cy + bh - 1, 4, COLOR_GREEN);

    // 3. Draw transparent HUD overlays
    _canvas.setTextSize(1);

    // Top-Left: LIVE state with a small status dot
    const bool liveActive = (liveviewStatus == "LIVE");
    _canvas.fillCircle(14, 10, 3, liveActive ? COLOR_GREEN : COLOR_YELLOW);
    _canvas.setTextColor(COLOR_WHITE);
    _canvas.setCursor(22, 6);
    _canvas.print(liveActive ? "LIVE" : "IDLE");

    // Top-Left (below LIVE): FPS display
    char fpsText[16];
    if (fps >= 0.0f) {
        snprintf(fpsText, sizeof(fpsText), "%.1f FPS", static_cast<double>(fps));
    } else {
        snprintf(fpsText, sizeof(fpsText), "-- FPS");
    }
    _canvas.setTextColor(COLOR_GRAY);
    _canvas.setCursor(14, 18);
    _canvas.print(fpsText);

    // Bottom-Left: Camera Model & Battery level icon
    _canvas.setTextColor(COLOR_WHITE);
    _canvas.setCursor(14, _height - 24);
    if (model.length() > 0) {
        _canvas.print(model.substring(0, 8));
    } else {
        _canvas.print("RICOH GR");
    }
    drawBatteryIcon(14, _height - 12, battery.c_str());

    // Bottom-Right: RSSI (WiFi signal bars)
    drawWifiIcon(_width - 24, _height - 12, rssi);

    // Top-Right: Frame statistics
    char statsText[24];
    snprintf(statsText, sizeof(statsText), "%lu/%lu",
             static_cast<unsigned long>(frames),
             static_cast<unsigned long>(droppedFrames));
    _canvas.setTextColor(droppedFrames == 0 ? COLOR_WHITE : COLOR_YELLOW);
    _canvas.setCursor(_width - (strlen(statsText) * 6) - 14, 6);
    _canvas.print(statsText);
}

int16_t DisplayUi::width() const {
    return _width;
}

int16_t DisplayUi::height() const {
    return _height;
}

void DisplayUi::clear(uint16_t color) {
    _canvas.fillScreen(color);
}

void DisplayUi::drawStatusLines(const char* line1, const char* line2, const char* line3, const char* line4) {
    const char* s1 = safeText(line1);
    const char* s2 = safeText(line2);
    const char* s3 = safeText(line3);
    const char* s4 = safeText(line4);

    const bool bleStopped = statusContains(s1, s2, s3, s4, "BLE UNAVAILABLE") ||
                            statusContains(s1, s2, s3, s4, "SCAN STOP") ||
                            statusContains(s1, s2, s3, s4, "STOPPED") ||
                            statusContains(s1, s2, s3, s4, "ATTEMPTS EXHAUSTED") ||
                            statusContains(s1, s2, s3, s4, "CAMERA STANDBY") ||
                            statusContains(s1, s2, s3, s4, "AUTO WAKE") ||
                            statusContains(s1, s2, s3, s4, "COOLDOWN");

    const bool bleConnected = statusContains(s1, s2, s3, s4, "BLE_READY") ||
                              statusContains(s1, s2, s3, s4, "BLE LINK READY") ||
                              statusContains(s1, s2, s3, s4, "WIFI") ||
                              statusContains(s1, s2, s3, s4, "HTTP") ||
                              statusContains(s1, s2, s3, s4, "LIVEVIEW") ||
                              statusContains(s1, s2, s3, s4, "LIVE VIEW") ||
                              statusContains(s1, s2, s3, s4, "BUTTON A SHUTTER");

    const bool bleConnecting = statusContains(s1, s2, s3, s4, "CONNECTING") ||
                               statusContains(s1, s2, s3, s4, "CAMERA FOUND") ||
                               statusContains(s1, s2, s3, s4, "FAST CONNECT");

    const char* bleStatus = "BLE SEARCHING...";
    if (bleConnected) {
        bleStatus = "BLE CONNECTED";
    } else if (bleStopped) {
        bleStatus = "BLE PAUSED";
    } else if (bleConnecting) {
        bleStatus = "BLE CONNECTING...";
    }

    // Fixed rectangular viewfinder frame. The border intentionally stays the
    // same graphite color; only the GR IV status dot reflects BLE state.
    const int16_t x = 8;
    const int16_t y = 6;
    const int16_t w = _width - 16;
    const int16_t h = _height - 12;
    _canvas.fillRoundRect(x, y, w, h, 10, COLOR_CARD);
    _canvas.drawRoundRect(x, y, w, h, 10, COLOR_GRAPHITE);
    _canvas.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 8, COLOR_SLATE);

    // Abstract dark geometric surface inspired by the reference image, adapted
    // to the StickS3 rectangular screen instead of a round lens.
    _canvas.fillTriangle(x + 10, y + 16, x + 94, y + 16, x + 45, y + 106, COLOR_PANEL);
    _canvas.fillTriangle(x + 92, y + 18, x + 142, y + 68, x + 98, y + 112, COLOR_DARK);
    _canvas.fillTriangle(x + 120, y + 60, x + 205, y + 16, x + 202, y + 108, COLOR_PANEL);
    _canvas.fillTriangle(x + 118, y + 62, x + 166, y + 112, x + 210, y + 80, COLOR_SLATE);
    _canvas.fillTriangle(x + 142, y + 72, x + 184, y + 34, x + 216, y + 74, COLOR_DARK);
    _canvas.fillTriangle(x + 72, y + 24, x + 108, y + 110, x + 56, y + 112, COLOR_DARK);

    // White diagonal highlight band.
    const int16_t hx1 = x + 14;
    const int16_t hy1 = y + 28;
    const int16_t hx2 = x + 214;
    const int16_t hy2 = y + 86;
    const int16_t band = 12;
    _canvas.fillTriangle(hx1, hy1, hx2, hy2, hx2, hy2 + band, COLOR_HILITE);
    _canvas.fillTriangle(hx1, hy1, hx2, hy2 + band, hx1, hy1 + band, COLOR_HILITE);

    // Re-darken lower lens facets over the highlight for a layered aperture feel.
    _canvas.fillTriangle(x + 150, y + 78, x + 184, y + 34, x + 216, y + 80, COLOR_DARK);
    _canvas.fillTriangle(x + 146, y + 82, x + 210, y + 82, x + 134, y + 112, COLOR_DARK);

    _canvas.setTextSize(2);
    _canvas.setTextColor(COLOR_WHITE, COLOR_CARD);
    const char* title = "GR IV";
    const int16_t titleW = static_cast<int16_t>(strlen(title) * 12);
    const int16_t titleX = static_cast<int16_t>((_width - titleW) / 2 - 4);
    _canvas.setCursor(titleX, y + 14);
    _canvas.print(title);
    _canvas.fillCircle(titleX + titleW + 11, y + 21, 4, bleConnected ? COLOR_GREEN : COLOR_RED);
    drawDeviceBatteryStatus(x + w - 10, y + 17);

    _canvas.setTextSize(2);
    const int16_t statusW = static_cast<int16_t>(strlen(bleStatus) * 12);
    const int16_t statusX = static_cast<int16_t>((_width - statusW) / 2);
    _canvas.setTextColor(COLOR_WHITE, COLOR_CARD);
    _canvas.setCursor(statusX, y + h - 29);
    _canvas.print(bleStatus);
}

// StickS3 battery indicator for the BLE pairing/connection status screen.
void DisplayUi::drawDeviceBatteryStatus(int16_t rightX, int16_t y) {
    int32_t batteryLevel = M5.Power.getBatteryLevel();
    if (batteryLevel > 100) batteryLevel = 100;

    char batteryText[8];
    if (batteryLevel >= 0) {
        snprintf(batteryText, sizeof(batteryText), "%ld%%", static_cast<long>(batteryLevel));
    } else {
        snprintf(batteryText, sizeof(batteryText), "--%%");
    }

    constexpr int16_t iconWidth = 15;
    constexpr int16_t textGap = 4;
    const int16_t textWidth = static_cast<int16_t>(strlen(batteryText) * 6);
    const int16_t iconX = rightX - iconWidth - textGap - textWidth;

    drawBatteryIcon(iconX, y, batteryText);
    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_WHITE, COLOR_CARD);
    _canvas.setCursor(iconX + iconWidth + textGap, y);
    _canvas.print(batteryText);
}

// Graphic helper to draw WiFi RSSI strength bars
void DisplayUi::drawWifiIcon(int16_t x, int16_t y, int32_t rssi) {
    uint8_t bars = 0;
    if (rssi < 0) {
        if (rssi >= -60) bars = 4;
        else if (rssi >= -70) bars = 3;
        else if (rssi >= -80) bars = 2;
        else bars = 1;
    }

    for (uint8_t i = 0; i < 4; ++i) {
        uint16_t color = (i < bars) ? COLOR_GREEN : COLOR_SLATE;
        int16_t barH = 2 + (i * 2);
        _canvas.fillRect(x + (i * 3), y + (8 - barH), 2, barH, color);
    }
}

// Graphic helper to draw dynamic battery outline & fill level
void DisplayUi::drawBatteryIcon(int16_t x, int16_t y, const char* batteryStr) {
    int pct = -1;
    const char* text = safeText(batteryStr);
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (isDigit(text[i])) {
            if (pct < 0) pct = 0;
            pct = pct * 10 + (text[i] - '0');
        } else if (text[i] == '%' || text[i] == ' ') {
            break;
        }
    }

    _canvas.drawRect(x, y, 14, 8, COLOR_WHITE);
    _canvas.fillRect(x + 14, y + 2, 1, 4, COLOR_WHITE);

    if (pct >= 0) {
        if (pct > 100) pct = 100;
        int fillW = (pct * 10) / 100;
        if (fillW > 10) fillW = 10;

        uint16_t color = COLOR_GREEN;
        if (pct < 20) color = COLOR_RED;
        else if (pct < 50) color = COLOR_YELLOW;

        _canvas.fillRect(x + 2, y + 2, fillW, 4, color);
    } else {
        _canvas.drawLine(x + 2, y + 2, x + 11, y + 5, COLOR_GRAY);
    }
}
