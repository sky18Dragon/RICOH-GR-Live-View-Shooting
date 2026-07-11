#include "DebugUiRenderer.h"

#include <cstdio>

#include "DebugUiProfile.h"

namespace rvf::ui {

namespace {

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kCyan = 0x07FF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kYellow = 0xFFE0;

const char* yesNo(bool value) {
    return value ? "Y" : "N";
}

const char* safeText(const char* value, const char* fallback = "") {
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

}  // namespace

void DebugUiRenderer::renderBoot(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(kBlack);
    canvas.setTextSize(1);
    canvas.setTextColor(kCyan, kBlack);
    canvas.setCursor(4, 4);
    canvas.print("DEBUG UI / BOOT");
    canvas.setTextColor(kWhite, kBlack);
    canvas.setCursor(4, 24);
    canvas.print(safeText(model.detail, "booting"));
}

void DebugUiRenderer::renderStatus(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(kBlack);
    canvas.setTextSize(1);
    canvas.setTextColor(kCyan, kBlack);
    canvas.setCursor(4, 4);
    canvas.print("DEBUG UI / STATUS");
    drawModel(canvas, model, 20);
}

void DebugUiRenderer::renderSettings(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(kBlack);
    canvas.setTextSize(1);
    canvas.setTextColor(kCyan, kBlack);
    canvas.setCursor(4, 4);
    canvas.print("DEBUG UI / SETTINGS");
    canvas.setTextColor(kWhite, kBlack);
    canvas.setCursor(4, 24);
    canvas.print("STATIC QUICK CONTROLS");
    canvas.setCursor(4, 42);
    canvas.print("SHUTTER=1/125 FILTER=STD");
    canvas.setCursor(4, 58);
    canvas.print("EXPOSURE=+/-0 WIFI=ON");
    canvas.setCursor(4, 82);
    canvas.print(uiPhaseName(model.phase));
}

void DebugUiRenderer::renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model) {
    char line[64];
    canvas.setTextSize(1);
    canvas.setTextColor(kYellow);
    canvas.setCursor(2, 2);
    canvas.print(uiPhaseName(model.phase));
    if (model.shutterStatus == UiShutterStatus::Succeeded) {
        canvas.print(" SHOT_OK");
    } else if (model.shutterStatus == UiShutterStatus::Failed) {
        canvas.print(" SHOT_FAIL");
    }
    if constexpr (DebugUiProfile::kShowFps) {
        std::snprintf(line, sizeof(line), " %.1ffps", static_cast<double>(model.fps));
        canvas.print(line);
    }
    if constexpr (DebugUiProfile::kShowFrameStats) {
        std::snprintf(line,
                      sizeof(line),
                      " f=%lu d=%lu",
                      static_cast<unsigned long>(model.renderedFrames),
                      static_cast<unsigned long>(model.droppedFrames));
        canvas.print(line);
    }
    if constexpr (DebugUiProfile::kShowFocusBracket) {
        const int16_t cx = canvas.width() / 2;
        const int16_t cy = canvas.height() / 2;
        canvas.drawRect(cx - 12, cy - 8, 24, 16, kGreen);
    }
    canvas.setTextColor(kWhite);
    canvas.setCursor(2, canvas.height() - 12);
    std::snprintf(line,
                  sizeof(line),
                  "B:%s W:%s P:%s",
                  yesNo(model.bleConnected),
                  yesNo(model.wifiConnected),
                  yesNo(model.previewRunning));
    canvas.print(line);
    if constexpr (DebugUiProfile::kShowWifiRssi) {
        std::snprintf(line, sizeof(line), " R:%ld", static_cast<long>(model.wifiRssi));
        canvas.print(line);
    }
    if constexpr (DebugUiProfile::kShowBattery) {
        canvas.print(" ");
        canvas.print(safeText(model.battery, "--"));
    }
    if constexpr (DebugUiProfile::kShowCameraModel) {
        canvas.setCursor(2, canvas.height() - 24);
        canvas.print(safeText(model.cameraModel, safeText(model.cameraName, "RICOH GR")));
    }
}

void DebugUiRenderer::renderError(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(kBlack);
    canvas.setTextSize(1);
    canvas.setTextColor(kRed, kBlack);
    canvas.setCursor(4, 4);
    canvas.print("DEBUG UI / ERROR");
    canvas.setCursor(4, 22);
    canvas.print(safeText(model.errorMessage, "Unknown error"));
    drawModel(canvas, model, 40);
}

void DebugUiRenderer::renderShutdown(LovyanGFX& canvas, const UiModel& model) {
    canvas.fillScreen(kBlack);
    canvas.setTextSize(1);
    canvas.setTextColor(kYellow, kBlack);
    canvas.setCursor(4, 4);
    canvas.print("DEBUG UI / SHUTDOWN");
    canvas.setCursor(4, 24);
    canvas.print(safeText(model.detail, "powering off"));
}

void DebugUiRenderer::drawModel(LovyanGFX& canvas, const UiModel& model, int16_t y) {
    char line[64];
    canvas.setTextColor(kWhite, kBlack);
    canvas.setCursor(4, y);
    canvas.print(uiPhaseName(model.phase));
    canvas.setCursor(4, y + 14);
    std::snprintf(line,
                  sizeof(line),
                  "BLE:%s WIFI:%s PREVIEW:%s SHUT:%s",
                  yesNo(model.bleConnected),
                  yesNo(model.wifiConnected),
                  yesNo(model.previewRunning),
                  yesNo(model.shutterReady));
    canvas.print(line);
    canvas.setCursor(4, y + 28);
    if constexpr (DebugUiProfile::kShowWifiRssi && DebugUiProfile::kShowFps) {
        std::snprintf(line,
                      sizeof(line),
                      "RSSI:%ld FPS:%.1f",
                      static_cast<long>(model.wifiRssi),
                      static_cast<double>(model.fps));
        canvas.print(line);
    } else if constexpr (DebugUiProfile::kShowWifiRssi) {
        std::snprintf(line, sizeof(line), "RSSI:%ld", static_cast<long>(model.wifiRssi));
        canvas.print(line);
    } else if constexpr (DebugUiProfile::kShowFps) {
        std::snprintf(line, sizeof(line), "FPS:%.1f", static_cast<double>(model.fps));
        canvas.print(line);
    } else {
        canvas.print("METRICS HIDDEN");
    }
    canvas.setCursor(4, y + 42);
    if constexpr (DebugUiProfile::kShowCameraModel) {
        canvas.print(safeText(model.cameraModel, safeText(model.cameraName, "NO CAMERA")));
    } else {
        canvas.print(safeText(model.cameraName, "RICOH CAMERA"));
    }
    canvas.setCursor(4, y + 56);
    canvas.print(safeText(model.detail));
    canvas.setTextColor(model.bleConnected ? kGreen : kRed, kBlack);
    canvas.fillCircle(canvas.width() - 8, 8, 3, model.bleConnected ? kGreen : kRed);
}

}  // namespace rvf::ui
