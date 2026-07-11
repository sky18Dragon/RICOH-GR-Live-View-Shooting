#pragma once

#include <M5Unified.h>

#include "../../model/UiModel.h"

namespace rvf::ui {

class MinimalUiRenderer {
public:
    void renderBoot(LovyanGFX& canvas, const UiModel& model);
    void renderStatus(LovyanGFX& canvas, const UiModel& model);
    void renderSettings(LovyanGFX& canvas, const UiModel& model);
    void renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model);
    void renderError(LovyanGFX& canvas, const UiModel& model);
    void renderShutdown(LovyanGFX& canvas, const UiModel& model);

private:
    static void drawFocusBracket(LovyanGFX& canvas);
    static void drawWifiBars(LovyanGFX& canvas, int16_t x, int16_t y, int32_t rssi);
};

}  // namespace rvf::ui
