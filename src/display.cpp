#include "display.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_heap_caps.h>

#include "ui/UiTheme.h"

namespace {

const char* safeText(const char* value, const char* fallback = "") {
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

int parsePercent(const char* text) {
    if (text == nullptr) return -1;
    int value = -1;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] >= '0' && text[i] <= '9') {
            if (value < 0) value = 0;
            value = value * 10 + (text[i] - '0');
        } else if (value >= 0) {
            break;
        }
    }
    return value > 100 ? 100 : value;
}

uint16_t gray565(float intensity) {
    const uint8_t five = static_cast<uint8_t>(rvf::uiClamp01(intensity) * 31.0f + 0.5f);
    const uint8_t six = static_cast<uint8_t>(rvf::uiClamp01(intensity) * 63.0f + 0.5f);
    return static_cast<uint16_t>((five << 11) | (six << 5) | five);
}

}  // namespace

bool DisplayUi::begin() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_imu = true;
    cfg.internal_spk = rvf::UiTheme::kSoundEnabled;
    M5.begin(cfg);

    _canvas.setColorDepth(16);
    _canvasUsePsram = psramFound();
    _canvas.setPsram(_canvasUsePsram);
    Serial.printf("UI Canvas: storage=%s free_int=%lu largest_int=%lu free_psram=%lu largest_psram=%lu\n",
                  _canvasUsePsram ? "psram" : "internal",
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
    _backlight.begin(rvf::UiTheme::kActiveBrightness);
    M5.Display.setBrightness(rvf::UiTheme::kActiveBrightness);
    if (!createCanvasFor(rvf::UiOrientation::Portrait)) return false;
    clear(rvf::UiTheme::kBlack);
    pushCanvas();
    return true;
}

bool DisplayUi::createCanvasFor(rvf::UiOrientation orientation) {
    const uint8_t primaryRotation = orientation == rvf::UiOrientation::Portrait ? 0 : 1;
    const uint8_t alternateRotation = orientation == rvf::UiOrientation::Portrait ? 2 : 3;
    M5.Display.setRotation(primaryRotation);
    bool shapeMatches = orientation == rvf::UiOrientation::Portrait
                            ? M5.Display.width() < M5.Display.height()
                            : M5.Display.width() > M5.Display.height();
    if (!shapeMatches) M5.Display.setRotation(alternateRotation);

    const int16_t width = M5.Display.width();
    const int16_t height = M5.Display.height();
    void* sprite = _canvas.createSprite(width, height);
    if (sprite == nullptr) return false;

    _width = width;
    _height = height;
    _orientation = orientation;
    _canvasReady = true;
    _canvas.setTextSize(1);
    _canvas.setTextWrap(false);
    _sceneDrawn = false;
    return true;
}

bool DisplayUi::setOrientation(rvf::UiOrientation orientation) {
    if (_canvasReady && orientation == _orientation) return true;
    if (_frameWriteActive) return false;

    const uint32_t nowMs = millis();
    if (_canvasReady && _orientationFailurePending && orientation == _failedOrientation &&
        static_cast<int32_t>(nowMs - _orientationRetryAfterMs) < 0) {
        return false;
    }

    const rvf::UiOrientation previous = _orientation;
    const bool hadCanvas = _canvasReady;
    Serial.printf("UI Canvas: switch %s -> %s heap=%lu psram=%lu\n",
                  previous == rvf::UiOrientation::Portrait ? "portrait" : "landscape",
                  orientation == rvf::UiOrientation::Portrait ? "portrait" : "landscape",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));

    if (_canvasReady) {
        _canvas.deleteSprite();
        _canvasReady = false;
    }
    if (!createCanvasFor(orientation)) {
        _failedOrientation = orientation;
        _orientationRetryAfterMs = nowMs + rvf::UiTheme::kCanvasAllocationRetryMs;
        _orientationFailurePending = true;
        Serial.printf("UI Canvas: create failed; restoring previous orientation; retry_in=%lums "
                      "largest_int=%lu largest_psram=%lu\n",
                      static_cast<unsigned long>(rvf::UiTheme::kCanvasAllocationRetryMs),
                      static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                      static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
        _canvas.deleteSprite();
        _canvasReady = false;
        if (hadCanvas && createCanvasFor(previous)) {
            clear(rvf::UiTheme::kBlack);
            pushCanvas();
        }
        return false;
    }

    _orientationFailurePending = false;
    clear(rvf::UiTheme::kBlack);
    pushCanvas();
    Serial.printf("UI Canvas: ready %dx%d heap=%lu psram=%lu\n",
                  _width,
                  _height,
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    return true;
}

void DisplayUi::pushCanvas() {
    if (_canvasReady) _canvas.pushSprite(&M5.Display, 0, 0);
}

void DisplayUi::clear(uint16_t color) {
    if (_canvasReady) _canvas.fillScreen(color);
}

uint32_t DisplayUi::renderIntervalMs(rvf::UiScene scene) const {
    if (scene == rvf::UiScene::CameraSleep) return 1000U / rvf::UiTheme::kSleepAnimationFps;
    if (scene == rvf::UiScene::Error) return 1000U;
    return 1000U / rvf::UiTheme::kRemoteAnimationFps;
}

void DisplayUi::updateBacklight(const rvf::UiViewModel& view) {
    const bool sleeping = view.scene == rvf::UiScene::CameraSleep;
    _backlight.setTarget(sleeping ? rvf::UiTheme::kSleepBrightness : rvf::UiTheme::kActiveBrightness,
                         sleeping ? rvf::UiTheme::kSleepDimDurationMs
                                  : rvf::UiTheme::kWakeBrightenDurationMs,
                         view.nowMs);
    if (_backlight.update(view.nowMs)) M5.Display.setBrightness(_backlight.value());
}

void DisplayUi::render(const rvf::UiViewModel& view) {
    if (!setOrientation(view.orientation) || !_canvasReady) return;
    updateBacklight(view);

    const bool sceneChanged = !_sceneDrawn || view.scene != _lastScene;
    if (view.scene == rvf::UiScene::LivePreview) {
        _lastScene = view.scene;
        _sceneDrawn = true;
        return;
    }
    if (!sceneChanged && rvf::uiElapsedMs(view.nowMs, _lastRenderAtMs) < renderIntervalMs(view.scene)) {
        return;
    }

    _lastRenderAtMs = view.nowMs;
    _lastScene = view.scene;
    _sceneDrawn = true;
    clear(rvf::UiTheme::kBlack);
    switch (view.scene) {
        case rvf::UiScene::Boot:
            drawBoot(view);
            break;
        case rvf::UiScene::Pairing:
        case rvf::UiScene::Connecting:
            drawConnecting(view);
            break;
        case rvf::UiScene::RemoteReady:
            drawRemote(view);
            break;
        case rvf::UiScene::ResetPairing:
            drawReset(view);
            break;
        case rvf::UiScene::CameraSleep:
            drawSleep(view);
            break;
        case rvf::UiScene::Disconnected:
            drawDisconnected(view);
            break;
        case rvf::UiScene::Error:
            drawError(view);
            break;
        case rvf::UiScene::LivePreview:
            break;
    }
    if (view.scene != rvf::UiScene::LivePreview) drawShutterOverlay(view);
    pushCanvas();
}

LovyanGFX* DisplayUi::beginLiveFrame() {
    if (!_canvasReady || _frameWriteActive || _orientation != rvf::UiOrientation::Landscape) return nullptr;
    _frameWriteActive = true;
    return &_canvas;
}

void DisplayUi::finishLiveFrame(bool push) {
    if (!_frameWriteActive) return;
    if (push) pushCanvas();
    _frameWriteActive = false;
}

void DisplayUi::renderLiveFrameOverlay(const rvf::UiViewModel& view) {
    if (!_canvasReady || !_frameWriteActive) return;
    drawBatteryIndicator(view);
    drawShutterOverlay(view);
}

void DisplayUi::drawBoot(const rvf::UiViewModel& view) {
    const int16_t radius = static_cast<int16_t>(4 + 2 * view.sceneProgress);
    _canvas.fillCircle(_width / 2, _height / 2, radius, rvf::UiTheme::kWhite);
}

void DisplayUi::drawConnecting(const rvf::UiViewModel& view) {
    const float progress = rvf::uiSmoothStep(view.sceneProgress);
    const int16_t centerX = _width / 2;
    const int16_t centerY = _height / 2;
    const int16_t leftStart = _width * 24 / 100;
    const int16_t rightStart = _width - leftStart;
    const int16_t leftX = static_cast<int16_t>(leftStart + (centerX - leftStart) * progress);
    const int16_t rightX = static_cast<int16_t>(rightStart + (centerX - rightStart) * progress);
    const int16_t dotRadius = rvf::UiTheme::kConnectDotDiameter / 2;
    if (progress < 0.94f) {
        _canvas.fillCircle(leftX, centerY, dotRadius, rvf::UiTheme::kGreen);
        _canvas.fillCircle(rightX, centerY, dotRadius, rvf::UiTheme::kGreen);
    } else {
        const float merge = (progress - 0.94f) / 0.06f;
        const int16_t mergedRadius = static_cast<int16_t>(dotRadius +
            (rvf::UiTheme::kConnectMergedDiameter / 2 - dotRadius) * rvf::uiClamp01(merge));
        _canvas.fillCircle(centerX, centerY, mergedRadius, rvf::UiTheme::kGreen);
    }
}

void DisplayUi::drawRemote(const rvf::UiViewModel& view) {
    const int16_t centerX = _width / 2;
    const int16_t centerY = _height / 2;
    const float scale = 1.0f - (1.0f - rvf::UiTheme::kRemoteFocusScalePct / 100.0f) *
                                     view.focusProgress;
    const int16_t radius = static_cast<int16_t>(rvf::UiTheme::kRemoteApertureDiameter * scale / 2.0f);
    const uint16_t ringColor = view.focusProgress > 0.0f ? rvf::UiTheme::kGreen : rvf::UiTheme::kWhite;
    if (view.focusActive && view.focusProgress <= 0.0f) {
        _canvas.drawCircle(centerX, centerY, radius + 3, rvf::UiTheme::kDarkGray);
    }
    _canvas.drawCircle(centerX, centerY, radius, ringColor);
    _canvas.drawCircle(centerX, centerY, radius - 1, ringColor);
    _canvas.fillCircle(centerX,
                       centerY,
                       rvf::UiTheme::kRemoteCoreDiameter / 2,
                       rvf::UiTheme::kWhite);
    drawBatteryIndicator(view);
}

void DisplayUi::drawReset(const rvf::UiViewModel& view) {
    const int16_t centerX = _width / 2;
    const int16_t centerY = _height / 2;
    if (view.resetSplitActive) {
        const float progress = rvf::uiEaseOut(view.resetSplitProgress);
        const int16_t offset = static_cast<int16_t>(rvf::UiTheme::kResetSplitDistance * progress);
        _canvas.fillCircle(centerX - offset, centerY, 10, rvf::UiTheme::kRed);
        _canvas.fillCircle(centerX + offset, centerY, 10, rvf::UiTheme::kRed);
    } else {
        _canvas.fillCircle(centerX,
                           centerY,
                           rvf::UiTheme::kResetCircleDiameter / 2,
                           rvf::UiTheme::kGreen);
    }

    const int16_t barWidth = _width * rvf::UiTheme::kResetProgressWidthPct / 100;
    const int16_t barX = (_width - barWidth) / 2;
    const int16_t barY = _height - rvf::UiTheme::kResetProgressBottom -
                         rvf::UiTheme::kResetProgressHeight;
    _canvas.fillRect(barX, barY, barWidth, rvf::UiTheme::kResetProgressHeight,
                     rvf::UiTheme::kDarkGray);
    _canvas.fillRect(barX,
                     barY,
                     static_cast<int16_t>(barWidth * rvf::uiClamp01(view.resetProgress)),
                     rvf::UiTheme::kResetProgressHeight,
                     rvf::UiTheme::kRed);
}

void DisplayUi::drawSleep(const rvf::UiViewModel& view) {
    const int16_t drift = static_cast<int16_t>(12.0f * view.sleepProgress);
    _canvas.setTextSize(2);
    _canvas.setTextColor(rvf::UiTheme::kSleepGray, rvf::UiTheme::kBlack);
    _canvas.setCursor(_width / 2 - 12, _height / 2 - 12 + drift);
    _canvas.print("Z");
    _canvas.setTextSize(1);
    _canvas.setCursor(_width / 2 + 5, _height / 2 + drift);
    _canvas.print("z");
}

void DisplayUi::drawDisconnected(const rvf::UiViewModel& view) {
    const int16_t centerX = _width / 2;
    const int16_t centerY = _height / 2;
    const int16_t offset = static_cast<int16_t>(rvf::UiTheme::kResetSplitDistance * view.sceneProgress);
    _canvas.fillCircle(centerX - offset, centerY, 8, rvf::UiTheme::kGray);
    _canvas.fillCircle(centerX + offset, centerY, 8, rvf::UiTheme::kGray);
}

void DisplayUi::drawCenteredText(const char* text, int16_t y, uint16_t color) {
    char clipped[41];
    const size_t maxChars = _width < _height ? 20 : 36;
    snprintf(clipped, sizeof(clipped), "%.*s", static_cast<int>(maxChars), safeText(text));
    _canvas.setTextSize(1);
    _canvas.setTextColor(color, rvf::UiTheme::kBlack);
    const int16_t x = std::max<int16_t>(0, (_width - static_cast<int16_t>(strlen(clipped) * 6)) / 2);
    _canvas.setCursor(x, y);
    _canvas.print(clipped);
}

void DisplayUi::drawError(const rvf::UiViewModel& view) {
    _canvas.fillCircle(_width / 2, _height / 2 - 24, 8, rvf::UiTheme::kRed);
    drawCenteredText(safeText(view.errorTitle, "Camera unavailable"), _height / 2, rvf::UiTheme::kWhite);
    drawCenteredText(safeText(view.errorDetail, "Press A to retry"), _height / 2 + 14, rvf::UiTheme::kGray);
}

void DisplayUi::drawBatteryIndicator(const rvf::UiViewModel& view) {
    int percent = parsePercent(view.cameraBattery);
    if (percent < 0) percent = view.deviceBatteryPercent;
    const int16_t x = _width - 22;
    const int16_t y = 7;
    _canvas.drawRect(x, y, 14, 8, rvf::UiTheme::kWhite);
    _canvas.fillRect(x + 14, y + 2, 2, 4, rvf::UiTheme::kWhite);
    if (percent >= 0) {
        const int16_t fill = std::max<int16_t>(1, std::min<int16_t>(10, percent / 10));
        const uint16_t color = percent < 20 ? rvf::UiTheme::kRed : rvf::UiTheme::kWhite;
        _canvas.fillRect(x + 2, y + 2, fill, 4, color);
    }
}

void DisplayUi::drawShutterOverlay(const rvf::UiViewModel& view) {
    if (!view.shutterOverlayActive) return;
    if (_orientation == rvf::UiOrientation::Portrait) {
        const float intensity = 1.0f - rvf::uiEaseOut(view.overlayProgress);
        _canvas.fillScreen(view.shutterFailed ?
                               static_cast<uint16_t>((static_cast<uint16_t>(31 * intensity) << 11)) :
                               gray565(intensity));
        return;
    }

    const float triangular = view.overlayProgress <= 0.5f
                                 ? view.overlayProgress * 2.0f
                                 : (1.0f - view.overlayProgress) * 2.0f;
    const int16_t thickness = std::max<int16_t>(1,
        static_cast<int16_t>(rvf::UiTheme::kShutterFrameWidth * rvf::uiEaseOut(triangular)));
    const uint16_t color = view.shutterFailed ? rvf::UiTheme::kRed : rvf::UiTheme::kWhite;
    _canvas.fillRect(0, 0, _width, thickness, color);
    _canvas.fillRect(0, _height - thickness, _width, thickness, color);
    _canvas.fillRect(0, 0, thickness, _height, color);
    _canvas.fillRect(_width - thickness, 0, thickness, _height, color);
}

void DisplayUi::showBoot(const char*) {
    rvf::UiViewModel view;
    view.scene = rvf::UiScene::Boot;
    view.orientation = rvf::UiOrientation::Portrait;
    view.nowMs = millis();
    view.sceneProgress = 0.5f;
    _sceneDrawn = false;
    render(view);
}

void DisplayUi::showStatus(const char*, const char*, const char*, const char*) {
    rvf::UiViewModel view;
    view.scene = rvf::UiScene::Pairing;
    view.orientation = rvf::UiOrientation::Portrait;
    view.nowMs = millis();
    view.sceneProgress = 0.25f;
    _sceneDrawn = false;
    render(view);
}

void DisplayUi::showStatus(const String& line1, const String& line2, const String& line3, const String& line4) {
    showStatus(line1.c_str(), line2.c_str(), line3.c_str(), line4.c_str());
}

void DisplayUi::showError(const char* message, const char* detail) {
    rvf::UiViewModel view;
    view.scene = rvf::UiScene::Error;
    view.orientation = rvf::UiOrientation::Portrait;
    view.nowMs = millis();
    view.errorTitle = message;
    view.errorDetail = detail;
    _sceneDrawn = false;
    render(view);
}

void DisplayUi::showError(const String& message, const String& detail) {
    showError(message.c_str(), detail.c_str());
}

void DisplayUi::showPasskeyEntry(const uint8_t digits[6], uint8_t activeIndex) {
    if (!setOrientation(rvf::UiOrientation::Portrait) || !_canvasReady) return;
    clear(rvf::UiTheme::kBlack);

    drawCenteredText("PAIRING PASSKEY", 26, rvf::UiTheme::kGreen);
    drawCenteredText("Match camera code", 44, rvf::UiTheme::kGray);

    const int16_t cellW = 24;
    const int16_t cellH = 34;
    const int16_t gap = 5;
    const int16_t totalW = 6 * cellW + 5 * gap;
    const int16_t startX = (_width - totalW) / 2;
    const int16_t y = _height / 2 - cellH / 2;

    _canvas.setTextSize(2);
    for (uint8_t i = 0; i < 6; ++i) {
        const int16_t x = startX + i * (cellW + gap);
        const bool active = i == activeIndex;
        const bool confirmed = i < activeIndex;
        const uint16_t frame = active ? rvf::UiTheme::kGreen : rvf::UiTheme::kDarkGray;
        const uint16_t text = active ? rvf::UiTheme::kGreen :
                              (confirmed ? rvf::UiTheme::kWhite : rvf::UiTheme::kGray);
        _canvas.drawRoundRect(x, y, cellW, cellH, 4, frame);
        _canvas.setTextColor(text, rvf::UiTheme::kBlack);
        _canvas.setCursor(x + 7, y + 9);
        _canvas.print(static_cast<char>('0' + (digits[i] % 10)));
    }

    _canvas.setTextSize(1);
    drawCenteredText(activeIndex >= 6 ? "Submitting..." : "A:+1  Hold:Next",
                     _height - 28,
                     activeIndex >= 6 ? rvf::UiTheme::kGreen : rvf::UiTheme::kWhite);
    pushCanvas();
}

void DisplayUi::drawOverlay(const String&,
                            const String&,
                            const String&,
                            const String& battery,
                            float fps,
                            int32_t,
                            uint32_t frames,
                            uint32_t droppedFrames) {
    rvf::UiViewModel view;
    view.cameraBattery = battery.c_str();
    view.deviceBatteryPercent = static_cast<int8_t>(M5.Power.getBatteryLevel());
    drawBatteryIndicator(view);
    if (rvf::UiTheme::kDebugHud) {
        char text[32];
        snprintf(text, sizeof(text), "%.1f %lu/%lu", static_cast<double>(fps),
                 static_cast<unsigned long>(frames),
                 static_cast<unsigned long>(droppedFrames));
        _canvas.setTextSize(1);
        _canvas.setTextColor(rvf::UiTheme::kWhite);
        _canvas.setCursor(5, 5);
        _canvas.print(text);
    }
}
