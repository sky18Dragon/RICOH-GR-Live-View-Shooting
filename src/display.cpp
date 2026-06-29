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
    _canvas.print("BtnA: shutter / wake");

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
    _canvas.print("Press BtnA to reconnect");

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

void DisplayUi::drawStatusLines(const char* line1, const char* line2, const char* line3, const char* line4) {
    String s1 = safeText(line1);
    String s2 = safeText(line2);
    String s3 = safeText(line3);
    String s4 = safeText(line4);

    String all = s1 + " " + s2 + " " + s3 + " " + s4;
    all.reserve(all.length() + 16);
    String upper = all;
    upper.toUpperCase();

    enum StepState { STEP_PENDING, STEP_RUNNING, STEP_OK, STEP_ERROR };

    StepState bleState = STEP_PENDING;
    StepState wifiState = STEP_PENDING;
    StepState httpState = STEP_PENDING;
    StepState liveState = STEP_PENDING;

    bool hasError = upper.indexOf("FAILED") >= 0 ||
                    upper.indexOf("ERROR") >= 0 ||
                    upper.indexOf("UNAVAILABLE") >= 0 ||
                    upper.indexOf("NOT FOUND") >= 0 ||
                    upper.indexOf("LINK LOST") >= 0 ||
                    upper.indexOf("CONNECTION_LOST") >= 0 ||
                    upper.indexOf("NOT READY") >= 0 ||
                    upper.indexOf("NO_SSID") >= 0;

    bool isStandby = upper.indexOf("STANDBY") >= 0 ||
                     upper.indexOf("COOLDOWN") >= 0 ||
                     upper.indexOf("AUTO WAKE") >= 0;
    bool isRecovery = upper.indexOf("CAMERA RECOVERY") >= 0;
    bool isManualWake = upper.indexOf("MANUAL WAKE") >= 0;
    bool isBleReset = upper.indexOf("BLE STACK RESET") >= 0;
    bool isButtonShutter = upper.indexOf("BUTTON A") >= 0;

    bool mentionsBle = upper.indexOf("BLE") >= 0 ||
                       upper.indexOf("PAIRING") >= 0 ||
                       upper.indexOf("SCANNING") >= 0;
    bool mentionsWifi = upper.indexOf("WIFI") >= 0 ||
                        upper.indexOf("CONNECTED") >= 0 ||
                        upper.indexOf("IDLE") >= 0 ||
                        upper.indexOf("DISCONNECTED") >= 0 ||
                        upper.indexOf("CONNECT_FAILED") >= 0;
    bool mentionsHttp = upper.indexOf("HTTP") >= 0 ||
                        upper.indexOf("PROBE") >= 0;
    bool mentionsLive = upper.indexOf("LIVEVIEW") >= 0 ||
                        upper.indexOf("LIVE VIEW") >= 0 ||
                        upper.indexOf("STARTING LIVEVIEW") >= 0;

    String topStatus = "BOOT";

    if (isStandby) {
        topStatus = "STANDBY";
        bleState = STEP_OK;
    } else if (isRecovery) {
        topStatus = "RECOVERY";
        if (upper.indexOf("BLE_READY") >= 0) {
            bleState = STEP_OK;
            wifiState = STEP_RUNNING;
        } else {
            bleState = STEP_RUNNING;
        }
    } else if (isManualWake) {
        topStatus = "WAKE";
        bleState = STEP_RUNNING;
    } else if (isBleReset) {
        topStatus = "RESET";
        bleState = hasError ? STEP_ERROR : STEP_RUNNING;
    } else if (isButtonShutter) {
        topStatus = "SHOT";
        if (upper.indexOf("BLE NOT READY") >= 0 || hasError) {
            bleState = STEP_ERROR;
        } else if (upper.indexOf("BLE SHOT OK") >= 0) {
            bleState = STEP_OK;
        } else {
            bleState = STEP_RUNNING;
        }
    } else if (mentionsLive) {
        topStatus = "LIVE";
        bleState = STEP_OK;
        wifiState = STEP_OK;
        httpState = STEP_OK;
        liveState = hasError ? STEP_ERROR : STEP_RUNNING;
    } else if (mentionsHttp) {
        topStatus = "HTTP";
        bleState = STEP_OK;
        wifiState = STEP_OK;
        httpState = hasError ? STEP_ERROR : STEP_RUNNING;
        if (upper.indexOf("HTTP PROBE OK") >= 0) {
            httpState = STEP_OK;
            liveState = STEP_RUNNING;
        }
    } else if (mentionsWifi) {
        topStatus = "WIFI";
        bleState = STEP_OK;
        wifiState = hasError ? STEP_ERROR : STEP_RUNNING;
        if (upper.indexOf("WIFI CONNECTED") >= 0 ||
            upper.indexOf("WIFI SENT") >= 0 ||
            upper.indexOf("WIFI PARAMS") >= 0) {
            wifiState = STEP_OK;
            httpState = STEP_RUNNING;
        } else if (upper.indexOf("BLE WIFI FAILED") >= 0) {
            wifiState = STEP_ERROR;
        }
    } else if (mentionsBle) {
        topStatus = "BLE";
        bleState = hasError ? STEP_ERROR : STEP_RUNNING;
        if (upper.indexOf("BLE_READY") >= 0 ||
            upper.indexOf("BLE LINK READY") >= 0 ||
            upper.indexOf("BLE CAMERA FOUND") >= 0) {
            bleState = STEP_OK;
            wifiState = STEP_RUNNING;
        } else if (upper.indexOf("BLE NOT READY") >= 0 ||
                   upper.indexOf("BLE NOT FOUND") >= 0 ||
                   upper.indexOf("BLE CONNECT FAILED") >= 0 ||
                   upper.indexOf("BLE UNAVAILABLE") >= 0) {
            bleState = STEP_ERROR;
        }
    } else {
        bleState = STEP_RUNNING;
    }

    auto stateColor = [](StepState st) -> uint16_t {
        switch (st) {
            case STEP_OK:      return COLOR_GREEN;
            case STEP_RUNNING: return COLOR_AMBER;
            case STEP_ERROR:   return COLOR_RED;
            default:           return COLOR_GRAY;
        }
    };

    auto stateText = [](StepState st) -> const char* {
        switch (st) {
            case STEP_OK:      return "OK";
            case STEP_RUNNING: return "RUN";
            case STEP_ERROR:   return "ERR";
            default:           return "--";
        }
    };

    int16_t labelW = topStatus.length() * 6;
    _canvas.setTextColor(topStatus == "STANDBY" ? COLOR_YELLOW : COLOR_GREEN, COLOR_BG);
    _canvas.setCursor(_width - 10 - labelW, 8);
    _canvas.print(topStatus);

    auto drawStep = [&](int index, const char* name, const String& hint, StepState state) {
        const int16_t y = 37 + index * 18;
        const int16_t dotX = 18;
        const int16_t dotY = y + 7;
        const uint16_t color = stateColor(state);

        if (index < 3) {
            _canvas.drawFastVLine(dotX, dotY + 5, 9, COLOR_GRAY);
        }

        if (state == STEP_PENDING) {
            _canvas.drawCircle(dotX, dotY, 4, COLOR_GRAY);
        } else {
            _canvas.fillCircle(dotX, dotY, 4, color);
            if (state == STEP_OK) {
                _canvas.drawLine(dotX - 2, dotY + 1, dotX, dotY + 3, COLOR_BG);
                _canvas.drawLine(dotX, dotY + 3, dotX + 3, dotY - 1, COLOR_BG);
            } else if (state == STEP_RUNNING) {
                _canvas.fillRect(dotX - 1, dotY - 1, 2, 2, COLOR_BG);
            } else if (state == STEP_ERROR) {
                _canvas.drawLine(dotX - 2, dotY - 2, dotX + 2, dotY + 2, COLOR_BG);
                _canvas.drawLine(dotX + 2, dotY - 2, dotX - 2, dotY + 2, COLOR_BG);
            }
        }

        _canvas.setTextSize(1);
        _canvas.setTextColor(state == STEP_PENDING ? COLOR_GRAY : COLOR_WHITE, COLOR_BG);
        _canvas.setCursor(30, y + 1);
        _canvas.print(name);

        _canvas.setTextColor(COLOR_GRAY, COLOR_BG);
        _canvas.setCursor(70, y + 1);
        String h = hint.length() ? hint : String("--");
        _canvas.print(h.substring(0, 16));

        _canvas.drawRoundRect(176, y, 44, 14, 3, color);
        _canvas.setTextColor(color, COLOR_BG);
        const char* txt = stateText(state);
        _canvas.setCursor(176 + (44 - static_cast<int16_t>(strlen(txt)) * 6) / 2, y + 3);
        _canvas.print(txt);
    };

    String hintBle = s1.length() ? s1 : s2;
    String hintWifi = s2.length() ? s2 : s1;
    String hintHttp = s3.length() ? s3 : s1;
    String hintLive = s4.length() ? s4 : (s3.length() ? s3 : s1);

    drawStep(0, "BLE",  hintBle,  bleState);
    drawStep(1, "WIFI", hintWifi, wifiState);
    drawStep(2, "HTTP", hintHttp, httpState);
    drawStep(3, "LIVE", hintLive, liveState);

    _canvas.drawFastHLine(10, _height - 24, _width - 20, COLOR_SLATE);

    _canvas.setTextColor(COLOR_WHITE, COLOR_BG);
    _canvas.setCursor(10, _height - 16);
    if (s3.length() > 0) {
        _canvas.print(s3.substring(0, 15));
    } else {
        _canvas.print("RICOH GR");
    }

    bool showBattery = (s4.indexOf('%') >= 0 ||
                        s4.indexOf("BAT") >= 0 ||
                        s4.indexOf("CHARGE") >= 0);
    if (showBattery && s4.length() > 0) {
        drawBatteryIcon(113, _height - 18, s4);
    } else if (s4.length() > 0) {
        drawBatteryIcon(113, _height - 18, String());
    }

    _canvas.setTextColor(COLOR_GRAY, COLOR_BG);
    _canvas.setCursor(138, _height - 16);
    _canvas.print("BtnA Shoot/Wake");
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