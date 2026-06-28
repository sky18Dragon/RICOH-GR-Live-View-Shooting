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

const char* safeText(const char* value, const char* fallback = "") {
    return value != nullptr ? value : fallback;
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

    // Large stylish GR logo in signature amber
    _canvas.setTextColor(COLOR_AMBER, COLOR_BG);
    _canvas.setTextSize(3);
    _canvas.setCursor(102, 20);
    _canvas.print("GR");

    // Subtitle
    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_WHITE, COLOR_BG);
    _canvas.setCursor(84, 52);
    _canvas.print("VIEWFINDER");

    // Glowing progress bar outline
    _canvas.drawRoundRect(40, 72, 160, 6, 3, COLOR_SLATE);
    _canvas.fillRoundRect(42, 74, 60, 2, 1, COLOR_AMBER);

    // Boot status message
    _canvas.setTextColor(COLOR_GRAY, COLOR_BG);
    const char* msg = safeText(message, "Booting...");
    int msgLen = strlen(msg);
    _canvas.setCursor((240 - (msgLen * 6)) / 2, 88);
    _canvas.print(msg);

    // Footer divider and hotkeys
    _canvas.drawFastHLine(20, _height - 24, 200, COLOR_SLATE);
    _canvas.setCursor(33, _height - 16);
    _canvas.print("G11: shutter   BtnB: liveview");

    pushCanvas();
}

void DisplayUi::showStatus(const char* line1, const char* line2, const char* line3, const char* line4) {
    clear(COLOR_BG);

    _canvas.drawFastHLine(10, 24, _width - 20, COLOR_SLATE);
    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_AMBER, COLOR_BG);
    _canvas.setCursor(10, 8);
    _canvas.print("RICOH GR VIEWFINDER");

    drawStatusLines(line1, line2, line3, line4);

    _canvas.drawFastHLine(10, _height - 20, _width - 20, COLOR_SLATE);
    _canvas.setTextColor(COLOR_GRAY, COLOR_BG);
    _canvas.setCursor(10, _height - 14);
    _canvas.print("SYSTEM STATUS OK");

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
    _canvas.print("Press BtnB to reconnect");

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
    drawBatteryIcon(14, _height - 12, battery);

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

// Redesigned structured card layout for status display
void DisplayUi::drawStatusLines(const char* line1, const char* line2, const char* line3, const char* line4) {
    // Left Card: NETWORK
    _canvas.drawRoundRect(10, 32, 105, 78, 4, COLOR_SLATE);
    _canvas.setTextColor(COLOR_AMBER, COLOR_BG);
    _canvas.setCursor(16, 38);
    _canvas.print("NETWORK");

    _canvas.setTextSize(1);
    _canvas.setTextColor(COLOR_WHITE, COLOR_BG);
    if (line1 != nullptr && line1[0] != '\0') {
        _canvas.setCursor(16, 52);
        String s1 = line1;
        _canvas.print(s1.substring(0, 15));
    }
    _canvas.setTextColor(COLOR_GRAY, COLOR_BG);
    if (line2 != nullptr && line2[0] != '\0') {
        _canvas.setCursor(16, 68);
        String s2 = line2;
        _canvas.print(s2.substring(0, 15));
    }

    // Right Card: CAMERA / DEVICE
    _canvas.drawRoundRect(125, 32, 105, 78, 4, COLOR_SLATE);
    _canvas.setTextColor(COLOR_AMBER, COLOR_BG);
    _canvas.setCursor(131, 38);
    _canvas.print("CAMERA");

    _canvas.setTextColor(COLOR_WHITE, COLOR_BG);
    if (line3 != nullptr && line3[0] != '\0') {
        _canvas.setCursor(131, 52);
        String s3 = line3;
        _canvas.print(s3.substring(0, 15));
    }
    _canvas.setTextColor(COLOR_GRAY, COLOR_BG);
    if (line4 != nullptr && line4[0] != '\0') {
        _canvas.setCursor(131, 68);
        String s4 = line4;
        _canvas.print(s4.substring(0, 15));

        // Auto draw battery icon if battery status exists
        if (s4.indexOf("%") >= 0 || s4.indexOf("BAT") >= 0 || s4.indexOf("charge") >= 0) {
            drawBatteryIcon(131, 84, s4);
        }
    }
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
void DisplayUi::drawBatteryIcon(int16_t x, int16_t y, const String& batteryStr) {
    int pct = -1;
    for (unsigned int i = 0; i < batteryStr.length(); ++i) {
        if (isDigit(batteryStr[i])) {
            if (pct < 0) pct = 0;
            pct = pct * 10 + (batteryStr[i] - '0');
        } else if (batteryStr[i] == '%' || batteryStr[i] == ' ') {
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