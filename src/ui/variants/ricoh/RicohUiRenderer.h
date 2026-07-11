#pragma once

#include <M5Unified.h>

#include "../../model/UiModel.h"

namespace rvf::ui {

class RicohUiRenderer {
public:
    void renderBoot(LovyanGFX& canvas, const UiModel& model);
    void renderStatus(LovyanGFX& canvas, const UiModel& model);
    void renderSettings(LovyanGFX& canvas, const UiModel& model);
    void renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model);
    void renderError(LovyanGFX& canvas, const UiModel& model);
    void renderShutdown(LovyanGFX& canvas, const UiModel& model);

private:
    static void drawGeometricCard(LovyanGFX& canvas);
    static void drawWifiIcon(LovyanGFX& canvas, int16_t x, int16_t y, int32_t rssi);
    static void drawBatteryIcon(LovyanGFX& canvas, int16_t x, int16_t y, const char* battery);
    static void drawFocusBracket(LovyanGFX& canvas);
};

}  // namespace rvf::ui
