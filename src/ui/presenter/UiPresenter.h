#pragma once

#include "../model/UiModel.h"

namespace rvf::ui {

class UiPresenter {
public:
    UiModel present(const UiRuntimeSnapshot& snapshot) const;

private:
    static UiScreen mapScreen(const UiRuntimeSnapshot& snapshot);
    static UiPhase mapPhase(const UiRuntimeSnapshot& snapshot);
};

}  // namespace rvf::ui
