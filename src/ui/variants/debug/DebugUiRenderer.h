#pragma once

#include <M5Unified.h>

#include "../../model/UiModel.h"

namespace rvf::ui {

class DebugUiRenderer {
public:
    void renderBoot(LovyanGFX& canvas, const UiModel& model);
    void renderStatus(LovyanGFX& canvas, const UiModel& model);
    void renderLiveViewOverlay(LovyanGFX& canvas, const UiModel& model);
    void renderError(LovyanGFX& canvas, const UiModel& model);
    void renderShutdown(LovyanGFX& canvas, const UiModel& model);

private:
    static void drawModel(LovyanGFX& canvas, const UiModel& model, int16_t y);
};

}  // namespace rvf::ui
